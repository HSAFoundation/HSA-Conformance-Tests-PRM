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

#include "InitializerTests.hpp"
#include "BrigEmitter.hpp"
#include "BasicHexlTests.hpp"
#include "HCTests.hpp"
#include <sstream>

using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class InitializerTest : public Test {
private:
  Variable var;
  BrigTypeX type;
  BrigSegment segment;
  uint64_t dim;
  Values data;
  bool isConst;

  uint64_t DataSize() const { return dim == 0 ? 1 : dim; }
  ValueType ValueType() const { return Brig2ValueType(type); }

public:
  InitializerTest(Grid geometry, BrigTypeX type_, BrigSegment segment_, uint64_t dim_, bool isConst_)
    : Test(Location::KERNEL, geometry), 
      type(type_), segment(segment_), dim(dim_), data(), isConst(isConst_)
  {
  }

  ~InitializerTest() {
    delete geometry;
  }

  bool IsValid() const override {
    return (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_READONLY) &&
           data.size() < DataSize();
  }

  void Init() override {
    Test::Init();
    var = kernel->NewVariable("var", segment, type, Location::MODULE, BRIG_ALIGNMENT_NONE, dim, isConst);
    for (uint32_t i = 0; i < DataSize(); ++i) {
      auto val = ValueForType(ValueType());
      data.push_back(val);
      switch (type) {
      case BRIG_TYPE_S8:  var->PushBack(val.Data().s8);  break;
      case BRIG_TYPE_U8:  var->PushBack(val.Data().u8);  break;
      case BRIG_TYPE_S16: var->PushBack(val.Data().s16); break;
      case BRIG_TYPE_U16: var->PushBack(val.Data().u16); break;
      case BRIG_TYPE_S32: var->PushBack(val.Data().s32); break;
      case BRIG_TYPE_U32: var->PushBack(val.Data().u32); break;
      case BRIG_TYPE_S64: var->PushBack(val.Data().s64); break;
      case BRIG_TYPE_U64: var->PushBack(val.Data().u64); break;
      case BRIG_TYPE_F16: var->PushBack(val.Data().f);   break;
      case BRIG_TYPE_F32: var->PushBack(val.Data().f);   break;
      case BRIG_TYPE_F64: var->PushBack(val.Data().d);   break;
      default: assert(false);
      }
    }
  }

  void Name(std::ostream& out) const override {
    if (isConst) {
      out << "const_";
    }
    out << segment2str(segment) << "_" << typeX2str(type);
    if (dim != 0) { 
      out << "[" << dim << "]"; 
    }
  }

  BrigTypeX ResultType() const override { return type; }

  void ExpectedResults(Values* result) const override {
    for (auto val: data) {
      result->push_back(val);
    }
  }

  TypedReg Result() override {
    // work-item flat id
    auto wiId = be.EmitWorkitemFlatId();

    // calculate offset from work-item flat id
    auto addrType = getSegAddrSize(segment, te->CoreCfg()->IsLarge()) == 32 ? BRIG_TYPE_U32 : BRIG_TYPE_U64;
    auto cvt = be.AddTReg(addrType);
    if (addrType != wiId->Type()) {
      be.EmitCvt(cvt, wiId);
    } else {
      be.EmitMov(cvt, wiId);
    }
    auto offset = be.AddTReg(addrType);
    be.EmitArith(BRIG_OPCODE_MUL, offset, cvt, be.Immed(addrType, getBrigTypeNumBytes(type)));  

    // load from variable at offset
    auto reg = be.AddTReg(type);
    be.EmitLoad(segment, reg, be.Address(var->Variable(), offset->Reg(), 0)); 

    return reg;
  }

  void ModuleVariables() override {
    Test::ModuleVariables();
    var->ModuleVariables();
  }
};


void InitializerTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  TestForEach<InitializerTest>(cc->Ap(), it, "initializer", cc->Grids().DefaultGeometrySet(), cc->Types().Compound(), cc->Segments().InitializableSegments(), cc->Variables().InitializerDims(), Bools::All());
}

}
