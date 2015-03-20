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
  TypedReg destReg;

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  bool IsValid() const {
    if (BRIG_SEGMENT_GROUP == segment && geometry->WorkgroupSize() != geometry->GridSize())
      return false;
    if (BRIG_MEMORY_ORDER_SC_ACQUIRE == memoryOrder1)
      return false;
    if (BRIG_MEMORY_ORDER_SC_RELEASE == memoryOrder2)
      return false;
    return true;
  }

  virtual uint64_t GetInputValueForWI(uint64_t wi) const {
    return 7;
  }

  Value ExpectedResult(uint64_t i) const {
    return Value(MV_UINT32, 7);
  }

  void Init() {
    Test::Init();
    input = kernel->NewBuffer("input", HOST_INPUT_BUFFER, MV_UINT64, geometry->GridSize());
    for (uint64_t i = 0; i < uint64_t(geometry->GridSize()); ++i) {
      input->AddData(Value(MV_UINT64, GetInputValueForWI(i)));
    }
  }

  void ModuleVariables() {
    std::string globalVarName = "global_var";
    switch (segment) {
      case BRIG_SEGMENT_GROUP: globalVarName = "group_var"; break;
      default: break;
    }
    globalVar = be.EmitVariableDefinition(globalVarName, segment, type);
    if (segment != BRIG_SEGMENT_GROUP)
      globalVar.init() = be.Immed(type, initialValue);
  }

  void EmitInstrToTest(BrigOpcode opcode) {
    globalVarAddr = be.Address(globalVar);
    switch (opcode) {
      case BRIG_OPCODE_LD:
        be.EmitLoad(segment, type, destReg->Reg(), globalVarAddr);
        break;
      case BRIG_OPCODE_ST:
        be.EmitStore(segment, type, inputReg->Reg(), globalVarAddr);
        break;
      default: assert(false); break;
    }
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    inputReg = be.AddTReg(type);
    input->EmitLoadData(inputReg);
    TypedReg wiID = be.EmitWorkitemFlatAbsId(false);
    TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
    SRef s_label_skip_store = "@skip_store";
    SRef s_label_skip_load = "@skip_load";
    destReg = be.AddTReg(globalVar.type());
    be.EmitCmp(cReg->Reg(), wiID, be.Immed(wiID->Type(), 1), BRIG_COMPARE_NE);
    be.EmitCbr(cReg, s_label_skip_store);

    EmitInstrToTest(BRIG_OPCODE_ST);
    be.EmitMemfence(memoryOrder1, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitLabel(s_label_skip_store);

    be.EmitCmp(cReg->Reg(), wiID, be.Immed(wiID->Type(), 1), BRIG_COMPARE_EQ);
    be.EmitCbr(cReg, s_label_skip_load);
    be.EmitMemfence(memoryOrder2, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    be.EmitLabel(s_label_skip_load);

    EmitInstrToTest(BRIG_OPCODE_LD);
    if (type != BRIG_TYPE_U32 && type != BRIG_TYPE_S32) {
      if (type != BRIG_TYPE_F32 && type != BRIG_TYPE_F64) {
        be.EmitCvt(result, destReg);
      } else {
        be.EmitCvt(result, destReg, BRIG_ROUND_INTEGER_NEAR_EVEN);
      }
    }
    return result;
  }
};

void MemoryFenceTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<MemoryFenceTest>(ap, it, "memfence", cc->Grids().MemfenceSet(), cc->Types().Memfence(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
}

}
