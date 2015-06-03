#ifndef HEXL_OBJECTS_HPP
#define HEXL_OBJECTS_HPP

#include <string>
#include <ostream>
#include "HSAILBrigContainer.h"
#include "MObject.hpp"

struct BrigModuleHeader;

namespace hexl {

  class ResourceManager;
  class TestFactory;
  namespace runtime { class RuntimeContext; class RuntimeState; }
  class Options;
  class AllStats;
  class GridGeometry;
  class Value;
  class ImageParams;
  class SamplerParams;

  template <typename T>
  void Print(const T& t, std::ostream& out);

  template <>
  inline void Print<std::string>(const std::string& s, std::ostream& out) {
    out << s;
  }

  template <>
  inline void Print<std::ostream>(const std::ostream& o, std::ostream& out) {
    out << "<ostream>";
  }

  template <>
  void Print(const HSAIL_ASM::BrigContainer& o, std::ostream& out);

  template <>
  inline void Print(const BrigModuleHeader& o, std::ostream& out) { }

  template <>
  void Print(const Value& value, std::ostream& out);

  template <>
  void Print(const Values& values, std::ostream& out);

  template <>
  inline void Print<ResourceManager>(const ResourceManager& rm, std::ostream& out) { }

  template <>
  inline void Print<TestFactory>(const TestFactory& tf, std::ostream& out) { }

  template <>
  inline void Print<Options>(const Options& tf, std::ostream& out) { }

  template <>
  inline void Print<AllStats>(const AllStats& tf, std::ostream& out) { }

  template <>
  inline void Print<runtime::RuntimeContext>(const runtime::RuntimeContext& tf, std::ostream& out) { }

  template <>
  void Print(const GridGeometry& grid, std::ostream& out);

  template <>
  void Print(const ImageParams& imageParams, std::ostream& out);

  template <>
  void Print(const SamplerParams& samplerParams, std::ostream& out);

  template <typename T>
  inline void Dump(const T& t, const std::string& path, const std::string& name) { }

  template <>
  void Dump<HSAIL_ASM::BrigContainer>(const HSAIL_ASM::BrigContainer&, const std::string& path, const std::string& name);

  template <>
  void Dump<Values>(const Values&, const std::string& path, const std::string& name);

}

#endif // HEXL_OBJECTS_HPP
