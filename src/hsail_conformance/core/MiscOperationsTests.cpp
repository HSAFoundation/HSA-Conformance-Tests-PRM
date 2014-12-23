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

#include "MiscOperationsTests.hpp"
#include "HCTests.hpp"
#include "BrigEmitter.hpp"
#include "CoreConfig.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace Brig;
using namespace HSAIL_ASM;

namespace hsail_conformance {

class KernargBasePtrIdentityTest : public Test {
private:
  static const uint32_t argValue = 156;
  Variable testArg;

public:
  KernargBasePtrIdentityTest(Location codeLocation)
    : Test(codeLocation) { }

  void Init() {
    Test::Init();
    testArg = te->NewVariable("testArg", BRIG_SEGMENT_KERNARG, BRIG_TYPE_U32);
  }

  void Name(std::ostream& out) const {
    out << CodeLocationString();
  }

  void KernelArguments() {
    // Ensure testArg is the first argument.
    specList.KernelArguments();
    hexl::EmittedTest::KernelArguments();
  }

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }
  Value ExpectedResult() const { return Value(MV_UINT32, argValue); }

  TypedReg Result()
  {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    PointerReg kab = be.AddAReg(BRIG_SEGMENT_KERNARG);
    be.EmitKernargBasePtr(kab);
    be.EmitLoad(result, kab);
    return result;
  }

  void SetupDispatch(DispatchSetup* dsetup)
  {
    dsetup->MSetup().Add(NewMValue(dsetup->MSetup().Count(), "Test arg", MEM_KERNARG, MV_UINT32, U32(argValue)));
    Test::SetupDispatch(dsetup);
  }
};


class KernargBasePtrAlignmentTest : public Test {
private:
  VariableSpec varSpec;
  Variable var;

public:
  KernargBasePtrAlignmentTest(VariableSpec varSpec_)
    : varSpec(varSpec_), var(0) { }

  void Name(std::ostream& out) const {
    out << varSpec;
  }

  void Init() {
    Test::Init();
    if (varSpec) { var = kernel->NewVariable("var", varSpec); }
  }

  BrigTypeX ResultType() const { return be.PointerType(BRIG_SEGMENT_KERNARG); }
  Value ExpectedResult() const { return Value(Brig2ValueType(ResultType()), U64(1)); }

  TypedReg Result()
  {
    TypedReg result = be.AddAReg(BRIG_SEGMENT_KERNARG);
    PointerReg kab = be.AddAReg(BRIG_SEGMENT_KERNARG);
    be.EmitKernargBasePtr(kab);
    unsigned align = var ? align2num(var->Align()) : 16;
    be.EmitArith(BRIG_OPCODE_REM, result, kab, be.Brigantine().createImmed(align, kab->Type()));
    be.EmitArith(BRIG_OPCODE_ADD, result, result, be.Immed(result->Type(), 1)); 
    return result;
  }

  bool IsValid() const
  {
    return Test::IsValid() && varSpec->IsValid();
  }
};


class GroupBasePtrIdentityTest : public Test {
private:
  Buffer inArg;
  bool emitControlDirective;
  uint32_t testValue;

protected:
  virtual PointerReg StoreAddress(PointerReg groupBase) = 0;
  virtual PointerReg LoadAddress(PointerReg groupBase) { return StoreAddress(groupBase); }
  virtual size_t DynamicMemorySize() { return 0; }

public:
  explicit GroupBasePtrIdentityTest(Location codeLocation_, bool emitControlDirective_, uint32_t testValue_ = 156) 
    : Test(codeLocation_), emitControlDirective(emitControlDirective_), testValue(testValue_) {}

