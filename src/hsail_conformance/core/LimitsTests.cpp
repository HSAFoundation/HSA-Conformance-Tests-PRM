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

#include "LimitsTests.hpp"
#include "HCTests.hpp"
#include "BrigEmitter.hpp"
#include "CoreConfig.hpp"
#include "UtilTests.hpp"

using namespace hexl;
using namespace hexl::emitter;
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
  BrigType ValueType() {
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
    be.EmitMov(dst, (uint64_t) 0);
    auto src0 = be.AddTReg(ValueType());
    be.EmitMov(src0, (uint64_t) 0);
    auto src1 = be.AddTReg(ValueType());
    be.EmitMov(src1, (uint64_t) 0);
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
             atomicOperation == BRIG_ATOMIC_MIN;
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

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
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
  
  BrigType ResultType() const override { return BRIG_TYPE_U32; }
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

  void Name(std::ostream& out) const override {}
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

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  TypedReg Result() override {
    // compare grid sizes for each dimension reported by instruction gridsize with one obtained from original geometry
    auto gridSize = be.AddTReg(BRIG_TYPE_U32);
    auto eq = be.AddCTReg();
    auto cand = be.AddCTReg();
    be.EmitMov(cand, 1);
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


class MemorySegmentSizeLimitTest: public Test {
private:
  Variable var;

protected:
  Variable GetVariable() const { return var; }

  virtual void EmitInitialization(TypedReg value) = 0;
  virtual TypedReg EmitValue() = 0;
  virtual Variable InitializeVariable() = 0;
  virtual uint32_t Limit() const = 0;
  virtual BrigSegment Segment() const = 0;

public:
  explicit MemorySegmentSizeLimitTest(Location codeLocation = Location::KERNEL, Grid geometry = 0): 
      Test(codeLocation, geometry) {}

  void Init() override {
    Test::Init();
    var = InitializeVariable();
  }

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(ResultType()), 1); }

  TypedReg Result() override {
    auto falseLabel = "@false";
    auto endLabel = "@end";
    
    auto value = EmitValue();
    EmitInitialization(value);

    // read values from first and last positions in each work-items
    // compare values with expected

    // read first
    auto first = be.AddTReg(value->Type());
    var->EmitLoadTo(first);
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp->Reg(), first, value, BRIG_COMPARE_NE);
    be.EmitCbr(cmp->Reg(), falseLabel);

    // read last
    auto offset = Limit() - getBrigTypeNumBytes(value->Type());
    auto last = be.AddTReg(value->Type());
    be.EmitLoad(Segment(), last, be.Address(var->Variable(), offset));
    be.EmitCmp(cmp->Reg(), last, value, BRIG_COMPARE_NE);
    be.EmitCbr(cmp->Reg(), falseLabel);

    auto result = be.AddTReg(ResultType());
    be.EmitMov(result, 1);
    be.EmitBr(endLabel);

    be.EmitLabel(falseLabel);
    be.EmitMov(result, (uint64_t) 0);

    be.EmitLabel(endLabel);
    return result;
  }
};



class GroupMemorySizeLimitTest: public MemorySegmentSizeLimitTest {
private:
  static const uint32_t LIMIT = 0x8000; // 32 KBytes of group memory
  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

protected:
  void EmitInitialization(TypedReg value) override {
    auto skipLabel = "@skip_initializer";
    // store VALUE in first and last positions of var in first work-item
    auto wiId = be.EmitWorkitemFlatId();
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp->Reg(), wiId, be.Immed(wiId->Type(), 0), BRIG_COMPARE_NE);
    be.EmitCbr(cmp->Reg(), skipLabel);

    // store in first position
    GetVariable()->EmitStoreFrom(value);

    // store in last position
    auto offset = LIMIT - getBrigTypeNumBytes(value->Type());
    be.EmitStore(BRIG_SEGMENT_GROUP, value, be.Address(GetVariable()->Variable(), offset));

    be.EmitLabel(skipLabel);
    be.EmitBarrier();
  }

  TypedReg EmitValue() override {
    auto value = be.AddTReg(VALUE_TYPE);
    be.EmitMov(value, VALUE);
    return value;
  }

  Variable InitializeVariable() override {
    return kernel->NewVariable("var", BRIG_SEGMENT_GROUP, VALUE_TYPE, Location::AUTO, BRIG_ALIGNMENT_NONE, LIMIT / getBrigTypeNumBytes(VALUE_TYPE));
  }

  uint32_t Limit() const override { return LIMIT; }
  BrigSegment Segment() const override { return BRIG_SEGMENT_GROUP; }

