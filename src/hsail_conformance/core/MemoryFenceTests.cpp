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

#include "MemoryFenceTests.hpp"
#include "BrigEmitter.hpp"
#include "BasicHexlTests.hpp"
#include "HCTests.hpp"
#include "Brig.h"
#include <sstream>

namespace hsail_conformance {

using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

class MemoryFenceTest : public Test {
public:
  MemoryFenceTest(Grid geometry_, BrigType type_, BrigMemoryOrder memoryOrder1_, BrigMemoryOrder memoryOrder2_, BrigSegment segment_, BrigMemoryScope memoryScope_)
  : Test(KERNEL, geometry_), type(type_), memoryOrder1(memoryOrder1_), memoryOrder2(memoryOrder2_), segment(segment_), memoryScope(memoryScope_),
    initialValue(0) {}

  void Name(std::ostream& out) const {
    out << segment2str(segment) << "_" << type2str(type) << "/"
        << opcode2str(BRIG_OPCODE_ST) << "_" << opcode2str(BRIG_OPCODE_MEMFENCE) << "_" << memoryOrder2str(memoryOrder1) << "_" << memoryScope2str(memoryScope) << "__"
        << opcode2str(BRIG_OPCODE_LD) << "_" << opcode2str(BRIG_OPCODE_MEMFENCE) << "_" << memoryOrder2str(memoryOrder2) << "_" << memoryScope2str(memoryScope)
        << "/" << geometry;
  }

protected:
  BrigType type;
  BrigMemoryOrder memoryOrder1;
  BrigMemoryOrder memoryOrder2;
  BrigSegment segment;
  BrigMemoryScope memoryScope;
  int64_t initialValue;
  DirectiveVariable globalVar;
  OperandAddress globalVarAddr;
  Buffer input;
  TypedReg inputReg;

  BrigType ResultType() const { return type; }

  bool IsValid() const {
    if (BRIG_SEGMENT_GROUP == segment && geometry->WorkgroupSize() != geometry->GridSize())
      return false;
    if (BRIG_MEMORY_ORDER_SC_ACQUIRE == memoryOrder1)
      return false;
    if (BRIG_MEMORY_ORDER_SC_RELEASE == memoryOrder2)
      return false;
    return true;
  }

  Value GetInputValueForWI(uint64_t wi) const {
    return Value(Brig2ValueType(type), 7);
  }

  Value ExpectedResult(uint64_t i) const {
    return Value(Brig2ValueType(type), 7);
  }

  void Init() {
    Test::Init();
    input = kernel->NewBuffer("input", HOST_INPUT_BUFFER, Brig2ValueType(type), geometry->GridSize());
    for (uint64_t i = 0; i < uint64_t(geometry->GridSize()); ++i) {
      input->AddData(GetInputValueForWI(i));
    }
  }

  virtual void ModuleVariables() {
    std::string globalVarName = "global_var";
    switch (segment) {
      case BRIG_SEGMENT_GROUP: globalVarName = "group_var"; break;
      default: break;
    }
    globalVar = be.EmitVariableDefinition(globalVarName, segment, type);
    if (segment != BRIG_SEGMENT_GROUP)
      globalVar.init() = be.Immed(type, initialValue);
  }

