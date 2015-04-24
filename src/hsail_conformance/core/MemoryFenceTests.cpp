/*
   Copyright 2014-2015 Heterogeneous System Architecture (HSA) Foundation

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
    initialValue(0), s_label_skip_store("@skip_store"), s_label_skip_memfence("@skip_memfence"), globalVarName("global_var"), globalFlagName("global_flag") {}

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
  std::string globalVarName;
  std::string globalFlagName;
  DirectiveVariable globalVar;
  DirectiveVariable globalFlag;
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
    switch (segment) {
      case BRIG_SEGMENT_GROUP: globalVarName = "group_var"; break;
      default: break;
    }
    globalVar = be.EmitVariableDefinition(globalVarName, segment, type);
    globalFlag = be.EmitVariableDefinition(globalFlagName, BRIG_SEGMENT_GLOBAL, be.PointerType());
    if (segment != BRIG_SEGMENT_GROUP) {
      globalVar.init() = be.Immed(type, initialValue, false);
    }
  }

  void EmitInstrToTest(BrigOpcode opcode, BrigSegment seg, BrigType t, TypedReg reg, DirectiveVariable var) {
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
    be.EmitArith(BRIG_OPCODE_DIV, wiID, wiID, be.Wavesize());
    TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
    uint64_t lastWaveFront = geometry->GridSize()/te->CoreCfg()->Wavesize() - 1;
    be.EmitCmp(cReg->Reg(), wiID, be.Immed(wiID->Type(), lastWaveFront), BRIG_COMPARE_NE);
    TypedReg flagReg = be.AddTReg(be.PointerType());
    be.EmitCbr(cReg, s_label_skip_store);

    EmitInstrToTest(BRIG_OPCODE_ST, segment, type, inputReg, globalVar);
    be.EmitMov(flagReg, 1);
    be.EmitMemfence(memoryOrder1, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);

    OperandAddress flagAddr = be.Address(globalFlag);
    be.EmitAtomic(NULL, flagAddr, flagReg, NULL, BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_RELAXED, be.AtomicMemoryScope(BRIG_MEMORY_SCOPE_SYSTEM, segment), BRIG_SEGMENT_GLOBAL);
    be.EmitBr(s_label_skip_memfence);

    be.EmitLabel(s_label_skip_store);
    TypedReg flagReg2 = be.AddTReg(be.PointerType());
    be.EmitAtomic(flagReg2, flagAddr, NULL, NULL, BRIG_ATOMIC_LD, BRIG_MEMORY_ORDER_RELAXED, be.AtomicMemoryScope(BRIG_MEMORY_SCOPE_SYSTEM, segment), BRIG_SEGMENT_GLOBAL);
    be.EmitCmp(cReg->Reg(), flagReg2, be.Immed(flagReg2->Type(), 1), BRIG_COMPARE_NE);
    be.EmitCbr(cReg, s_label_skip_store);
    be.EmitMemfence(memoryOrder2, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);

    be.EmitLabel(s_label_skip_memfence);
    EmitInstrToTest(BRIG_OPCODE_LD, segment, type, result, globalVar);
    return result;
  }
};

class MemoryFenceArrayTest : public MemoryFenceTest {
public:
  MemoryFenceArrayTest(Grid geometry_, BrigType type_, BrigMemoryOrder memoryOrder1_, BrigMemoryOrder memoryOrder2_, BrigSegment segment_, BrigMemoryScope memoryScope_)
  : MemoryFenceTest(geometry_, type_, memoryOrder1_, memoryOrder2_, segment_, memoryScope_) {}

protected:

  Value ExpectedResult(uint64_t i) const {
    uint64_t val = geometry->GridSize()-i-1;
    ValueType valType = Brig2ValueType(type);
    switch (valType) {
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
      case MV_PLAIN_FLOAT16:
#endif
      case MV_FLOAT16: return Value(valType, H(hexl::half(float(val))));
      case MV_FLOAT: return Value(float(val));
      case MV_DOUBLE: return Value(double(val));
      default: return Value(valType, val);
    }
  }

  void Init() {
    Test::Init();
  }

  bool IsValid() const {
    if (!MemoryFenceTest::IsValid())
      return false;
    if (BRIG_TYPE_F16 == type)
      return false;
    if (segment == BRIG_SEGMENT_GROUP)
      return false;
    return true;
  }

  virtual void ModuleVariables() {
    switch (segment) {
      case BRIG_SEGMENT_GROUP: globalVarName = "group_var"; break;
      default: break;
    }
    globalVar = be.EmitVariableDefinition(globalVarName, segment, elementType2arrayType(type), 0, geometry->GridSize());
    globalFlag = be.EmitVariableDefinition(globalFlagName, BRIG_SEGMENT_GLOBAL, elementType2arrayType(be.PointerType()), 0, geometry->GridSize());
  }

  void EmitVectorInstrToTest(BrigOpcode opcode, BrigType t, TypedReg reg, DirectiveVariable var, TypedReg offsetReg) {
    switch (opcode) {
      case BRIG_OPCODE_LD:
        be.EmitLoad(reg, var, offsetReg->Reg(), 0, true);
        break;
      case BRIG_OPCODE_ST:
        be.EmitStore(reg, var, offsetReg->Reg(), 0, true);
        break;
      default: assert(false); break;
    }
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    TypedReg wiID = be.EmitWorkitemFlatAbsId(te->CoreCfg()->IsLarge());
    
    unsigned bytes = getBrigTypeNumBytes(type);
    unsigned pow = unsigned(std::log(bytes)/std::log(2));
    TypedReg offsetReg = be.AddTReg(wiID->Type());
    be.EmitArith(BRIG_OPCODE_SHL, offsetReg, wiID, be.Immed(BRIG_TYPE_U32, pow));

    unsigned bytesFlag = getBrigTypeNumBytes(be.PointerType());
    unsigned powFlag = unsigned(std::log(bytesFlag)/std::log(2));
    TypedReg offsetFlagReg = be.AddTReg(wiID->Type());
    be.EmitArith(BRIG_OPCODE_SHL, offsetFlagReg, wiID, be.Immed(BRIG_TYPE_U32, powFlag));

    TypedReg wiID_ld = be.AddTReg(wiID->Type());
    TypedReg offsetReg_ld = be.AddTReg(wiID->Type());
    TypedReg offsetFlagReg_ld = be.AddTReg(wiID->Type());
    be.EmitArith(BRIG_OPCODE_SUB, wiID_ld, be.Immed(wiID->Type(), geometry->GridSize()-1), wiID->Reg());
    be.EmitArith(BRIG_OPCODE_SHL, offsetReg_ld, wiID_ld, be.Immed(BRIG_TYPE_U32, pow));
    be.EmitArith(BRIG_OPCODE_SHL, offsetFlagReg_ld, wiID_ld, be.Immed(BRIG_TYPE_U32, powFlag));

    TypedReg idReg = be.AddTReg(type);
    be.EmitCvtOrMov(idReg, wiID);

    EmitVectorInstrToTest(BRIG_OPCODE_ST, type, idReg, globalVar, offsetReg);
    be.EmitMemfence(memoryOrder1, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);

    be.EmitAtomic(NULL, be.Address(globalFlag, offsetFlagReg->Reg(), 0), wiID, NULL, BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_RELAXED, be.AtomicMemoryScope(BRIG_MEMORY_SCOPE_SYSTEM, segment), BRIG_SEGMENT_GLOBAL);

    be.EmitLabel(s_label_skip_store);
    TypedReg flagReg2 = be.AddTReg(be.PointerType());
    be.EmitAtomic(flagReg2, be.Address(globalFlag, offsetFlagReg_ld->Reg(), 0), NULL, NULL, BRIG_ATOMIC_LD, BRIG_MEMORY_ORDER_RELAXED, be.AtomicMemoryScope(BRIG_MEMORY_SCOPE_SYSTEM, segment), BRIG_SEGMENT_GLOBAL);
    TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
    be.EmitCmp(cReg->Reg(), flagReg2, wiID_ld->Reg(), BRIG_COMPARE_NE);
    be.EmitCbr(cReg, s_label_skip_store);
    be.EmitMemfence(memoryOrder2, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);

    EmitVectorInstrToTest(BRIG_OPCODE_LD, type, result, globalVar, offsetReg_ld);
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

  BrigType ResultType2() const { return type2; }

  bool IsValid() const {
    if (!MemoryFenceTest::IsValid())
      return false;
    if (type != type2)
      return false;
    //if (isFloatType(type) || isFloatType(type2))
    //  return false;
    if (BRIG_SEGMENT_GROUP == segment2 && geometry->WorkgroupSize() != geometry->GridSize())
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
      globalVar2.init() = be.Immed(type2, initialValue, false);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    TypedReg result2 = be.AddTReg(ResultType2());
    inputReg = be.AddTReg(type);
    inputReg2 = be.AddTReg(type2);
    input->EmitLoadData(inputReg);
    input2->EmitLoadData(inputReg2);
    TypedReg wiID = be.EmitWorkitemFlatAbsId(true);
    be.EmitArith(BRIG_OPCODE_DIV, wiID, wiID, be.Wavesize());
    TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
    uint64_t lastWaveFront = geometry->GridSize()/te->CoreCfg()->Wavesize() - 1;
    be.EmitCmp(cReg->Reg(), wiID, be.Immed(wiID->Type(), lastWaveFront), BRIG_COMPARE_NE);
    TypedReg flagReg = be.AddTReg(be.PointerType());
    be.EmitCbr(cReg, s_label_skip_store);

    EmitInstrToTest(BRIG_OPCODE_ST, segment, type, inputReg, globalVar);
    EmitInstrToTest(BRIG_OPCODE_ST, segment2, type2, inputReg2, globalVar2);
    be.EmitMov(flagReg, 1);
    be.EmitMemfence(memoryOrder1, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    OperandAddress flagAddr = be.Address(globalFlag);
    be.EmitAtomic(NULL, flagAddr, flagReg, NULL, BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_RELAXED, be.AtomicMemoryScope(BRIG_MEMORY_SCOPE_SYSTEM, segment), BRIG_SEGMENT_GLOBAL);
    be.EmitBr(s_label_skip_memfence);
    be.EmitLabel(s_label_skip_store);

    TypedReg flagReg2 = be.AddTReg(be.PointerType());
    be.EmitAtomic(flagReg2, flagAddr, NULL, NULL, BRIG_ATOMIC_LD, BRIG_MEMORY_ORDER_RELAXED, be.AtomicMemoryScope(BRIG_MEMORY_SCOPE_SYSTEM, segment), BRIG_SEGMENT_GLOBAL);
    be.EmitCmp(cReg->Reg(), flagReg2, be.Immed(flagReg2->Type(), 1), BRIG_COMPARE_NE);
    be.EmitCbr(cReg, s_label_skip_store);
    be.EmitMemfence(memoryOrder2, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitLabel(s_label_skip_memfence);

    EmitInstrToTest(BRIG_OPCODE_LD, segment, type, result, globalVar);
    EmitInstrToTest(BRIG_OPCODE_LD, segment2, type2, result2, globalVar2);

    TypedReg result1 = be.AddTReg(type);
    be.EmitCvtOrMov(result1, result2);
    be.EmitArith(BRIG_OPCODE_ADD, result, result->Reg(), result1->Reg());
    return result;
  }

};

void MemoryFenceTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<MemoryFenceTest>(ap, it, "memfence/basic", cc->Grids().MemfenceSet(), cc->Types().Memfence(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
  TestForEach<MemoryFenceArrayTest>(ap, it, "memfence/array", cc->Grids().MemfenceSet(), cc->Types().Memfence(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
  TestForEach<MemoryFenceCompoundTest>(ap, it, "memfence/compound", cc->Grids().MemfenceSet(), cc->Types().Memfence(), cc->Types().Memfence(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
}

}
