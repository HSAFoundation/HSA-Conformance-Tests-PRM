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
    initialValue(0), s_label_skip_store("@skip_store"), s_label_skip_memfence("@skip_memfence") {}

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
  std::string s_label_skip_store;
  std::string s_label_skip_memfence;
  DirectiveVariable globalVar;
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

  virtual void EmitInstrToTest(BrigOpcode opcode, BrigSegment seg, BrigType t, TypedReg reg, DirectiveVariable var) {
    OperandAddress globalVarAddr = be.Address(var);
    switch (opcode) {
      case BRIG_OPCODE_LD:
        be.EmitLoad(seg, t, reg->Reg(), globalVarAddr);
        break;
      case BRIG_OPCODE_ST:
        be.EmitStore(seg, t, reg->Reg(), globalVarAddr);
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
    be.EmitCmp(cReg->Reg(), wiID, be.Immed(wiID->Type(), 1), BRIG_COMPARE_NE);
    be.EmitCbr(cReg, s_label_skip_store);

    EmitInstrToTest(BRIG_OPCODE_ST, segment, type, inputReg, globalVar);
    be.EmitMemfence(memoryOrder1, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitBr(s_label_skip_memfence);
    be.EmitLabel(s_label_skip_store);

    be.EmitMemfence(memoryOrder2, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitLabel(s_label_skip_memfence);

    EmitInstrToTest(BRIG_OPCODE_LD, segment, type, result, globalVar);
    return result;
  }
};


class MemoryFenceCompoundTest : public MemoryFenceTest {
protected:
  BrigType type2;
  BrigSegment segment2;
  Buffer input2;
  TypedReg inputReg2;
  DirectiveVariable globalVar2;

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

  BrigType ResultType2() const { return type2; }

  bool IsValid() const {
    if (type != type2)
      return false;
    //if (isFloatType(type) || isFloatType(type2))
    //  return false;
    if (16 == getBrigTypeNumBits(type) || 16 == getBrigTypeNumBits(type2))
      return false;
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

  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    TypedReg result2 = be.AddTReg(ResultType2());
    inputReg = be.AddTReg(type);
    inputReg2 = be.AddTReg(type2);
    input->EmitLoadData(inputReg);
    input2->EmitLoadData(inputReg2);
    TypedReg wiID = be.EmitWorkitemFlatAbsId(true);
    TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
    be.EmitCmp(cReg->Reg(), wiID, be.Immed(wiID->Type(), 1), BRIG_COMPARE_NE);
    be.EmitCbr(cReg, s_label_skip_store);

    EmitInstrToTest(BRIG_OPCODE_ST, segment, type, inputReg, globalVar);
    EmitInstrToTest(BRIG_OPCODE_ST, segment2, type2, inputReg2, globalVar2);
    be.EmitMemfence(memoryOrder1, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitBr(s_label_skip_memfence);
    be.EmitLabel(s_label_skip_store);

    be.EmitMemfence(memoryOrder2, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitLabel(s_label_skip_memfence);

    EmitInstrToTest(BRIG_OPCODE_LD, segment, type, result, globalVar);
    EmitInstrToTest(BRIG_OPCODE_LD, segment2, type2, result2, globalVar2);
    if (type != type2) {
      if (getBrigTypeNumBits(type) == getBrigTypeNumBits(type2) && isIntType(type) && isIntType(type2)) {
        be.EmitArith(BRIG_OPCODE_ADD, result, result->Reg(), result2->Reg());
      } else {
        BrigRound round = BRIG_ROUND_NONE;
        if (isIntType(type) && isFloatType(type2)) {
          round = BRIG_ROUND_INTEGER_ZERO;
        }
        if (isIntType(type2) && isFloatType(type)) {
          round = BRIG_ROUND_FLOAT_ZERO;
        }
        TypedReg result1 = be.AddTReg(ResultType());
        be.EmitCvt(result1, result2, round);
        be.EmitArith(BRIG_OPCODE_ADD, result, result->Reg(), result1->Reg());
      }
    } else {
      be.EmitArith(BRIG_OPCODE_ADD, result, result->Reg(), result2->Reg());
    }
    return result;
  }

};

void MemoryFenceTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<MemoryFenceTest>(ap, it, "memfence/basic", cc->Grids().MemfenceSet(), cc->Types().Memfence(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
  TestForEach<MemoryFenceCompoundTest>(ap, it, "memfence/compound", cc->Grids().MemfenceSet(), cc->Types().Memfence(), cc->Types().Memfence(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
}

}
