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

#include "LimitsTests.hpp"
#include "HCTests.hpp"
#include "BrigEmitter.hpp"
#include "CoreConfig.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace Brig;
using namespace HSAIL_ASM;

namespace hsail_conformance {

class EquivalenceClassesLimitsTest: public Test {
private:
  std::vector<Variable> memories;
  BrigOpcode opcode;

protected:
  static const int LIMIT = 256;

  Variable Memory(uint32_t index) const { return memories[index]; }
  BrigOpcode Opcode() const { return opcode; }
  BrigTypeX ValueType() {
    return te->CoreCfg()->IsLarge() ? BRIG_TYPE_U64 : BRIG_TYPE_U32;
  }

  virtual void EmitEquivInstruction(uint32_t equivClass, TypedReg dst, TypedReg src0, TypedReg src1) {
    if (opcode == BRIG_OPCODE_LD) {
      be.EmitLoad(BRIG_SEGMENT_GROUP, dst, be.Address(Memory(equivClass)->Variable()), true, equivClass);  
    } else if (opcode == BRIG_OPCODE_ST) {
      be.EmitStore(BRIG_SEGMENT_GROUP, src0, be.Address(Memory(equivClass)->Variable()), true, equivClass);
    } else {
      assert(false);
    }
  }

public:
  explicit EquivalenceClassesLimitsTest(BrigOpcode opcode_): Test(Location::KERNEL), opcode(opcode_) {}

  void Init() override {
    Test::Init();
    memories.reserve(LIMIT);
    for (int i = 0; i < LIMIT; ++i) {
      memories.push_back(kernel->NewVariable("memory_" + std::to_string(i), BRIG_SEGMENT_GROUP, ValueType()));
    }
  }

  void Name(std::ostream& out) const override {
    out << opcode2str(opcode);
  }

  bool IsValid() const override {
    return opcode == BRIG_OPCODE_ST || opcode == BRIG_OPCODE_LD;
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 0); }

  TypedReg Result() override {
    // generate 256 instructions with 256 equivalence classes
    auto dst = be.AddTReg(ValueType());
    be.EmitMov(dst, be.Immed(ValueType(), 0));
    auto src0 = be.AddTReg(ValueType());
    be.EmitMov(src0, be.Immed(ValueType(), 0));
    auto src1 = be.AddTReg(ValueType());
    be.EmitMov(src1, be.Immed(ValueType(), 0));
    for (uint32_t i = 0; i < LIMIT; ++i) {
      EmitEquivInstruction(i, dst, src0, src1);
    }

    // result value of kernel
    auto result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(result->Type(), 0));
    return result;
  }
};


class AtomicEquivalenceLimitsTest: public EquivalenceClassesLimitsTest {
private:
  BrigAtomicOperation atomicOperation;

protected:
  virtual void EmitEquivInstruction(uint32_t equivClass, TypedReg dst, TypedReg src0, TypedReg src1) override {
    auto memoryOrder = be.AtomicMemoryOrder(atomicOperation, BRIG_MEMORY_ORDER_RELAXED);
    auto memoryScope = be.AtomicMemoryScope(BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_SEGMENT_GROUP);
    if (Opcode() == BRIG_OPCODE_ATOMICNORET) {
      dst = nullptr;
    }
    be.EmitAtomic(dst, be.Address(Memory(equivClass)->Variable()), src0, src1, 
                  atomicOperation, memoryOrder, memoryScope, BRIG_SEGMENT_GROUP, false, equivClass);
  }

public:
  AtomicEquivalenceLimitsTest(BrigOpcode opcode_, BrigAtomicOperation atomicOperation_):
    EquivalenceClassesLimitsTest(opcode_), atomicOperation(atomicOperation_) {}

  void Name(std::ostream& out) const override {
    EquivalenceClassesLimitsTest::Name(out);
    out << "_" << atomicOperation2str(atomicOperation);
  }

  bool IsValid() const override {
    if (Opcode() == BRIG_OPCODE_ATOMIC) {
      return atomicOperation == BRIG_ATOMIC_LD ||
             atomicOperation == BRIG_ATOMIC_AND ||
             atomicOperation == BRIG_ATOMIC_OR ||
             atomicOperation == BRIG_ATOMIC_XOR ||
             atomicOperation == BRIG_ATOMIC_EXCH ||
             atomicOperation == BRIG_ATOMIC_ADD ||
             atomicOperation == BRIG_ATOMIC_SUB ||
             atomicOperation == BRIG_ATOMIC_WRAPINC ||
             atomicOperation == BRIG_ATOMIC_WRAPDEC ||
             atomicOperation == BRIG_ATOMIC_MAX ||
             atomicOperation == BRIG_ATOMIC_MIN ||
             atomicOperation == BRIG_ATOMIC_CAS;
    } else if (Opcode() == BRIG_OPCODE_ATOMICNORET) {
      return atomicOperation == BRIG_ATOMIC_ST ||
             atomicOperation == BRIG_ATOMIC_AND ||
             atomicOperation == BRIG_ATOMIC_OR ||
             atomicOperation == BRIG_ATOMIC_XOR ||
             atomicOperation == BRIG_ATOMIC_ADD ||
             atomicOperation == BRIG_ATOMIC_SUB ||
             atomicOperation == BRIG_ATOMIC_WRAPINC ||
             atomicOperation == BRIG_ATOMIC_WRAPDEC ||
             atomicOperation == BRIG_ATOMIC_MAX ||
             atomicOperation == BRIG_ATOMIC_MIN ||
             atomicOperation == BRIG_ATOMIC_CAS;
    } else {
      return false;
    }
  }
};


void LimitsTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string path;
  Arena* ap = cc->Ap();

  TestForEach<EquivalenceClassesLimitsTest>(ap, it, "equiv", cc->Memory().LdStOpcodes());
  TestForEach<AtomicEquivalenceLimitsTest>(ap, it, "equiv", cc->Memory().AtomicOpcodes(), cc->Memory().AtomicOperations());
}

}
