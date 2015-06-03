#include "HexlObjects.hpp"
#include "Grid.hpp"
#include "MObject.hpp"
#include "RuntimeContext.hpp"
#include <fstream>
#include <iomanip>
#include "HSAILTool.h"

namespace hexl {

  template <>
  void Print(const GridGeometry& grid, std::ostream& out) { grid.Print(out); }

  template <>
  void Print(const HSAIL_ASM::BrigContainer& o, std::ostream& out) { out << "<brig>"; }

  template <>
  void Print(const Value& value, std::ostream& out) { value.Print(out); }

  template <>
  void Print(const Values& values, std::ostream& out) { out << "<" << values.size() << " values>"; }

  template <>
  void Print(const ImageParams& imageParams, std::ostream& out) { imageParams.Print(out); }

  template <>
  void Print(const SamplerParams& samplerParams, std::ostream& out) { samplerParams.Print(out); }

  std::string GetOutputName(const std::string& path, const std::string& name, const std::string& ext)
  {
    if (ext.empty()) {
      return path + "/" + name;
    } else {
      return path + "/" + name + "." + ext;
    }
  }

  template <>
  void Dump<HSAIL_ASM::BrigContainer>(const HSAIL_ASM::BrigContainer& brig, const std::string& path, const std::string& name)
  {
    std::string brigFileName = GetOutputName(path, name, "brig");
    HSAIL_ASM::BrigContainer* brigC = const_cast<HSAIL_ASM::BrigContainer *>(&brig); 
    HSAIL_ASM::Tool tool(brigC);
    if (0 != tool.saveToFile(brigFileName)) {
      //Info() << "Warning: failed to dump brig to " << brigFileName << ": " << tool->output() << std::endl;
    }
    std::string hsailFileName = GetOutputName(path, name, "hsail");
    if (0 != tool.disassembleToFile(hsailFileName)) {
      //Info() << "Warning: failed to dump hsail to " << hsailFileName << ": " << tool->output() << std::endl;
    }
  }

  template <>
  void Dump<Values>(const Values& values, const std::string& path, const std::string& name)
  {
    std::string fname = GetOutputName(path, name, "");
    std::ofstream out(fname);
    for (unsigned i = 0; i < values.size(); ++i) {
      Value value = values[i];
      value.SetPrintExtraHex(true);
      out << "[" << i << "]" << ": " << std::setw(value.PrintWidth()) << value;
      out << std::endl;
    }
  }
}