  void Name(std::ostream& out) const override {
    out << CodeLocationString() << (emitControlDirective ? "_MDGS" : "_ND");
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, testValue); }

  void Init() override {
    Test::Init();
    inArg = kernel->NewBuffer("input", HOST_INPUT_BUFFER, MV_UINT32, 1);
    inArg->AddData(Value(MV_UINT32, testValue));
  }

  TypedReg Result() override {
    // read input value
    auto in = be.AddTReg(BRIG_TYPE_U32);
    inArg->EmitLoadData(in, false);

    // get group base pointer
    auto groupBase = be.AddAReg(BRIG_SEGMENT_GROUP);
    be.EmitGroupBasePtr(groupBase);

    auto storeAddr = StoreAddress(groupBase);
    auto loadAddr = LoadAddress(groupBase);

    // store input value group memory at groupAddr
    be.EmitStore(in, storeAddr);

    // load input value from group memory
    auto out = be.AddTReg(BRIG_TYPE_U32);
    be.EmitLoad(out, loadAddr);
    return out;
  }

  void FunctionDirectives() {
    Test::FunctionDirectives();
    if (codeLocation == FUNCTION && emitControlDirective) {
      be.EmitDynamicMemoryDirective(DynamicMemorySize());
    }
  }

  void KernelDirectives() {
    Test::KernelDirectives();
    if (codeLocation == KERNEL && emitControlDirective) {
      be.EmitDynamicMemoryDirective(DynamicMemorySize());
    }
  }
};


class GroupBasePtrStaticMemoryIdentityTest : public GroupBasePtrIdentityTest {
private:
  static const uint32_t offsetValue = 0;
  Variable buffer;

protected:
  PointerReg StoreAddress(PointerReg groupBase) override {
    return groupBase;
  }

  PointerReg LoadAddress(PointerReg groupBase) override {
    auto bufAddr = be.AddAReg(buffer->Segment());
    be.EmitLda(bufAddr, be.Address(buffer->Variable(), 0));
    return bufAddr;
  }

public:
  explicit GroupBasePtrStaticMemoryIdentityTest(Location codeLocation, bool emitControlDirective_) 
    : GroupBasePtrIdentityTest(codeLocation, emitControlDirective_), buffer(0)
  { }

  void Init() {
    GroupBasePtrIdentityTest::Init();
    buffer = kernel->NewVariable("buffer", BRIG_SEGMENT_GROUP, BRIG_TYPE_U32, MODULE);
  }

  void ModuleVariables() {
    GroupBasePtrIdentityTest::ModuleVariables();
    buffer->ModuleVariables();
  }

};


class GroupBasePtrDynamicMemoryIdentityTest : public GroupBasePtrIdentityTest {
private:
  static const uint32_t offsetValue = 0;
  Variable offsetArg;

  PointerReg groupAddr;

  PointerReg GroupAddress(PointerReg groupBase) {
    // get dynamic group memory offset
    auto offset = be.AddAReg(BRIG_SEGMENT_GROUP);
    offsetArg->EmitLoadTo(offset);

    // get address of dynamic group memory
    auto dynamicAddr = be.AddAReg(BRIG_SEGMENT_GROUP);
    be.EmitArith(BRIG_OPCODE_ADD, dynamicAddr, groupBase, offset->Reg());
    return dynamicAddr;
  }

protected:
  PointerReg StoreAddress(PointerReg groupBase) {
    if (groupAddr == nullptr) {
      groupAddr = GroupAddress(groupBase);
    }
    return groupAddr;
  }
  
  PointerReg LoadAddress(PointerReg groupBase) {
    if (groupAddr == nullptr) {
      groupAddr = GroupAddress(groupBase);
    }
    return groupAddr;
  }

  size_t DynamicMemorySize() override {
    return getBrigTypeNumBytes(BRIG_TYPE_U32);
  }

public:
  explicit GroupBasePtrDynamicMemoryIdentityTest(Location codeLocation, bool emitControlDirective_) 
    : GroupBasePtrIdentityTest(codeLocation, emitControlDirective_), groupAddr(nullptr) 
  {
  }

  void Init() {
    GroupBasePtrIdentityTest::Init();
    offsetArg = kernel->NewVariable("offset", BRIG_SEGMENT_KERNARG, BRIG_TYPE_U32);
  }