  virtual void EmitInstrToTest(BrigOpcode opcode, TypedReg reg) {
    globalVarAddr = be.Address(globalVar);
    switch (opcode) {
      case BRIG_OPCODE_LD:
        be.EmitLoad(segment, type, reg->Reg(), globalVarAddr);
        break;
      case BRIG_OPCODE_ST:
        be.EmitStore(segment, type, reg->Reg(), globalVarAddr);
        break;
      default: assert(false); break;
    }
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    inputReg = be.AddTReg(type);
    input->EmitLoadData(inputReg);
    TypedReg wiID = be.EmitWorkitemFlatAbsId(true);
    TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
    SRef s_label_skip_store = "@skip_store";
    SRef s_label_skip_memfence = "@skip_memfence";
    be.EmitCmp(cReg->Reg(), wiID, be.Immed(wiID->Type(), 1), BRIG_COMPARE_NE);
    be.EmitCbr(cReg, s_label_skip_store);

    EmitInstrToTest(BRIG_OPCODE_ST, inputReg);
    be.EmitMemfence(memoryOrder1, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitBr(s_label_skip_memfence);
    be.EmitLabel(s_label_skip_store);

    be.EmitMemfence(memoryOrder2, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitLabel(s_label_skip_memfence);

    EmitInstrToTest(BRIG_OPCODE_LD, result);
    return result;
  }
};

/*
class MemoryFenceCompoundTest : public MemoryFenceTest {
protected:
  BrigType type2;
  BrigSegment segment2;
  Buffer input2;
  DirectiveVariable globalVar2;
  OperandAddress globalVarAddr2;

public:
  MemoryFenceCompoundTest(Grid geometry_, BrigType type_, BrigType type2_, BrigMemoryOrder memoryOrder1_, BrigMemoryOrder memoryOrder2_, BrigSegment segment_, BrigSegment segment2_, BrigMemoryScope memoryScope_)
  : MemoryFenceTest(geometry_, type_, memoryOrder1_, memoryOrder2_, segment_, memoryScope_), type2(type2_), segment2(segment2_) {}

  void Name(std::ostream& out) const {
    out << segment2str(segment) << "_" << type2str(type) << "__" << segment2str(segment2) << "_" << type2str(type2) << "/"
        << opcode2str(BRIG_OPCODE_ST) << "_" << opcode2str(BRIG_OPCODE_MEMFENCE) << "_" << memoryOrder2str(memoryOrder1) << "_" << memoryScope2str(memoryScope) << "__"
        << opcode2str(BRIG_OPCODE_LD) << "_" << opcode2str(BRIG_OPCODE_MEMFENCE) << "_" << memoryOrder2str(memoryOrder2) << "_" << memoryScope2str(memoryScope)
        << "/" << geometry;
  }

  BrigType ResultType() const { return type; }

  bool IsValid() const {
    if ((BRIG_SEGMENT_GROUP == segment || BRIG_SEGMENT_GROUP == segment2) && geometry->WorkgroupSize() != geometry->GridSize())
      return false;
    if (BRIG_MEMORY_ORDER_SC_ACQUIRE == memoryOrder1)
      return false;
    if (BRIG_MEMORY_ORDER_SC_RELEASE == memoryOrder2)
      return false;
    return true;
  }

  Value GetInputValue2ForWI(uint64_t wi) const {
    return Value(Brig2ValueType(type), 3);
  }

  Value ExpectedResult(uint64_t i) const {
    return Value(Brig2ValueType(type), 10);
  }

  void Init() {
    MemoryFenceTest::Init();
    input2 = kernel->NewBuffer("input2", HOST_INPUT_BUFFER, Brig2ValueType(type2), geometry->GridSize());
    for (uint64_t i = 0; i < uint64_t(geometry->GridSize()); ++i) {
      input2->AddData(GetInputValue2ForWI(i));
    }
  }

  void ModuleVariables() {
    MemoryFenceTest::ModuleVariables();
    std::string globalVarName = "global_var_2";
    switch (segment2) {
      case BRIG_SEGMENT_GROUP: globalVarName = "group_var_2"; break;
      default: break;
    }
    globalVar2 = be.EmitVariableDefinition(globalVarName, segment2, type2);
    if (segment2 != BRIG_SEGMENT_GROUP)
      globalVar2.init() = be.Immed(type2, initialValue);
  }

  void EmitInstrToTest(BrigOpcode opcode) {
    MemoryFenceTest::EmitInstrToTest(opcode);
    globalVarAddr2 = be.Address(globalVar2);
    switch (opcode) {
      case BRIG_OPCODE_LD:
        be.EmitLoad(segment2, type2, destReg->Reg(), globalVarAddr2);
        break;
      case BRIG_OPCODE_ST:
        be.EmitStore(segment2, type2, inputReg->Reg(), globalVarAddr2);
        break;
      default: assert(false); break;
    }
  }

};
*/

void MemoryFenceTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<MemoryFenceTest>(ap, it, "memfence/basic", cc->Grids().MemfenceSet(), cc->Types().Memfence(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
//  TestForEach<MemoryFenceCompoundTest>(ap, it, "memfence/compound", cc->Grids().MemfenceSet(), cc->Types().Memfence(), cc->Types().Memfence(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
}

}
