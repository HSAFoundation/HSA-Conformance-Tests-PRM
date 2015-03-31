#include "HexlObjects.hpp"
#include "Grid.hpp"
#include "MObject.hpp"
#include "RuntimeContext.hpp"
#include <fstream>
#include <iomanip>

namespace hexl {

  template <>
  void Print(const GridGeometry& grid, std::ostream& out) { grid.Print(out); }

  template <>
  void Print(const brig_container_t& o, std::ostream& out) { out << "<brig>"; }

  template <>
  void Print(const brig_container_struct& o, std::ostream& out) { out << "<brig>";  }

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
  void Dump<brig_container_struct>(const brig_container_struct& brig, const std::string& path, const std::string& name)
  {
    brig_container_t brigt = const_cast<brig_container_t>(&brig);
    std::string brigFileName = GetOutputName(path, name, "brig");
    if (0 != brig_container_save_to_file(brigt, brigFileName.c_str())) {
      //Info() << "Warning: failed to dump brig to " << brigFileName << ": " << brig_container_get_error_text(brig) << std::endl;
    }
    std::string hsailFileName = GetOutputName(path, name, "hsail");
    if (0 != brig_container_disassemble_to_file(brigt, hsailFileName.c_str())) {
      //Info() << "Warning: failed to dump hsail to " << hsailFileName << ": " << brig_container_get_error_text(brig) << std::endl;
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
