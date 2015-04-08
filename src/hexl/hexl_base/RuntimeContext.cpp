/*
   Copyright 2014 Heterogeneous System Architecture (HSA) Foundation

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "RuntimeContext.hpp"
#include "RuntimeCommon.hpp"
#include "Options.hpp"
#include "HSAILItems.h"
#include <stdlib.h>
#include <sstream>

namespace hexl {

  void *alignedMalloc(size_t size, size_t align)
  {
    assert((align & (align - 1)) == 0);
#if defined(_WIN32) || defined(_WIN64)
    return _aligned_malloc(size, align);
#else
    void *ptr;
    int res = posix_memalign(&ptr, align, size);
    assert(res == 0);
    return ptr;
#endif // _WIN32 || _WIN64
  }

  void alignedFree(void *ptr)
  {
#if defined(_WIN32) || defined(_WIN64)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
  }

  void ImageParams::Print(std::ostream& out) const
  {
    out <<
      HSAIL_ASM::type2str(imageType) << "(" <<
      HSAIL_ASM::anyEnum2str(geometry) << ", " <<
      HSAIL_ASM::anyEnum2str(channelOrder) << ", " <<
      HSAIL_ASM::anyEnum2str(channelType) << ", " <<
      width << ", " <<
      height << ", " <<
      depth << ", " <<
      arraySize <<
      ")";
  }

  void SamplerParams::Print(std::ostream& out) const
  {
    out <<
      "sampler" << "(" <<
      HSAIL_ASM::anyEnum2str(addressing) << ", " <<
      HSAIL_ASM::anyEnum2str(coord) << ", " <<
      HSAIL_ASM::anyEnum2str(filter) << ")";
  }

  void ImageRegion::Print(std::ostream& out) const 
  {
    out << "image_region" <<
           "(x = " << x <<
           "; y = " << y <<
           "; z = " << z <<
           "; size_x = " << size_x <<
           "; size_y = " << size_y <<
           "; size_z = " << size_z << ")";
  }


  class NoneRuntimeState : public runtime::RuntimeState {
  private:
    Context* context;

  public:
    explicit NoneRuntimeState(Context* context_)
      : context(context_) { }

    Context* GetContext() { return context; }

    void Set(const std::string& key, Value value) { context->Put(key, value); context->Info() << "set " << key << " " << value << std::endl; }

    Value Get(const std::string& key) { return context->GetValue(key); }

    bool StartThread(unsigned id, runtime::Command* commandToRun) { return true; }
    bool WaitThreads() { return true; }

    bool ModuleCreateFromBrig(const std::string& moduleId = "module", const std::string& brigId = "brig") { return true; }

    bool ProgramCreate(const std::string& programId = "program") { return true; }
    bool ProgramAddModule(const std::string& programId = "program", const std::string& moduleId = "module") { return true; }
    bool ProgramFinalize(const std::string& codeId = "code", const std::string& programId = "program") { return true; }

    bool ExecutableCreate(const std::string& executableId = "executable") { return true; }
    bool ExecutableLoadCode(const std::string& executableId = "executable", const std::string& codeId = "code") { return true; }
    bool ExecutableFreeze(const std::string& executableId = "executable") { return true; }

    bool BufferCreate(const std::string& bufferId, size_t size, const std::string& initValuesId = "") { return true; }
    bool BufferValidate(const std::string& bufferId, const std::string& expectedValuesId, const std::string& method = "") { return true; }

    bool ImageCreate(const std::string& imageId, const std::string& imageParamsId) { return true; }
    bool ImageInitialize(const std::string& imageId, const std::string& imageParamsId, const std::string& initValueId) { return true; }
    bool ImageWrite(const std::string& imageId, const std::string& writeValuesId, const ImageRegion& region) { return true; }
    bool ImageValidate(const std::string& imageId, const std::string& expectedValuesId, const std::string& method = "") { return true; }
    bool SamplerCreate(const std::string& samplerId, const std::string& samplerParamsId) override { return true;  }

    bool DispatchCreate(const std::string& dispatchId = "dispatch", const std::string& executableId = "executable", const std::string& kernelName = "") { return true; }
    //bool DispatchSet(const std::string& dispatchId, const std::string& key, const std::string& value) { return true; }
    bool DispatchArg(const std::string& dispatchId, runtime::DispatchArgType argType, const std::string& argKey) { return true; }

    //bool DispatchArg(const std::string& dispatchId, const std::string& valuesId) { return true; }
    bool DispatchExecute(const std::string& dispatchId = "dispatch") { return true; }

    bool SignalCreate(const std::string& signalId, uint64_t signalInitialValue = 1) { return true; }
    bool SignalSend(const std::string& signalId, uint64_t signalSendValue = 1) { return true; }
    bool SignalWait(const std::string& signalId, uint64_t signalExpectedValue = 1) { return true; }

    bool QueueCreate(const std::string& queueId, uint32_t size = 0) { return true; }
  };

    class NoneRuntime : public runtime::RuntimeContext {
    public:
      explicit NoneRuntime(Context* context)
        : RuntimeContext(context) { }

      bool Init() override { return true; }
      runtime::RuntimeState* NewState(Context* context) override { return new NoneRuntimeState(context); }
      std::string Description() const override { return "No runtime"; }
      bool IsFullProfile() override { return true; }
      uint32_t Wavesize() override { return 64; }
      uint32_t WavesPerGroup() override { return 4; }
      bool IsLittleEndianness() override { return true; }
      bool IsBreakSupported() override { return false; }
      bool IsDetectSupported() override { return false; }
    };

    runtime::RuntimeContext* CreateNoneRuntime(Context* context)
    {
      return new NoneRuntime(context);
    }

  namespace runtime {

    bool RuntimeState::DispatchValueArg(const std::string& dispatchId, Value value)
    {
      std::ostringstream ss;
      ss << dispatchId << ".arg." << (argNum++);
      std::string argKey(ss.str());
      GetContext()->Put(argKey, value);
      return DispatchArg(dispatchId, DARG_VALUE, argKey);
    }

    bool RuntimeState::DispatchValuesArg(const std::string& dispatchId, Values* values)
    {
      std::ostringstream ss;
      ss << dispatchId << ".arg." << (argNum++);
      std::string argKey(ss.str());
      GetContext()->Move(argKey, values);
      return DispatchArg(dispatchId, DARG_VALUES, argKey);
    }

    bool RuntimeState::DispatchGroupOffsetArg(const std::string& dispatchId, Value value) 
    {
      std::ostringstream ss;
      ss << dispatchId << ".arg." << (argNum++);
      std::string argKey(ss.str());
      GetContext()->Put(argKey, value);
      return DispatchArg(dispatchId, DARG_GROUPOFFSET, argKey);
    }

    void RuntimeState::Set(const std::string& key, Value value)
    {
      GetContext()->Put(key, value);
    }

    void RuntimeState::Set(const std::string& parent, const std::string& key, Value value)
    {
      Set(parent + "." + key, value);
    }

    Value RuntimeState::Get(const std::string& key)
    {
      return GetContext()->GetValue(key);
    }

    Value RuntimeState::Get(const std::string& parent, const std::string& key)
    {
      return Get(parent + "." + key);
    }

    void HostThreads::RunThread(unsigned id, Command* command)
    {
      results[id] = command->Execute(rt);
    }

    bool HostThreads::StartThread(unsigned id, Command* command)
    {
      results.push_back(false);
      threads.push_back(std::thread(&HostThreads::RunThread, this, id, command));
      return true;
    }

    bool HostThreads::Result(unsigned id) const
    {
      return results[id];
    }

    bool HostThreads::WaitThreads()
    {
      for (std::thread& t : threads) {
        t.join();
      }
      bool result = true;
      for (bool r : results) { result &= r; }
      return result;
    }

  }
}
