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
#include "UtilTests.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace Brig;
using namespace HSAIL_ASM;
using namespace hsail_conformance::utils;

namespace hsail_conformance {

class EquivalenceClassesLimitsTest: public SkipTest {
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
  explicit EquivalenceClassesLimitsTest(BrigOpcode opcode_): SkipTest(Location::KERNEL), opcode(opcode_) {}

  void Init() override {
    SkipTest::Init();
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

    return SkipTest::Result();
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


class WorkGroupSizeLimitTest: public Test {
private:
  static const int LIMIT = 256;

  TypedReg WorkgroupSize() {
    auto xSize = be.EmitWorkgroupSize(0);
    auto ySize = be.EmitWorkgroupSize(1);
    auto zSize = be.EmitWorkgroupSize(2);

    auto size = be.AddTReg(BRIG_TYPE_U32);
    be.EmitArith(BRIG_OPCODE_MUL, size, xSize, ySize->Reg());
    be.EmitArith(BRIG_OPCODE_MUL, size, size, zSize->Reg());
    return size;
  }

public:
  explicit WorkGroupSizeLimitTest(Grid geometry): Test(Location::KERNEL, geometry) {}

  void Name(std::ostream& out) const {
    out << geometry;
  }
  
  bool IsValid() const override {
    return geometry->WorkgroupSize() >= LIMIT &&
           geometry->isPartial() == false;
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  TypedReg Result() override {
    // compare current workgroup size with Limit (256)
    auto wgSize = WorkgroupSize();
    auto ge = be.AddCTReg();
    be.EmitCmp(ge->Reg(), wgSize, be.Immed(wgSize->Type(), LIMIT), BRIG_COMPARE_GE);

    auto result = be.AddTReg(ResultType());
    be.EmitCvt(result, ge);
    return result;
  }
};


class WavesizeLimitTest: public Test {
private:
  static const int BOTTOM_LIMIT = 1;
  static const int TOP_LIMIT = 256;

public:
  explicit WavesizeLimitTest(Grid geometry): Test(Location::KERNEL, geometry) {}

  void Name(std::ostream& out) const {
    out << geometry;
  }
  
  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  TypedReg Result() override {
    //wavesize
    auto waveSize = be.AddTReg(BRIG_TYPE_U64);
    be.EmitMov(waveSize, be.Wavesize());
    
    // compare wavesize with limits (1; 256)
    auto ge = be.AddCTReg();
    be.EmitCmp(ge->Reg(), waveSize, be.Immed(waveSize->Type(), BOTTOM_LIMIT), BRIG_COMPARE_GE);
    auto le = be.AddCTReg();
    be.EmitCmp(le->Reg(), waveSize, be.Immed(waveSize->Type(), TOP_LIMIT), BRIG_COMPARE_LE);
    
    // check if wavesize is a power of 2 with help of formula: x & (x - 1) == 0
    auto tmp = be.AddTReg(waveSize->Type());
    be.EmitArith(BRIG_OPCODE_SUB, tmp, waveSize, be.Immed(waveSize->Type(), 1));
    be.EmitArith(BRIG_OPCODE_AND, tmp, waveSize, tmp->Reg());
    auto eq = be.AddCTReg();
    be.EmitCmp(eq->Reg(), tmp, be.Immed(tmp->Type(), 0), BRIG_COMPARE_EQ);

    // check all conditions
    auto cmp = be.AddCTReg();
    be.EmitArith(BRIG_OPCODE_AND, cmp, ge, le->Reg());
    be.EmitArith(BRIG_OPCODE_AND, cmp, cmp, eq->Reg());
    auto result = be.AddTReg(ResultType());
    be.EmitCvt(result, cmp);
    return result;
  }
};


class WorkGroupNumberLimitTest : public SkipTest {
private:
  static const uint64_t LIMIT = 0xffffffff; // 2^32 - 1
  static const GridGeometry LIMIT_GEOMETRY; // geometry with 2^32 - 1 work-groups

public:
  explicit WorkGroupNumberLimitTest(bool): SkipTest(Location::KERNEL, &LIMIT_GEOMETRY) { }

  void Name(std::ostream& out) const {}
};

const GridGeometry WorkGroupNumberLimitTest::LIMIT_GEOMETRY(3, 65537, 257, 255, 1, 1, 1);


class DimsLimitTest : public BoundaryTest {
private:
  static const uint64_t LIMIT = 0xffffffff; // 2^32 - 1

public:
  explicit DimsLimitTest(Grid geometry): BoundaryTest(1, Location::KERNEL, geometry) {}

  void Name(std::ostream& out) const override {
    out << geometry;
  }

  bool IsValid() const override {
    return geometry->GridSize() == LIMIT;
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  TypedReg Result() override {
    // compare grid sizes for each dimension reported by instruction gridsize with one obtained from original geometry
    auto gridSize = be.AddTReg(BRIG_TYPE_U32);
    auto eq = be.AddCTReg();
    auto cand = be.AddCTReg();
    be.EmitMov(cand, be.Immed(cand->Type(), 1));
    for (uint16_t i = 0; i < 3; ++i) {
      gridSize = be.EmitGridSize(i);
      be.EmitCmp(eq->Reg(), gridSize, be.Immed(gridSize->Type(), geometry->GridSize(i)), BRIG_COMPARE_EQ);
      be.EmitArith(BRIG_OPCODE_AND, cand, cand, eq->Reg());
    }
    
    auto result = be.AddTReg(ResultType());
    be.EmitCvt(result, cand);
    return result;
  }
};


void LimitsTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string path;
  Arena* ap = cc->Ap();

  TestForEach<EquivalenceClassesLimitsTest>(ap, it, "equiv", cc->Memory().LdStOpcodes());
  TestForEach<AtomicEquivalenceLimitsTest>(ap, it, "equiv", cc->Memory().AtomicOpcodes(), cc->Memory().AtomicOperations());

  TestForEach<WorkGroupSizeLimitTest>(ap, it, "wgsize", cc->Grids().WorkGroupsSize256());

  TestForEach<WavesizeLimitTest>(ap, it, "wavesize", cc->Grids().SimpleSet());
  
  TestForEach<WorkGroupNumberLimitTest>(ap, it, "wgnumber", Bools::Value(true));
  
  TestForEach<DimsLimitTest>(ap, it, "dims", cc->Grids().LimitGridSet());
}

}