  void SetupDispatch(DispatchSetup* dsetup) override {
    Test::SetupDispatch(dsetup);
    auto offsetType = Brig2ValueType(offsetArg->Type());
    dsetup->MSetup().Add(NewMValue(dsetup->MSetup().Count(), "Offset arg", MEM_KERNARG, 
      offsetType, U64(0)));
    dsetup->MSetup().Add(NewMValue(dsetup->MSetup().Count(), "Dynamic memory", MEM_GROUP, 
      MV_UINT32, U32(0)));
  }
};


class GroupBasePtrAlignmentTest : public Test {
private:
  VariableSpec firstVarSpec, secondVarSpec;
  Variable firstVar, secondVar;

public:
  GroupBasePtrAlignmentTest(VariableSpec firstVarSpec_, VariableSpec secondVarSpec_)
    : firstVarSpec(firstVarSpec_), secondVarSpec(secondVarSpec_) { }

  void Name(std::ostream& out) const override {
    out << firstVarSpec << "__" << secondVarSpec;
  }

  BrigTypeX ResultType() const override { return be.PointerType(BRIG_SEGMENT_GROUP); }
  Value ExpectedResult() const override { return Value(Brig2ValueType(ResultType()), U64(0)); }

  void Init() {
    Test::Init();
    firstVar = kernel->NewVariable("var1", firstVarSpec);
    secondVar = kernel->NewVariable("var2", secondVarSpec);
  }

  TypedReg Result() override {
    // groupbaseptr
    auto groupBase = be.AddAReg(BRIG_SEGMENT_GROUP);
    be.EmitGroupBasePtr(groupBase);
    // maximum alignment
    auto firstAlign = firstVar->AlignNum();
    auto secondAlign = secondVar->AlignNum();
    auto maxAlign = std::max(firstAlign, secondAlign);
    // groupBase alignment
    auto result = be.AddAReg(BRIG_SEGMENT_GROUP);
    be.EmitArith(BRIG_OPCODE_REM, result, groupBase, be.Immed(groupBase->Type(), maxAlign));
    return result;
  }

  bool IsValid() const override {
    return Test::IsValid() 
        && firstVarSpec->IsValid()
        && secondVarSpec->IsValid();
  }
};



class NopTest : public Test {
public:
  explicit NopTest(Location codeLocation) : Test(codeLocation) {}

  void Name(std::ostream& out) const override { 
    out << CodeLocationString();
  }

  void Init() override {
    cc = hexl::emitter::CoreConfig::Get(context.get());
    geometry = cc->Grids().DefaultGeometry();
    kernel = te->NewKernel("test_kernel");
    if (codeLocation == emitter::FUNCTION) {
      function = te->NewFunction("test_function");
    }
  }

  void KernelCode() override {
    if (codeLocation == Location::KERNEL) {
      be.EmitNop();
    } else if (codeLocation == Location::FUNCTION) {
      auto kernArgInRegs = te->Brig()->AddTRegList();
      auto kernArgOutRegs = te->Brig()->AddTRegList();
      te->Brig()->EmitCallSeq(function->Directive(), kernArgInRegs, kernArgOutRegs);
    }
  }

  void FunctionCode() override {
    if (codeLocation == Location::FUNCTION) {
      be.EmitNop();
    }
  }

  void SetupDispatch(DispatchSetup* dispatch) override {
    dispatch->SetDimensions(geometry->Dimensions());
    dispatch->SetWorkgroupSize(geometry->WorkgroupSize(0), geometry->WorkgroupSize(1), geometry->WorkgroupSize(2));
    dispatch->SetGridSize(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2));
    kernel->SetupDispatch(dispatch);
  }

}; 


class ClockMonotonicTest : public Test {
private:
  Buffer input;
  
public:
  ClockMonotonicTest(Location codeLocation, Grid geometry)
    : Test(codeLocation, geometry) { }

