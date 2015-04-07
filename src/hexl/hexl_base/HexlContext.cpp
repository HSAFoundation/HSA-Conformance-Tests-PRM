#include "HexlContext.hpp"
#include "Options.hpp"
#include "HexlResource.hpp"
#include "HexlTestFactory.hpp"
#include "RuntimeCommon.hpp"
#include "Stats.hpp"
#include "Options.hpp"
#include "hsail_c.h"
#ifdef _WIN32
#include <windows.h>
#include <strsafe.h>
#endif // _WIN32

namespace hexl {

  void Context::Print(std::ostream& out) const
  {
    for (auto i = map.begin(); i != map.end(); ++i) {
      out << i->first << ":" << std::endl;
      {
        IndentStream indent(out);
        i->second->Print(out);
        out << std::endl;
      }
    }
    if (parent) { parent->Print(out); }
  }

  void Context::Dump() const
  {
    std::string outputPath = RM()->GetOutputDirName(GetOutputPath());
    for (auto i = map.begin(); i != map.end(); ++i) {
      i->second->Dump(outputPath, i->first);
    }
  }

  void Context::Move(const std::string& key, Values& values)
  {
    Values* newvalues = new Values();
    newvalues->swap(values);
    PutObject(key, new ContextManagedPointer<Values>(newvalues));
  }

  bool Context::IsVerbose(const std::string& what, bool enabledWithPlainVerboseOption) const
  {
    return (enabledWithPlainVerboseOption && Opts()->IsSet("verbose")) || Opts()->IsSet("hexl.verbose.all") || Opts()->IsSet("hexl.verbose." + what);
  }

  bool Context::IsDumpEnabled(const std::string& what, bool enableWithPlainDumpOption) const
  {
    return (enableWithPlainDumpOption && Opts()->IsSet("dump")) || Opts()->IsSet("dump." + what);
  }

  std::string Context::GetOutputName(const std::string& name, const std::string& what)
  {
    std::string fullName = GetString("hexl.outputPath");
    if (!fullName.empty()) { fullName += "/"; }
    fullName += name;
    return fullName + "." + what;
  }

  bool Context::DumpTextIfEnabled(const std::string& name, const std::string& what, const std::string& text)
  {
    if (IsDumpEnabled(name)) {
      std::string outname = GetOutputName(name, what);
      if (!SaveTextResource(RM(), outname, text)) {
        Info() << "Warning: failed to dump " << what << " to " << outname << std::endl;
        return false;
      } else {
        return true;
      }
    } else {
      return false;
    }
  }

  bool Context::DumpBinaryIfEnabled(const std::string& name, const std::string& what, const void *buffer, size_t bufferSize)
  {
    if (IsDumpEnabled(name)) {
      std::string outname = GetOutputName(name, what);
      if (!SaveBinaryResource(RM(), outname, buffer, bufferSize)) {
        Info() << "Warning: failed to dump " << what << " to " << outname<< std::endl;
        return false;
      } else {
        return true;
      }
    } else {
      return false;
    }
  }

  void Context::DumpBrigIfEnabled(const std::string& name, void* _brig)
  {
    brig_container_t brig = reinterpret_cast<brig_container_t>(_brig);

    if (IsDumpEnabled("brig")) {
      std::string testName = GetOutputName(name, "brig");
      std::string brigFileName = RM()->GetOutputFileName(testName);
      if (0 != brig_container_save_to_file(brig, brigFileName.c_str())) {
        Info() << "Warning: failed to dump brig to " << brigFileName << ": " << brig_container_get_error_text(brig) << std::endl;
      }
    }
    if (IsDumpEnabled("hsail")) {
      std::string testName = GetOutputName(name, "hsail");
      std::string hsailFileName = RM()->GetOutputFileName(testName);
      if (0 != brig_container_disassemble_to_file(brig, hsailFileName.c_str())) {
        Info() << "Warning: failed to dump hsail to " << hsailFileName << ": " << brig_container_get_error_text(brig) << std::endl;
      }
    }
  }

#ifdef _WIN32
  void Context::Win32Error(const std::string& msg)
  {
    // Retrieve the system error message for the last-error code
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    if (!msg.empty()) {
      Error() << msg << ": ";
    }
    Error() << "error " << dw << ": " << (LPTSTR) lpMsgBuf;
    LocalFree(lpMsgBuf);
  }

#endif // _WIN32

  Value Context::GetRuntimeValue(Value v)
  {
    switch (v.Type()) {
    case MV_EXPR: return GetValue(v.S());
    case MV_STRING: return GetValue(v.Str());
    default: return v;
    }
  }

  static const unsigned MAX_SHOWN_FAILURES = 16;

  bool ValidateMemory(Context* context, const Values& expected, const void *actualPtr, const std::string& method)
  {
    assert(expected.size() > 0);
    ValueType vtype = expected[0].Type();
    std::unique_ptr<Comparison> comparison(NewComparison(method, vtype));
    assert(comparison.get());
    comparison->Reset(vtype);
    unsigned maxShownFailures = context->Opts()->GetUnsigned("hexl.max_shown_failures", MAX_SHOWN_FAILURES);
    bool verboseData = context->IsVerbose("data");
    unsigned shownFailures = 0;
    Value actualValue;
    const char *aptr = (const char *) actualPtr;
    for (unsigned i = 0; i < expected.size(); ++i) {
      Value expectedValue = context->GetRuntimeValue(expected[i]);
      actualValue.ReadFrom(aptr, expectedValue.Type()); aptr += actualValue.Size();
      bool passed = comparison->Compare(expectedValue, actualValue);
      if ((!passed && comparison->GetFailed() < maxShownFailures) || verboseData) {
        context->Info() << "  " << "[" << std::setw(2) << i << "]" << ": ";
        comparison->PrintLong(context->Info());
        context->Info() << std::endl;
        if (!passed) { shownFailures++; }
      }
    }
    if (comparison->GetFailed() > shownFailures) {
      context->Info() << "  ... (" << (comparison->GetFailed() - shownFailures) << " more failures not shown)" << std::endl;
    }
    context->Info() << "  ";
    if (comparison->IsFailed()) {
      context->Info() << "Error: failed " << comparison->GetFailed() << " / " << comparison->GetChecks() << " comparisons, "
        << "max " << comparison->GetMethodDescription() << " error " << comparison->GetMaxError() << " at " <<
        "[" << std::setw(2) << comparison->GetMaxErrorIndex() << "]" << "." << std::endl;
    } else {
      context->Info() << "Successful " << comparison->GetChecks() << " comparisons." << std::endl;
    }
    return !comparison->IsFailed();
  }

}