public:
  explicit GroupMemorySizeLimitTest(Grid geometry): MemorySegmentSizeLimitTest(Location::KERNEL, geometry) {}

  bool IsValid() const override {
    return geometry->GridGroups() == 1;
  }

  void Name(std::ostream& out) const override {
    out << geometry;
  }
};


class PrivateMemorySizeLimitTest: public MemorySegmentSizeLimitTest {
private:
  static const uint32_t LIMIT = 0x100; // 256 Bytes of private memory per work-item = 256 * 256 = 64 KBytes

protected:
  void EmitInitialization(TypedReg value) override {
    // store value in first and last positions
    GetVariable()->EmitStoreFrom(value);
    auto offset = LIMIT - getBrigTypeNumBytes(value->Type());
    be.EmitStore(BRIG_SEGMENT_PRIVATE, value, be.Address(GetVariable()->Variable(), offset));
  }

  TypedReg EmitValue() override {
    return be.WorkitemFlatAbsId(false);
  }

  Variable InitializeVariable() override {
    return kernel->NewVariable("var", BRIG_SEGMENT_PRIVATE, BRIG_TYPE_U32, Location::AUTO, 
                               BRIG_ALIGNMENT_NONE, LIMIT/getBrigTypeNumBytes(BRIG_TYPE_U32));
  }

  uint32_t Limit() const override { return LIMIT; }
  BrigSegment Segment() const override { return BRIG_SEGMENT_PRIVATE; }

public:
  explicit PrivateMemorySizeLimitTest(Grid geometry): MemorySegmentSizeLimitTest(Location::KERNEL, geometry) {}

  bool IsValid() const override {
    return 256 == geometry->WorkgroupSize()   // check that work-group is full
        && !geometry->isPartial();
  }

  void Name(std::ostream& out) const override {
    out << geometry;
  }
};


class KernargMemorySizeLimitTest: public MemorySegmentSizeLimitTest {
private:
  static const uint32_t LIMIT = 1024; // 1KByte of kernarg memory
  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

  uint32_t VarSize() const {
    return Limit() / getBrigTypeNumBytes(VALUE_TYPE);
  }

protected:
  void EmitInitialization(TypedReg value) override {}

  TypedReg EmitValue() override {
    auto value = be.AddTReg(VALUE_TYPE);
    be.EmitMov(value, VALUE);
    return value;
  }

  Variable InitializeVariable() override {
    auto var = kernel->NewVariable("var", BRIG_SEGMENT_KERNARG, VALUE_TYPE, Location::AUTO, BRIG_ALIGNMENT_NONE, VarSize());
    for (uint32_t i = 0; i < VarSize(); ++i) {
      var->AddData(Value(Brig2ValueType(VALUE_TYPE), VALUE));
    }
    return var;
  }

  uint32_t Limit() const override { 
    return LIMIT - (getSegAddrSize(BRIG_SEGMENT_GLOBAL, te->CoreCfg()->IsLarge()) / 8); 
  }

  BrigSegment Segment() const override { return BRIG_SEGMENT_KERNARG; }

public:
  explicit KernargMemorySizeLimitTest(Grid geometry): MemorySegmentSizeLimitTest(Location::KERNEL, geometry) {}

  void Name(std::ostream& out) const override {
    out << geometry;
  }
};


class ArgMemorySizeLimitTest: public MemorySegmentSizeLimitTest {
private:
  static const uint32_t LIMIT = 64; // 64 bytes of arg memory
  static const BrigType VALUE_TYPE = BRIG_TYPE_U64;
  static const uint32_t VALUE = 123456789;

  uint32_t VarSize() const {
    return Limit() / getBrigTypeNumBytes(VALUE_TYPE);
  }

protected:
  void EmitInitialization(TypedReg value) override {}

  TypedReg EmitValue() override {
    auto value = be.AddTReg(VALUE_TYPE);
    be.EmitMov(value, VALUE);
    return value;
  }

  Variable InitializeVariable() override {
    return function->NewVariable("var", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::AUTO, 
                                 BRIG_ALIGNMENT_NONE, VarSize());
  }

  uint32_t Limit() const override { 
    return LIMIT - getBrigTypeNumBytes(ResultType()); // part of arg space is occupied by result argument of function 
  }

  BrigSegment Segment() const override { return BRIG_SEGMENT_ARG; }

public:
  explicit ArgMemorySizeLimitTest(Grid geometry): MemorySegmentSizeLimitTest(Location::FUNCTION, geometry) {}