  BrigTypeX ResultType() const { return BRIG_TYPE_U64; }

  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry;
  }
  
  void Init() {
    Test::Init();
    input = kernel->NewBuffer("input", HOST_INPUT_BUFFER, MV_DOUBLE, geometry->GridSize());
    for (uint64_t i = 0; i < geometry->GridSize(); ++i) {
      input->AddData(Value(MV_DOUBLE, D((double) i)));
    }
  }

  unsigned GetCycles() const {  return 64; }
  
  void ExpectedResults(Values* result) const {
    for(uint16_t i = 0; i < GetCycles(); i++) {
      result->push_back(Value(MV_UINT64, 372));
    }
  }

  bool IsValid() const
  {
    return (codeLocation != FUNCTION);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    TypedReg cnt = be.AddTReg(BRIG_TYPE_U64);
    TypedReg clk = be.AddTReg(BRIG_TYPE_U64);
    TypedReg old_val = be.AddTReg(BRIG_TYPE_U64);
    //mov
    be.EmitMov(cnt, be.Immed(cnt->Type(), 0));
    be.EmitMov(clk, be.Immed(cnt->Type(), 0));
    be.EmitMov(result, be.Immed(cnt->Type(), 0));
    TypedReg reg_c = be.AddTReg(BRIG_TYPE_B1);
    SRef s_label_do = "@do";
    SRef s_label_until = "@until";
    //do:
    be.Brigantine().addLabel(s_label_do);
    //cmp_ge c0, s0, n
    be.EmitCmp(reg_c->Reg(), cnt, be.Immed(cnt->Type(), GetCycles()), BRIG_COMPARE_GE);
    //cbr c0, @until
    be.EmitCbr(reg_c, s_label_until);
    PointerReg addrReg = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    TypedReg data = input->AddDataReg();
    // Load input
    input->EmitLoadData(data, cnt);
    TypedReg sqrt = be.AddTReg(BRIG_TYPE_F64);
    //mov
    be.EmitMov(old_val, clk);
    //clock
    be.EmitClock(clk);
    //sqrt d, d
    be.EmitArith(BRIG_OPCODE_SQRT, sqrt, data->Reg());
    //cmp_ge
    TypedReg idx = be.AddTReg(BRIG_TYPE_U64);
    be.EmitCmpTo(idx, clk, old_val->Reg(), BRIG_COMPARE_GE);
    TypedReg cvt = be.AddTReg(BRIG_TYPE_U64);
    //cvt float to int
    be.EmitCvt(cvt, sqrt, BRIG_ROUND_INTEGER_ZERO);
    //add
    be.EmitArith(BRIG_OPCODE_ADD, cvt, cvt, idx->Reg());
    be.EmitArith(BRIG_OPCODE_ADD, result, result, cvt->Reg());
    //add
    be.EmitArith(BRIG_OPCODE_ADD, cnt, cnt, be.Immed(cnt->Type(), 1));
    //br @do
    be.EmitBr(s_label_do);
    //until:
    be.Brigantine().addLabel(s_label_until);
    return result;
  }
};

class LessEqMaximumText: public Test {
private:
  BrigTypeX type;

protected:
  virtual TypedReg EmitValue() = 0;
  virtual TypedReg EmitMaxValue() = 0;

public:
  LessEqMaximumText(
    BrigTypeX type_,
    Location codeLocation_, 
    Grid geometry_) 
    : Test(codeLocation_, geometry_), type(type_)  { }
      
  void Name(std::ostream& out) const override {
    out << CodeLocationString() << "_" << geometry;
  }

  BrigTypeX ResultType() const override { return type; }

  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  TypedReg Result() override {
    auto value = EmitValue();
    auto maximum = EmitMaxValue();
    // compare value with maximum (value <= maximum)
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp->Reg(), value, maximum, BRIG_COMPARE_LE);
    // covert b1 to u32
    auto result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCvt(result, cmp);
    return result;
  }
};

class CuidLessMaxTest: public LessEqMaximumText {
public:
  CuidLessMaxTest(
    Location codeLocation_, 
    Grid geometry_) 
    : LessEqMaximumText(BRIG_TYPE_U32, codeLocation_, geometry_) {}

