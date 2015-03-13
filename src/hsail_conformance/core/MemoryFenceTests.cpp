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
  static const uint32_t workgroupSizeX = 256;
  static const BrigType type = BRIG_TYPE_U64;

  MemoryFenceTest(Grid geometry_, BrigOpcode opcode_, BrigMemoryOrder memoryOrder_, BrigSegment segment_, BrigMemoryScope memoryScope_)
  : Test(KERNEL, geometry_), opcode(opcode_), memoryOrder(memoryOrder_), segment(segment_), memoryScope(memoryScope_),
    initialValue(0), expectedValue(1) {}

  void Name(std::ostream& out) const {
    out << opcode2str(opcode) << "_" << segment2str(segment) << "_" << type2str(type) << "/"
        << opcode2str(BRIG_OPCODE_MEMFENCE) << "_" << memoryOrder2str(memoryOrder) << "_" << memoryScope2str(memoryScope);
  }

protected:
  // Instruction opcode to test with memfence
  BrigOpcode opcode;
  BrigMemoryOrder memoryOrder;
  BrigSegment segment;
  BrigMemoryScope memoryScope;
  int64_t initialValue;
  int64_t expectedValue;
  DirectiveVariable globalVar;
  OperandAddress globalVarAddr;
  Buffer input;
  TypedReg inputReg;
  TypedReg destReg;

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  bool IsValid() const {
    // TODO: memfence on ld.
    // [Note] it looks from the first sight like at least 2 kernels are needed:
    // 1 stores, 2 loads and memfence for both.
    if (BRIG_OPCODE_LD == opcode) return false;
    return true;
  }

  virtual uint64_t GetInputValueForWI(uint64_t wi) const {
    switch (opcode) {
      case BRIG_OPCODE_ST: return wi;
      default: return wi;
    }
  }

  Value ExpectedResult(uint64_t i) const {
    switch (opcode) {
      case BRIG_OPCODE_LD: return Value(MV_UINT32, i);
      case BRIG_OPCODE_ST: return Value(MV_UINT32, workgroupSizeX - 1);
      default: return Value(MV_UINT32, 2);
    }
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

  void EmitInstrToTest() {
    globalVarAddr = be.Address(globalVar);
    switch (opcode) {
      case BRIG_OPCODE_LD:
//        destReg = be.AddTReg(ResultType());
//        be.EmitLoad(segment, destReg, globalVarAddr);
        break;
      case BRIG_OPCODE_ST:
        destReg = be.AddTReg(globalVar.type());
        be.EmitStore(segment, type, inputReg->Reg(), globalVarAddr);
        break;
      default: assert(false); break;
    }
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    inputReg = be.AddTReg(type);
    input->EmitLoadData(inputReg);
    EmitInstrToTest();
    be.EmitMemfence(memoryOrder, memoryScope, BRIG_MEMORY_SCOPE_NONE, BRIG_MEMORY_SCOPE_NONE);
    TypedReg destReg = be.AddTReg(globalVar.type());
    if (opcode != BRIG_OPCODE_LD)
      be.EmitAtomic(destReg, globalVarAddr, NULL, NULL, BRIG_ATOMIC_LD, BRIG_MEMORY_ORDER_SC_ACQUIRE, be.AtomicMemoryScope(memoryScope, segment), segment, false);
//    if (result->Type() != addrReg->Type())
    be.EmitCvt(result, destReg);
    return result;
  }
};

void MemoryFenceTests::Iterate(TestSpecIterator& it)
{
//  CoreConfig* cc = CoreConfig::Get(context);
//  Arena* ap = cc->Ap();
//  TestForEach<MemoryFenceTest>(ap, it, "memfence", cc->Grids().SeveralWavesSet(), cc->Memory().LdStOpcodes(), cc->Memory().MemfenceMemoryOrders(), cc->Memory().MemfenceSegments(), cc->Memory().MemfenceMemoryScopes());
}

}