  void Name(std::ostream& out) const override {
    out << geometry;
  }

  BrigType ResultType() const override  { return VALUE_TYPE; }

  bool IsValid() const override {
    return MemorySegmentSizeLimitTest::IsValid()
        && VarSize() <= 16;  // ensure that we sutisfy assertion on TypedReg count limit
  }

  void ActualCallArguments(TypedRegList inputs, TypedRegList outputs) override {
    MemorySegmentSizeLimitTest::ActualCallArguments(inputs, outputs);
    auto value = EmitValue();
    auto reg = be.AddTReg(VALUE_TYPE, VarSize());
    for (uint32_t i = 0; i < VarSize(); ++i) {
      be.EmitMov(reg->Reg(i), value->Reg(), getBrigTypeNumBits(VALUE_TYPE));
    }
    inputs->Add(reg);
  }
};


class RegisterLimitBaseTest: public Test {
private:
  static const uint32_t VALUE = 123456789;

protected:
  virtual std::vector<TypedReg> CreateRegisters() = 0;

  virtual uint32_t GetValue() { return VALUE; }

  virtual void StoreValues(const std::vector<TypedReg>& registers, uint32_t value) {
    for (auto reg: registers) {
      be.EmitMov(reg, value);
    }
  }

  virtual void CompareValues(const std::vector<TypedReg>& registers, uint32_t value, 
                             const std::string& trueLabel, const std::string& falseLabel) {
    auto cmp = be.AddCTReg();
    for (auto reg: registers) {
      if (getBrigTypeNumBits(reg->Type()) == 128) {
        continue;
      }          
      be.EmitCmp(cmp->Reg(), reg, be.Immed(reg->Type(), value), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);
    }
    be.EmitBr(trueLabel);
  }

  virtual TypedReg ResultReg(const std::vector<TypedReg>& registers) {
    return registers[0];
  }

public:
  RegisterLimitBaseTest(Location codeLocation = Location::KERNEL, Grid geometry = 0): Test(codeLocation, geometry) {}

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(ResultType()), 1); }

  TypedReg Result() override {
    auto trueLabel = "@true";
    auto falseLabel = "@false";
    auto endLabel = "@end";
    
    // create registers
    auto registers = CreateRegisters();

    // store value in registers
    auto value = GetValue();
    StoreValues(registers, value);    

    // compare contents of registers with value
    CompareValues(registers, value, trueLabel, falseLabel);

    auto result = ResultReg(registers);
    be.EmitLabel(trueLabel);
    be.EmitMov(result, 1);
    be.EmitBr(endLabel);

    be.EmitLabel(falseLabel);
    be.EmitMov(result, (uint64_t) 0);

    be.EmitLabel(endLabel);
    return result;
  }
};


class RegistersLimitTest: public RegisterLimitBaseTest {
private:
  uint32_t typeSize;

  static const uint32_t LIMIT = 128;  // 128 s registers

protected:
  std::vector<TypedReg> CreateRegisters() override {
    std::vector<TypedReg> registers;
    registers.reserve(Limit());
    for (uint32_t i = 0; i < Limit(); ++i) {
      registers.push_back(be.AddTReg(RegisterType()));
    }
    return registers;
  }

  virtual uint32_t Limit() const {
    return LIMIT / (typeSize / 32);
  }

public:
  RegistersLimitTest(size_t typeSize_, Location codeLocation): 
            RegisterLimitBaseTest(codeLocation), typeSize(static_cast<uint32_t>(typeSize_)) {}

  void Name(std::ostream& out) const override {
    switch (typeSize) {
    case 32: out << "s_"; break;
    case 64: out << "d_"; break;
    case 128: out << "q_"; break;
    default: assert(false); break;
    }
    out << CodeLocationString();
  }

  bool IsValid() const override {
    return RegisterLimitBaseTest::IsValid()
      && (typeSize == 32 || typeSize == 64 || typeSize == 128);
  }

  BrigType ResultType() const override { return RegisterType(); }

  BrigType RegisterType() const {
    switch (typeSize) {
    case 32: return BRIG_TYPE_U32;
    case 64: return BRIG_TYPE_U64;
    case 128: return BRIG_TYPE_B128;
    default: assert(false); return BRIG_TYPE_NONE;
    }
  }
};