  virtual TypedReg EmitValue() {
    auto cuid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCuid(cuid);
    return cuid;
  }

  virtual TypedReg EmitMaxValue() {
    auto maxcuid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMaxcuid(maxcuid);
    return maxcuid;
  }
};


class BufferIdentityTest: public Test {
protected:
  BrigSegment bufferSegment;
  BrigTypeX compareType;
  uint64_t size;
  Variable buffer;

  bool IsGlobal() const { return buffer->Segment() == BRIG_SEGMENT_GLOBAL; }

protected:
  BrigTypeX CompareType() { return buffer->Type(); }
  virtual TypedReg EmitCompareValue() = 0;
  virtual TypedReg EmitWorkItemId() = 0;

  virtual TypedReg EmitIsFirst(TypedReg wiId) {
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp->Reg(), wiId, be.Immed(wiId->Type(), 0), BRIG_COMPARE_EQ);
    return cmp;
  }

  virtual void EmitBufferInitialization(PointerReg bufAddr, uint32_t numBytes) { }

  virtual TypedReg EmitPrev(PointerReg bufAddr, PointerReg storeAddr, uint32_t numBytes) {
    // load compare value for previous work-item
    auto prevVal = be.AddTReg(CompareType());
    auto loadAddr = be.AddAReg(buffer->Segment());
    be.EmitArith(BRIG_OPCODE_SUB, loadAddr, storeAddr, be.Immed(loadAddr->Type(), numBytes));
    be.EmitLoad(prevVal, loadAddr);

    return prevVal;
  }

public:
  BufferIdentityTest(
    Location codeLocation_, 
    Grid geometry_,
    BrigSegment bufferSegment_,
    BrigTypeX compareType_,
    uint64_t size_)
    : Test(codeLocation_, geometry_),
      bufferSegment(bufferSegment_),
      compareType(compareType_),
      size(size_), buffer(0)
  {
  }

  BrigTypeX CompareType() const { return compareType; }

  void Name(std::ostream& out) const override {
    out << CodeLocationString() << "_" << geometry;
  }

  void Init() {
    Test::Init();
    buffer = kernel->NewVariable("buffer", bufferSegment, compareType, MODULE, BRIG_ALIGNMENT_NONE, size);
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }

  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  bool IsValid() const override {
    auto numBytes = getBrigTypeNumBytes(CompareType());
    return geometry->GridSize() < (UINT32_MAX / numBytes);
  }

  TypedReg Result() override {
    SRef returnLabel = "@return";

    // address of buffer to store compare values
    auto bufAddr = be.AddAReg(buffer->Segment());
    be.EmitLda(bufAddr, be.Address(buffer->Variable(), 0));

    auto numBytes = getBrigTypeNumBytes(CompareType());
    EmitBufferInitialization(bufAddr, numBytes);

    // a value to store in buffer and to compare
    auto compareVal = EmitCompareValue();
    assert(compareVal->Type() == CompareType());
    
    //work-item id
    auto wiId = EmitWorkItemId();

    // store compare vales for all work-items
    auto storeAddr = be.AddAReg(buffer->Segment());
    if (wiId->Type() != storeAddr->Type()) {
      be.EmitCvt(storeAddr, wiId);
    } else {
      be.EmitMov(storeAddr, wiId);
    }
    be.EmitArith(BRIG_OPCODE_MUL, storeAddr, storeAddr, be.Immed(storeAddr->Type(), numBytes));
    be.EmitArith(BRIG_OPCODE_ADD, storeAddr, storeAddr, bufAddr->Reg());
    be.EmitStore(compareVal, storeAddr);

    // barrier
    be.EmitBarrier();

    // if first work item then always return 1
    auto cmp = EmitIsFirst(wiId);
    assert(cmp->Type() == BRIG_TYPE_B1);
    be.EmitCbr(cmp, returnLabel);

    // compare value from previous work-item
    auto prevVal = EmitPrev(bufAddr, storeAddr, numBytes);
    
    // compare current value to previous
    be.EmitCmp(cmp->Reg(), compareVal, prevVal, BRIG_COMPARE_EQ);

    // convert b1 to u32
    be.EmitLabel(returnLabel);
    auto result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCvt(result, cmp);

    return result;
  }

  void ModuleVariables() {
    Test::ModuleVariables();
    buffer->ModuleVariables();
  }

};


class GroupBufferIdentityTest: public BufferIdentityTest {
protected:
  virtual TypedReg EmitWorkItemId() override {
    return be.EmitWorkitemFlatId();
  }
  
  virtual void EmitBufferInitialization(PointerReg bufAddr, uint32_t numBytes) override {
    // initialize buffer with max value
    // initialization is executed only in first work-item in loop;
    SRef skipLabel = "@skip_initialize";
    SRef doLabel = "@do_initialize";

    auto wiId = EmitWorkItemId();
    auto cmp = be.AddCReg();
    be.EmitCmp(cmp, wiId, be.Immed(BRIG_TYPE_U32, 0), BRIG_COMPARE_NE);
    be.EmitCbr(cmp, skipLabel);

    auto count = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(count, be.Immed(BRIG_TYPE_U32, 0));
    
    // loop
    be.EmitLabel(doLabel);
    
    // store max value in buffer
    auto storeAddr = be.AddAReg(buffer->Segment());
    if (wiId->Type() != storeAddr->Type()) {
      be.EmitCvt(storeAddr, count);
    } else {
      be.EmitMov(storeAddr, count);
    }
    be.EmitArith(BRIG_OPCODE_MUL, storeAddr, storeAddr, be.Immed(storeAddr->Type(), numBytes));
    be.EmitArith(BRIG_OPCODE_ADD, storeAddr, storeAddr, bufAddr->Reg());
    auto maxReg = be.AddTReg(CompareType());
    be.EmitMov(maxReg, be.Immed(CompareType(), UINT64_MAX));
    be.EmitStore(maxReg, storeAddr);

    // check for end of loop
    be.EmitArith(BRIG_OPCODE_ADD, count, count, be.Immed(count->Type(), 1));
    be.EmitCmp(cmp, count, be.Immed(count->Type(), buffer->Dim()), BRIG_COMPARE_LT);
    be.EmitCbr(cmp, doLabel);

    be.EmitLabel(skipLabel);
  }

  virtual TypedReg EmitPrev(PointerReg bufAddr, PointerReg storeAddr, uint32_t numBytes) override {
    SRef doLabel = "@do_prev";

    // load compare value for previous work-item
    auto prevVal = be.AddTReg(CompareType());
    auto loadAddr = be.AddAReg(buffer->Segment());
    be.EmitMov(loadAddr, storeAddr);
    
    // loop
    be.EmitLabel(doLabel);

    be.EmitArith(BRIG_OPCODE_SUB, loadAddr, loadAddr, be.Immed(loadAddr->Type(), numBytes));
    be.EmitLoad(prevVal, loadAddr);

    auto cmp1 = be.AddCTReg();
    be.EmitCmpTo(cmp1, prevVal, be.Immed(CompareType(), UINT32_MAX), BRIG_COMPARE_EQ);

    auto cmp2 = be.AddCTReg();
    be.EmitCmpTo(cmp2, loadAddr, bufAddr->Reg(), BRIG_COMPARE_GT);
    be.EmitArith(BRIG_OPCODE_AND, cmp1, cmp1, cmp2->Reg());

    be.EmitCbr(cmp1, doLabel);
    return prevVal;
  }

public:
  GroupBufferIdentityTest(
    Location codeLocation_, 
    Grid geometry_,
    BrigTypeX compareType_ = BRIG_TYPE_U32) 
    : BufferIdentityTest(
        codeLocation_, 
        geometry_,
        BRIG_SEGMENT_GROUP, 
        compareType_, 
        geometry_->WorkgroupSize()) { }