class LiveRegistersLimitTest: public RegistersLimitTest {
private:
  Variable buffer;

protected:
  virtual void LoadRegisters(const std::vector<TypedReg>& registers) {
    uint32_t offset = 0;
    for (uint32_t i = 0; i < registers.size(); ++i) {
      be.EmitLoad(BRIG_SEGMENT_GLOBAL, registers[i], be.Address(buffer->Variable(), offset));
      offset += getBrigTypeNumBytes(registers[i]->Type());
    }
  }

  virtual void StoreRegisters(const std::vector<TypedReg>& registers) {
    uint32_t offset = 0;
    for (uint32_t i = 0; i < registers.size(); ++i) {
      be.EmitStore(BRIG_SEGMENT_GLOBAL, registers[i], be.Address(buffer->Variable(), offset));
      offset += getBrigTypeNumBytes(registers[i]->Type());
    }
  }

public:
  LiveRegistersLimitTest(size_t typeSize, Location codeLocation): 
            RegistersLimitTest(typeSize, codeLocation) {}

  void Init() override {
    RegistersLimitTest::Init();
    buffer = module->NewVariable("buffer", BRIG_SEGMENT_GLOBAL, RegisterType(), Location::MODULE, BRIG_ALIGNMENT_NONE, Limit());
  }

  TypedReg Result() override {
    auto registers = CreateRegisters();

    // load from global buffer in each register
    LoadRegisters(registers);

    be.EmitBarrier();

    // store zeroes in global buffer
    auto bufferSize32 = (buffer->Dim32() * getBrigTypeNumBytes(buffer->Type())) / 4;
    for (uint32_t i = 0; i < bufferSize32; ++i) {
      auto storeType = be.MemOpType(BRIG_TYPE_U32);
      auto zero = be.Immed(storeType, 0);
      auto offset = i * getBrigTypeNumBytes(storeType);
      be.EmitStore(BRIG_SEGMENT_GLOBAL, storeType, zero, be.Address(buffer->Variable(), offset));
    }

    be.EmitBarrier();

    // store original values in global buffer
    StoreRegisters(registers);

    auto result = ResultReg(registers);
    be.EmitMov(result, 1);
    return result;
  }
};


class SDQRegistersLimitTest: public RegisterLimitBaseTest {
private:
  uint32_t sNumber;
  uint32_t dNumber;
  uint32_t qNumber;

  static const uint32_t LIMIT = 128;

protected:
  std::vector<TypedReg> CreateRegisters() override {
    std::vector<TypedReg> registers;
    registers.reserve(sNumber + dNumber + qNumber);
    for (uint32_t i = 0; i < sNumber; ++i) {
      registers.push_back(be.AddTReg(BRIG_TYPE_U32));
    }
    for (uint32_t i = 0; i < dNumber; ++i) {
      registers.push_back(be.AddTReg(BRIG_TYPE_U64));
    }
    for (uint32_t i = 0; i < qNumber; ++i) {
      registers.push_back(be.AddTReg(BRIG_TYPE_B128));
    }
    return registers;
  }

public:
  SDQRegistersLimitTest(Location codeLocation, uint32_t sNumber_ = 42, uint32_t dNumber_ = 21, uint32_t qNumber_ = 11):
    RegisterLimitBaseTest(codeLocation), sNumber(sNumber_), dNumber(dNumber_), qNumber(qNumber_) {}

  void Name(std::ostream& out) const override {
    out << "sdq_" << CodeLocationString();
  }

  bool IsValid() const override {
    return RegisterLimitBaseTest::IsValid()
      && ((sNumber + dNumber * 2 + qNumber * 4) <= LIMIT);
  }
};


class SDQLiveRegistersLimitTest: public LiveRegistersLimitTest {
private:
  uint32_t sNumber;
  uint32_t dNumber;
  uint32_t qNumber;

  static const uint32_t LIMIT = 128;

protected:
  std::vector<TypedReg> CreateRegisters() override {
    std::vector<TypedReg> registers;
    registers.reserve(sNumber + dNumber + qNumber);
    for (uint32_t i = 0; i < sNumber; ++i) {
      registers.push_back(be.AddTReg(BRIG_TYPE_U32));
    }
    for (uint32_t i = 0; i < dNumber; ++i) {
      registers.push_back(be.AddTReg(BRIG_TYPE_U64));
    }
    for (uint32_t i = 0; i < qNumber; ++i) {
      registers.push_back(be.AddTReg(BRIG_TYPE_B128));
    }
    return registers;
  }

public:
  SDQLiveRegistersLimitTest(Location codeLocation, uint32_t sNumber_ = 42, uint32_t dNumber_ = 21, uint32_t qNumber_ = 11):
    LiveRegistersLimitTest(32, codeLocation), sNumber(sNumber_), dNumber(dNumber_), qNumber(qNumber_) {}