  bool IsValid() const override {
    auto numBytes = getBrigTypeNumBytes(CompareType());
    return geometry->WorkgroupSize() < (UINT32_MAX / numBytes);
  }
};


class GlobalBufferIdentityTest: public BufferIdentityTest {
protected:
  virtual TypedReg EmitWorkItemId() override {
    return be.EmitWorkitemFlatAbsId(false);
  }
  
public:
  GlobalBufferIdentityTest(
    Location codeLocation_, 
    Grid geometry_,
    BrigTypeX compareType_ = BRIG_TYPE_U32) 
    : BufferIdentityTest(
        codeLocation_, 
        geometry_, 
        BRIG_SEGMENT_GLOBAL, 
        compareType_, 
        geometry_->GridSize())  {}
};


class CuidIdentityTest: public GroupBufferIdentityTest {
protected:
  virtual TypedReg EmitCompareValue() override {
    auto cuid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCuid(cuid);
    return cuid;
  }

public:
  CuidIdentityTest(Location codeLocation_, Grid geometry_) 
    : GroupBufferIdentityTest(codeLocation_, geometry_, BRIG_TYPE_U32) {}
};


class MaxcuidIdentityTest: public GlobalBufferIdentityTest {
protected:
  virtual TypedReg EmitCompareValue() override {
    auto maxcuid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMaxcuid(maxcuid);
    return maxcuid;
  }

public:
  MaxcuidIdentityTest(Location codeLocation_, Grid geometry_) 
    : GlobalBufferIdentityTest(codeLocation_, geometry_, BRIG_TYPE_U32) {}
};

class WaveidLessMaxTest: public LessEqMaximumText {
public:
  WaveidLessMaxTest(
    Location codeLocation_, Grid geometry_) 
    : LessEqMaximumText(BRIG_TYPE_U32, codeLocation_, geometry_) {}

  virtual TypedReg EmitValue() {
    auto waveid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitWaveid(waveid);
    return waveid;
  }

  virtual TypedReg EmitMaxValue() {
    auto maxwaveid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMaxwaveid(maxwaveid);
    return maxwaveid;
  }
};


class WaveidIdentityTest: public GroupBufferIdentityTest {
protected:
  virtual TypedReg EmitCompareValue() override {
    auto waveid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitWaveid(waveid);
    return waveid;
  }

  virtual TypedReg EmitIsFirst(TypedReg wiId) {
    // reminder of (work-item flat id) / (WAVESIZE)
    auto wavesize = be.Wavesize();
    auto rem = be.AddTReg(BRIG_TYPE_U32);
    be.EmitArith(BRIG_OPCODE_REM, rem, wiId, wavesize);

    // compare reminder with 0
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp->Reg(), rem, be.Immed(BRIG_TYPE_U32, 0), BRIG_COMPARE_EQ);
    return cmp;
  }

public:
  WaveidIdentityTest(Location codeLocation_, Grid geometry_) 
    : GroupBufferIdentityTest(codeLocation_, geometry_, BRIG_TYPE_U32) {}
};


class MaxwaveidIdentityTest: public GlobalBufferIdentityTest {
protected:
  virtual TypedReg EmitCompareValue() override {
    auto maxwaveid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMaxwaveid(maxwaveid);
    return maxwaveid;
  }

public:
  MaxwaveidIdentityTest(Location codeLocation_, Grid geometry_) 
    : GlobalBufferIdentityTest(codeLocation_, geometry_, BRIG_TYPE_U32) {}
};


class LaneidLessWavesizeTest: public LessEqMaximumText {
public:
  LaneidLessWavesizeTest(
    Location codeLocation_, 
    Grid geometry_) 
    : LessEqMaximumText(BRIG_TYPE_U32, codeLocation_, geometry_) {}

  virtual TypedReg EmitValue() {
    auto laneid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitLaneid(laneid);
    return laneid;
  }