  void Name(std::ostream& out) const override {
    out << "sdq_" << CodeLocationString();
  }

  bool IsValid() const override {
    return LiveRegistersLimitTest::IsValid()
      && ((sNumber + dNumber * 2 + qNumber * 4) <= LIMIT);
  }
};


class CRegistersLimitTest: public RegisterLimitBaseTest {
private:
  static const uint32_t LIMIT = 8;  // 8 c registers

protected:
  std::vector<TypedReg> CreateRegisters() override {
    std::vector<TypedReg> registers;
    registers.reserve(LIMIT);
    for (uint32_t i = 0; i < LIMIT; ++i) {
      registers.push_back(be.AddCTReg());
    }
    return registers;
  }

  uint32_t GetValue() override { return 1; }

  void CompareValues(const std::vector<TypedReg>& registers, uint32_t value, 
                     const std::string& trueLabel, const std::string& falseLabel) override {
    for (auto reg: registers) {
      be.EmitArith(BRIG_OPCODE_NOT, reg, reg->Reg());
      be.EmitCbr(reg->Reg(), falseLabel);
    }
    be.EmitBr(trueLabel);
  }

  TypedReg ResultReg(const std::vector<TypedReg>& registers) override {
    return be.AddTReg(ResultType());
  }

public:
  explicit CRegistersLimitTest(Location codeLocation): RegisterLimitBaseTest(codeLocation) {}

  void Name(std::ostream& out) const override {
    out << "c_" << CodeLocationString();
  }
};


class CLiveRegistersLimitTest: public LiveRegistersLimitTest {
private:
  std::vector<TypedReg> cRegisters;

  static const uint32_t LIMIT = 8;  // 8 c registers
  
protected:
  std::vector<TypedReg> CreateRegisters() override {
    cRegisters.reserve(Limit());
    for (uint32_t i = 0; i < Limit(); ++i) {
      cRegisters.push_back(be.AddCTReg());
    }
    return LiveRegistersLimitTest::CreateRegisters();
  }

  uint32_t GetValue() override { return 1; }

  TypedReg ResultReg(const std::vector<TypedReg>& registers) override {
    return be.AddTReg(ResultType());
  }

  uint32_t Limit() const override {
    return LIMIT;
  }

  void LoadRegisters(const std::vector<TypedReg>& registers) override {
    LiveRegistersLimitTest::LoadRegisters(registers);
    for (uint32_t i = 0; i < Limit(); ++i) {
      be.EmitCvt(cRegisters[i], registers[i]);
    }
  } 

  virtual void StoreRegisters(const std::vector<TypedReg>& registers) override {
    for (uint32_t i = 0; i < Limit(); ++i) {
      be.EmitCvt(registers[i], cRegisters[i]);
    }
    LiveRegistersLimitTest::StoreRegisters(registers);
  }

public:
  explicit CLiveRegistersLimitTest(Location codeLocation): LiveRegistersLimitTest(32, codeLocation) {}

  void Name(std::ostream& out) const override {
    out << "c_" << CodeLocationString();
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

  TestForEach<GroupMemorySizeLimitTest>(ap, it, "group_memory_size", cc->Grids().SingleGroupSet());
  TestForEach<PrivateMemorySizeLimitTest>(ap, it, "private_memory_size", cc->Grids().WorkGroupsSize256());
  TestForEach<KernargMemorySizeLimitTest>(ap, it, "kernarg_memory_size", cc->Grids().SimpleSet());
  TestForEach<ArgMemorySizeLimitTest>(ap, it, "arg_memory_size", cc->Grids().SimpleSet());

  TestForEach<RegistersLimitTest>(ap, it, "registers", cc->Types().RegisterSizes(), CodeLocations());
  TestForEach<SDQRegistersLimitTest>(ap, it, "registers", CodeLocations());
  TestForEach<CRegistersLimitTest>(ap, it, "registers", CodeLocations());

  TestForEach<LiveRegistersLimitTest>(ap, it, "registers/live", cc->Types().RegisterSizes(), CodeLocations());
  TestForEach<SDQLiveRegistersLimitTest>(ap, it, "registers/live", CodeLocations());
  TestForEach<CLiveRegistersLimitTest>(ap, it, "registers/live", CodeLocations());
}

}