  virtual TypedReg EmitMaxValue() {
    auto wavesize = be.Wavesize();
    auto result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitArith(BRIG_OPCODE_SUB, result, wavesize, be.Immed(BRIG_TYPE_U32, 1)); 
    return result;
  }
};

class LaneidSequenceTest: public Test {
public:
  LaneidSequenceTest(
    Location codeLocation_, 
    Grid geometry_) 
    : Test(codeLocation_, geometry_) {}

  void Name(std::ostream& out) const override {
    out << CodeLocationString() << "_" << geometry;
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }

  void ExpectedResults(Values* result) const override { 
    // number of work-groups
    auto groupsNum = geometry->GridGroups(0) * geometry->GridGroups(1) * geometry->GridGroups(2);

    // vector of counters for work-groups
    std::vector<uint32_t> workgroups;
    workgroups.reserve(groupsNum);
    for (unsigned i = 0; i < groupsNum; ++i) {
      workgroups.push_back(0);
    }

    // initializing expected results
    for (uint32_t z = 0; z < geometry->GridSize(2); z++) {
      for (uint32_t y = 0; y < geometry->GridSize(1); y++) {
        for (uint32_t x = 0; x < geometry->GridSize(0); x++) {
          Dim point(x, y, z);
          
          auto wgId = geometry->WorkgroupFlatId(point);
          result->push_back(Value(MV_UINT32, workgroups[wgId]));
          ++ workgroups[wgId];
        }
      }
    }
  }

  TypedReg Result() override {
    auto laneid = be.AddTReg(BRIG_TYPE_U32);
    be.EmitLaneid(laneid);
    return laneid;
  }
};



void MiscOperationsTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string path;
  Arena* ap = cc->Ap();
  TestForEach<KernargBasePtrIdentityTest>(ap, it, "misc/kernargbaseptr/identity", CodeLocations());
  TestForEach<KernargBasePtrAlignmentTest>(ap, it, "misc/kernargbaseptr/alignment", cc->Variables().ByTypeAlign(BRIG_SEGMENT_KERNARG));

  TestForEach<GroupBasePtrStaticMemoryIdentityTest>(ap, it, "misc/groupbaseptr/static", KernelLocation(), Bools::All());  
  TestForEach<GroupBasePtrDynamicMemoryIdentityTest>(ap, it, "misc/groupbaseptr/dynamic", KernelLocation(), Bools::All());
  TestForEach<GroupBasePtrAlignmentTest>(ap, it, "misc/groupbaseptr/alignment", cc->Variables().ByTypeAlign(BRIG_SEGMENT_GROUP), cc->Variables().ByTypeAlign(BRIG_SEGMENT_GROUP));
  
  TestForEach<NopTest>(ap, it, "misc/nop", CodeLocations());
  
  TestForEach<ClockMonotonicTest>(ap, it, "misc/clock/monotonic", CodeLocations(), cc->Grids().All());

  TestForEach<CuidLessMaxTest>(ap, it, "misc/cuid/lessmax", CodeLocations(), cc->Grids().All());
  TestForEach<CuidIdentityTest>(ap, it, "misc/cuid/identity", CodeLocations(), cc->Grids().All());
  
  TestForEach<MaxcuidIdentityTest>(ap, it, "misc/maxcuid/identity", CodeLocations(), cc->Grids().All());

  TestForEach<WaveidLessMaxTest>(ap, it, "misc/waveid/lessmax", CodeLocations(), cc->Grids().All());  
  TestForEach<WaveidIdentityTest>(ap, it, "misc/waveid/identity", CodeLocations(), cc->Grids().All());

  TestForEach<MaxwaveidIdentityTest>(ap, it, "misc/maxwaveid/identity", CodeLocations(), cc->Grids().All());

  TestForEach<LaneidLessWavesizeTest>(ap, it, "misc/laneid/lessmax", CodeLocations(), cc->Grids().All());
  TestForEach<LaneidSequenceTest>(ap, it, "misc/laneid/sequence", CodeLocations(), cc->Grids().All());
}

}