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

#include "FunctionsTests.hpp"
#include "Emitter.hpp"
#include "BrigEmitter.hpp"
#include "RuntimeContext.hpp"
#include "HCTests.hpp"
#include <sstream>

using namespace hexl;
using namespace hexl::emitter;
using namespace HSAIL_ASM;

namespace hsail_conformance {

class FunctionArguments : public Test {
private:
  VariableSpec argSpec;
  Variable functionArg;
  Buffer input;
  bool useVectorInstructionsForFormals;
  bool useVectorInstructionsForActuals;

public:
  FunctionArguments(VariableSpec argSpec_,
    bool useVectorInstructionsForFormals_, bool useVectorInstructionsForActuals_)
    : Test(emitter::FUNCTION),
      argSpec(argSpec_),
      useVectorInstructionsForFormals(useVectorInstructionsForFormals_),
      useVectorInstructionsForActuals(useVectorInstructionsForActuals_)
  {
  }

  void Name(std::ostream& out) const {
    out << argSpec << "_"
        << (useVectorInstructionsForFormals ? "v" : "s")
        << (useVectorInstructionsForActuals ? "v" : "s");
  }

  bool IsValid() const {
    if (!Test::IsValid()) { return false; }
    if (!argSpec->IsValid()) { return false; }
    if (useVectorInstructionsForFormals && !argSpec->IsArray()) { return false; }
    if (useVectorInstructionsForActuals && !argSpec->IsArray()) { return false; }
    return true;
  }

  void Init() {
    Test::Init();
    functionArg = function->NewVariable("in", argSpec, false);
    input = kernel->NewBuffer("input", HOST_INPUT_BUFFER, argSpec->VType(), OutputBufferSize());
    for (unsigned i = 0; i < input->Count(); ++i) { input->AddData(Value(argSpec->VType(), i)); }
  }

  TypedReg Result() {
    TypedReg in = functionArg->AddDataReg();
    TypedReg out = functionResult->AddDataReg();
    functionArg->EmitLoadTo(in);
    be.EmitMov(out, in);
    return out;
  }

  void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs)
  {
    TypedReg indata = functionArg->AddDataReg();
    input->EmitLoadData(indata);
    Test::ActualCallArguments(inputs, outputs);
    inputs->Add(indata);
  }

  BrigType ResultType() const {
    return argSpec->Type();
  }

  uint64_t ResultDim() const {
    return argSpec->Dim();
  }

  Value ExpectedResult(uint64_t wi, uint64_t pos) const {
    return Value(argSpec->VType(), wi * ResultCount() + pos);
  }
};

class RecursiveFactorial : public Test {
private:
  BrigType type;
  Variable functionArg;
  Buffer input;

public:
  RecursiveFactorial(BrigType type_)
    : Test(emitter::FUNCTION),
      type(type_) { }

  void Name(std::ostream& out) const {
    out << type2str(type);
  }

  void Init() {
    Test::Init();
    ValueType vtype = Brig2ValueType(type);
    functionArg = function->NewVariable("n", BRIG_SEGMENT_ARG, type);
    input = kernel->NewBuffer("input", HOST_INPUT_BUFFER, vtype, geometry->GridSize());
    for (unsigned wi = 0; wi < input->Count(); ++wi) { input->AddData(Value(vtype, InputValue(wi))); }
  }

  uint64_t InputValue(uint64_t wi) const {
    return wi % 11;
  }

  uint64_t Factorial(uint64_t n) const {
    assert(n < 15);
    uint64_t f = 1;
    for (uint64_t i = 1; i <= n; ++i) {
      f *= i;
    }
    return f;
  }

  BrigType ResultType() const override { return type; }

  Value ExpectedResult(uint64_t wi) const {
    return Value(Brig2ValueType(type), Factorial(InputValue(wi)));
  }

  void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs)
  {
    TypedReg indata = functionArg->AddDataReg();
    input->EmitLoadData(indata);
    Test::ActualCallArguments(inputs, outputs);
    inputs->Add(indata);
  }

  TypedReg Result() {
    TypedReg in = functionArg->AddDataReg();
    TypedReg in1 = functionArg->AddDataReg();
    TypedReg out = functionResult->AddDataReg();
    functionArg->EmitLoadTo(in);

    OperandRegister c = be.AddCReg();
    be.EmitCmp(c, in, be.Immed(type, (uint64_t) 0), BRIG_COMPARE_EQ);
    std::string zero = be.AddLabel();
    std::string end = be.AddLabel();
    be.EmitCbr(c, zero);
    be.EmitArith(BRIG_OPCODE_SUB, in1, in, be.Immed(type, 1));

    emitter::TypedRegList inputs = be.AddTRegList(), outputs = be.AddTRegList();
    inputs->Add(in1);
    outputs->Add(out);
    be.EmitCallSeq(function, inputs, outputs);

    be.EmitArith(BRIG_OPCODE_MUL, out, out, in);
    be.EmitBr(end);
    be.EmitLabel(zero);
    be.EmitMov(out, 1);
    be.EmitLabel(end);
    return out;
  }
};

class RecursiveFibonacci : public Test {
private:
  BrigType type;
  Variable functionArg;
  Buffer input;

public:
  RecursiveFibonacci(BrigType type_)
    : Test(emitter::FUNCTION),
      type(type_) { }

  void Name(std::ostream& out) const {
    out << type2str(type);
  }

  void Init() {
    Test::Init();
    ValueType vtype = Brig2ValueType(type);
    functionArg = function->NewVariable("n", BRIG_SEGMENT_ARG, type);
    input = kernel->NewBuffer("input", HOST_INPUT_BUFFER, vtype, geometry->GridSize());
    for (unsigned wi = 0; wi < input->Count(); ++wi) { input->AddData(Value(vtype, InputValue(wi))); }
  }

  uint64_t InputValue(uint64_t wi) const {
    return wi % 11;
  }

  uint64_t Fibonacci(uint64_t n) const {
    assert(n <= 11);
    uint64_t f = 1, f1 = 0;
    for (uint64_t i = 1; i <= n; ++i) {
      uint64_t fs = f;
      f += f1;
      f1 = fs;
    }
    return f;
  }

  BrigType ResultType() const override { return type; }

  Value ExpectedResult(uint64_t wi) const {
    return Value(Brig2ValueType(type), Fibonacci(InputValue(wi)));
  }

  void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs)
  {
    TypedReg indata = functionArg->AddDataReg();
    input->EmitLoadData(indata);
    Test::ActualCallArguments(inputs, outputs);
    inputs->Add(indata);
  }

  TypedReg Result() {
    TypedReg in = functionArg->AddDataReg();
    TypedReg in1 = functionArg->AddDataReg();
    TypedReg out = functionResult->AddDataReg();
    TypedReg out1 = functionResult->AddDataReg();
    functionArg->EmitLoadTo(in);

    OperandRegister c = be.AddCReg();
    be.EmitCmp(c, in, be.Immed(type, (uint64_t) 1), BRIG_COMPARE_LE);
    std::string zero = be.AddLabel();
    std::string end = be.AddLabel();
    be.EmitCbr(c, zero);
    be.EmitArith(BRIG_OPCODE_SUB, in1, in, be.Immed(type, 1));

    emitter::TypedRegList inputs = be.AddTRegList(), outputs = be.AddTRegList();
    inputs->Add(in1);
    outputs->Add(out);
    be.EmitCallSeq(function, inputs, outputs);

    be.EmitArith(BRIG_OPCODE_SUB, in1, in1, be.Immed(type, 1));
    emitter::TypedRegList inputs1 = be.AddTRegList(), outputs1 = be.AddTRegList();
    inputs1->Add(in1);
    outputs1->Add(out1);
    be.EmitCallSeq(function, inputs1, outputs1);

    be.EmitArith(BRIG_OPCODE_ADD, out, out, out1);
    be.EmitBr(end);
    be.EmitLabel(zero);
    be.EmitMov(out, 1);
    be.EmitLabel(end);
    return out;
  }
};

class VariadicSum : public Test {
private:
  BrigType type;
  Variable fcount, farray;
  Buffer input;
  static const unsigned callCount = 1;

public:
  VariadicSum(Grid geometry, BrigType type_)
    : Test(emitter::FUNCTION, geometry),
      type(type_) { }

  void Name(std::ostream& out) const {
    out << type2str(type) << "_" << geometry;
  }

  void Init() {
    Test::Init();
    ValueType vtype = Brig2ValueType(type);
    fcount = function->NewVariable("count", BRIG_SEGMENT_ARG, BRIG_TYPE_U32);
    farray = function->NewFlexArray("array", type);
    input = kernel->NewBuffer("input", HOST_INPUT_BUFFER, vtype, geometry->GridSize() + 16 * 4);
    for (unsigned wi = 0; wi < input->Count(); ++wi) { input->AddData(Value(vtype, InputValue(wi))); }
  }

  uint64_t InputValue(uint64_t wi) const {
    return wi % 11;
  }

  uint64_t Count(uint64_t pos) const {
    switch (pos) {
    case 0: return 6;
    case 1: return 1;
    case 2: return 16;
    default: assert(false); return 0;
    }
  }

  uint64_t Sum(uint64_t wi, uint64_t pos) const {
    uint64_t sum = 0;
    for (uint64_t i = 0; i < Count(pos); ++i) {
      sum += InputValue(wi + i);
    }
    return sum;
  }

  BrigType ResultType() const override { return type; }

  //uint64_t ResultDim() const { return callCount; }
  size_t OutputBufferSize() const { return geometry->GridSize() * callCount; }

  Value ExpectedResult(uint64_t wi, uint64_t pos) const {
    return Value(Brig2ValueType(type), Sum(wi, pos));
  }

  void Call(unsigned index, uint64_t count, TypedRegList rdata, PointerReg base) {
    emitter::TypedRegList inputs = be.AddTRegList(), outputs = be.AddTRegList();
    TypedReg rcount = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(rcount, count);
    inputs->Add(rcount);
    TypedReg arr = be.AddTRegFrom(rdata, count);
    inputs->Add(arr);
    TypedReg result = be.AddTReg(type);
    outputs->Add(result);
    be.EmitCallSeq(function, inputs, outputs);
    be.EmitStore(result, base, index * result->TypeSizeBytes());
  }

  void KernelCode() {
    TypedRegList rdata = be.AddTRegList();
    uint64_t offset = 0;
    PointerReg ibase = input->DataAddressReg(be.WorkitemFlatAbsId(input->Address()->IsLarge()), 0, false);
    for (unsigned i = 0; i < 16; ++i) {
      TypedReg r = be.AddTReg(type);
      rdata->Add(r);
      be.EmitLoad(r, ibase, offset);
      //input->EmitLoadData(r, offset);
      offset += r->TypeSizeBytes();
    }
    PointerReg base = output->DataAddressReg(be.WorkitemFlatAbsId(output->Address()->IsLarge()), 0, false, callCount);
    for (uint64_t i = 0; i < callCount; ++i) {
      Call(i, Count(i), rdata, base);
    }
  }

  TypedReg Result() {
    TypedReg rindex = fcount->AddDataReg(), roffset = fcount->AddDataReg();
    TypedReg rsum = be.AddTReg(type);
    fcount->EmitLoadTo(rindex);
    be.EmitMov(rsum, (uint64_t) 0);
    be.EmitMov(roffset, (uint64_t) 0);
    std::string loop = be.AddLabel();
    std::string loopEnd = be.AddLabel();
    be.EmitLabel(loop);
    OperandRegister c = be.AddCReg();
    be.EmitCmp(c, rindex, be.Immed(rindex->Type(), 0), BRIG_COMPARE_EQ);
    be.EmitCbr(c, loopEnd);
    TypedReg rvalue = be.AddTReg(type);
    be.EmitLoad(BRIG_SEGMENT_ARG, rvalue, be.Address(farray->Variable(), roffset->Reg(), 0));
    be.EmitArith(BRIG_OPCODE_ADD, rsum, rsum, rvalue);
    be.EmitArith(BRIG_OPCODE_SUB, rindex, rindex, be.Immed(rindex->Type(), 1));
    be.EmitArith(BRIG_OPCODE_ADD, roffset, roffset, be.Immed(roffset->Type(), rsum->TypeSizeBytes()));
    be.EmitBr(loop);
    be.EmitLabel(loopEnd);
    return rsum;
  }

};

class DoubleRecursiveTest : public Test {
protected:
  EFunction* base;
  EFunction* first;
  EFunction* second;
  Variable baseInput;
  Variable baseResult;
  Variable firstInput;
  Variable firstResult;
  Variable secondInput;
  Variable secondResult;
  Buffer input;

  virtual void InitFunctions() {
    base = te->NewFunction("base");
    first = te->NewFunction("first");
    second = te->NewFunction("second");
    baseInput = base->NewVariable("input", BRIG_SEGMENT_ARG, ResultType(), Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, false);
    baseResult = base->NewVariable("result", BRIG_SEGMENT_ARG, ResultType(), Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, true);
    firstInput = first->NewVariable("input", BRIG_SEGMENT_ARG, ResultType(), Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, false);
    firstResult = first->NewVariable("result", BRIG_SEGMENT_ARG, ResultType(), Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, true);
    secondInput = second->NewVariable("input", BRIG_SEGMENT_ARG, ResultType(), Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, false);
    secondResult = second->NewVariable("result", BRIG_SEGMENT_ARG, ResultType(), Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, true);
  }

  virtual void InitInputData(Buffer input) {
    for (size_t i = 0; i < geometry->GridSize(); ++i) {
      input->AddData(Value(Brig2ValueType(ResultType()), i));
    }
  }

  virtual void EmitBaseFunction() = 0;
  virtual void EmitFirstFunction() = 0;
  virtual void EmitSecondFunction() = 0;

public:
  DoubleRecursiveTest(Grid geometry_) : Test(Location::KERNEL, geometry_) {}

  void Name(std::ostream& out) const override {
    out << geometry;
  }

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  
  void Init() override {
    Test::Init();
    InitFunctions();
    
    input = kernel->NewBuffer("input", BufferType::HOST_INPUT_BUFFER, Brig2ValueType(ResultType()), geometry->GridSize());
    InitInputData(input);
  }

  void Executables() override {
    base->Declaration();

    first->StartFunction();
    first->FunctionFormalOutputArguments();
    first->FunctionFormalInputArguments();
    first->StartFunctionBody();
    EmitFirstFunction();
    first->EndFunction();

    second->StartFunction();
    second->FunctionFormalOutputArguments();
    second->FunctionFormalInputArguments();
    second->StartFunctionBody();
    EmitSecondFunction();
    second->EndFunction();

    base->StartFunction();
    base->FunctionFormalOutputArguments();
    base->FunctionFormalInputArguments();
    base->StartFunctionBody();
    EmitBaseFunction();
    base->EndFunction();

    Test::Executables();
  }

  TypedReg Result() override {
    // kernel calls base function with input value and returns its result
    auto value = be.AddTReg(ResultType());
    input->EmitLoadData(value);
    auto ins = be.AddTRegList();
    ins->Add(value);
    auto outs = be.AddTRegList();
    outs->Add(value);
    be.EmitCallSeq(base->Directive(), ins, outs);
    return value;
  }
};

class CollatzRecursiveTest : public DoubleRecursiveTest {
protected:
  virtual void EmitSwitchCall(TypedReg index, EFunction* first, EFunction* second, TypedRegList inArgs, TypedRegList outArgs) = 0;

  void InitInputData(Buffer input) override {
    for (size_t i = 1; i <= geometry->GridSize(); ++i) { // input must be greater than 0
      input->AddData(Value(Brig2ValueType(ResultType()), i));
    }
  }

  void EmitBaseFunction() override {
    // base function checks if input is odd or even and transfers control to corresponding function
    std::string oneLabel = "@one";
    auto n = be.AddTReg(baseInput->Type());
    baseInput->EmitLoadTo(n);

    // if n == 0 then return 0
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp, n, be.Immed(n->Type(), 1), BRIG_COMPARE_EQ);
    be.EmitCbr(cmp, oneLabel);

    // calculate reminder of n / 2
    auto rem = be.AddTReg(n->Type());
    be.EmitArith(BRIG_OPCODE_REM, rem, n, be.Immed(n->Type(), 2));

    // call recursive functions
    auto ins = be.AddTRegList();
    ins->Add(n);
    auto outs = be.AddTRegList();
    outs->Add(n);
    EmitSwitchCall(rem, first, second, ins, outs);
    baseResult->EmitStoreFrom(n);
    be.EmitRet();

    // if one return 0
    be.EmitLabel(oneLabel);
    be.EmitStore(baseResult->Segment(), baseResult->Type(), be.Immed(baseResult->Type(), 0), be.Address(baseResult->Variable()));
  }

  void EmitFirstFunction() override {
    // even - divide input by 2 and call base function
    // then increment number of steps by 1
    auto n = be.AddTReg(firstInput->Type());
    firstInput->EmitLoadTo(n);
    be.EmitArith(BRIG_OPCODE_DIV, n, n, be.Immed(n->Type(), 2));
    
    auto ins = be.AddTRegList();
    ins->Add(n);
    auto outs = be.AddTRegList();
    outs->Add(n);
    be.EmitCallSeq(base->Directive(), ins, outs);

    be.EmitArith(BRIG_OPCODE_ADD, n, n, be.Immed(n->Type(), 1));
    firstResult->EmitStoreFrom(n);
  }

  void EmitSecondFunction() override {
    // odd - calculate 3x + 1
    // the next function always must be even so call it directly without calling base function
    // then increment number of steps by 1
    auto n = be.AddTReg(firstInput->Type());
    firstInput->EmitLoadTo(n);
    be.EmitArith(BRIG_OPCODE_MAD, n, n, be.Immed(n->Type(), 3), be.Immed(n->Type(), 1));
    
    auto ins = be.AddTRegList();
    ins->Add(n);
    auto outs = be.AddTRegList();
    outs->Add(n);
    be.EmitCallSeq(first->Directive(), ins, outs);

    be.EmitArith(BRIG_OPCODE_ADD, n, n, be.Immed(n->Type(), 1));
    firstResult->EmitStoreFrom(n);
  }

public:
  CollatzRecursiveTest(Grid geometry_) : DoubleRecursiveTest(geometry_) {}

  Value ExpectedResult(uint64_t id) const override {
    // calculate number of Collatz conjecture steps
    ++id; // increment id to correspond with input values
    unsigned steps = 0;
    while (id != 1) {
      if ((id % 2) == 1) {  // id is odd
        id = 3 * id + 1;
      } else {  // id is even
        id /= 2;
      }
      ++steps;
    }
    return Value(Brig2ValueType(ResultType()), steps);
  }
};

class CallCollatzRecursiveTest : public CollatzRecursiveTest {
protected:
  void EmitSwitchCall(TypedReg index, EFunction* first, EFunction* second, TypedRegList inArgs, TypedRegList outArgs) override {
    // emulate scall with help of sbr and two calls
    Condition cond = new(te->Ap()) ECondition(COND_SWITCH, index, BRIG_WIDTH_ALL, 2);
    cond->Reset(te.get());
    
    cond->EmitSwitchStart();
    cond->EmitSwitchBranchStart(0);
    be.EmitCallSeq(first->Directive(), inArgs, outArgs);
    cond->EmitSwitchBranchStart(1);
    be.EmitCallSeq(second->Directive(), inArgs, outArgs);
    cond->EmitSwitchEnd();
  }

public:
  CallCollatzRecursiveTest(Grid geometry_) : CollatzRecursiveTest(geometry_) {}
};

class IncrementRecursiveTest : public DoubleRecursiveTest {
private:
  static const unsigned limitValue = 100;

protected:
  unsigned LimitValue() const { return limitValue; }

  virtual void EmitSwitchCall(TypedReg index, EFunction* first, EFunction* second, TypedRegList inArgs, TypedRegList outArgs) = 0;

  void EmitBaseFunction() override {
    // base function checks if current value is more or equal to limit
    // if so calls identity function
    // else calls increment function (recursive call)
    // and returns the result of this call
    auto current = be.AddTReg(baseInput->Type());
    baseInput->EmitLoadTo(current);
    auto cmp = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCmp(cmp, current, be.Immed(current->Type(), limitValue), BRIG_COMPARE_GE);
    be.EmitArith(BRIG_OPCODE_AND, cmp, cmp, be.Immed(cmp->Type(), 1)); // apply mask to convert 0xFFFFFFFF to 0x00000001
  
    auto ins = be.AddTRegList();
    ins->Add(current);
    auto outs = be.AddTRegList();
    outs->Add(current);
    EmitSwitchCall(cmp, first, second, ins, outs);

    baseResult->EmitStoreFrom(current);
  }
    
  void EmitFirstFunction() override {
    // increment functions input variable
    // and then call base function with new current value (recursive call)
    auto current = be.AddTReg(firstInput->Type());
    firstInput->EmitLoadTo(current);
    be.EmitArith(BRIG_OPCODE_ADD, current, current, be.Immed(current->Type(), 1));
    
    auto ins = be.AddTRegList();
    ins->Add(current);
    auto outs = be.AddTRegList();
    outs->Add(current);
    be.EmitCallSeq(base->Directive(), ins, outs);

    firstResult->EmitStoreFrom(current);
  }

  void EmitSecondFunction() override {
    // identity - simply returns value that was passed to it
    auto tmp = be.AddTReg(secondInput->Type());
    secondInput->EmitLoadTo(tmp);
    secondResult->EmitStoreFrom(tmp);
  }

public:
  IncrementRecursiveTest(Grid geometry_) : DoubleRecursiveTest(geometry_) {}

  Value ExpectedResult(uint64_t id) const override {
    if (id >= limitValue) {
      return Value(Brig2ValueType(ResultType()), id);
    } else {
      return Value(Brig2ValueType(ResultType()), limitValue);
    }
  }
};

class CallIncrementRecursiveTest : public IncrementRecursiveTest {
protected:
  void EmitSwitchCall(TypedReg index, EFunction* first, EFunction* second, TypedRegList inArgs, TypedRegList outArgs) override {
    // emulate scall with help of sbr and two calls
    Condition cond = new(te->Ap()) ECondition(COND_SWITCH, index, BRIG_WIDTH_ALL, 2);
    cond->Reset(te.get());
    
    cond->EmitSwitchStart();
    cond->EmitSwitchBranchStart(0);
    be.EmitCallSeq(first->Directive(), inArgs, outArgs);
    cond->EmitSwitchBranchStart(1);
    be.EmitCallSeq(second->Directive(), inArgs, outArgs);
    cond->EmitSwitchEnd();
  }

public:
  CallIncrementRecursiveTest(Grid geometry_) : IncrementRecursiveTest(geometry_) {}
};


class ScallBasicTest : public Test {
protected:
  unsigned functionsNumber; // functionsNumber is not always equal to the size of functions vector
                            // in case of repeating functions functionsNumber will be greater
  BrigType indexType;
  BrigType resultType;
  std::vector<EFunction*> functions;
  std::vector<Variable> outArgs;

  virtual Value FunctionResult(uint64_t number) const {   // result of function with specified number
    assert(number < functionsNumber);
    return Value(Brig2ValueType(ResultType()), number);
  } 

  virtual void InitFunctions() {
    EFunction* func;
    for (unsigned i = 0; i < functionsNumber; ++i) {
      func = te->NewFunction("func" + std::to_string(i));
      outArgs.push_back(func->NewVariable("out", BRIG_SEGMENT_ARG, ResultType(), Location::AUTO, 
        BRIG_ALIGNMENT_NONE, 0, false, true));
      functions.push_back(func);
    }
  }

public:
  ScallBasicTest(Location codeLocation_, Grid geometry_, unsigned functionsNumber_, 
                 BrigType indexType_ = BRIG_TYPE_U32, BrigType resultType_ = BRIG_TYPE_U32) : 
    Test(codeLocation_, geometry_), functionsNumber(functionsNumber_), indexType(indexType_), resultType(resultType_) {}

  bool IsValid() const override {
    return Test::IsValid() 
      && functionsNumber > 0
      && (indexType == BRIG_TYPE_U32 || indexType == BRIG_TYPE_U64);
  }

  void Name(std::ostream& out) const override {
    out << functionsNumber << "_" << type2str(indexType) << "_" << type2str(resultType) << "/" << geometry << "_" << CodeLocationString();
  }

  void Init() override {
    Test::Init();
    InitFunctions();
  }

  BrigType ResultType() const override { return resultType; }

  Value ExpectedResult(uint64_t id) const override {
    return FunctionResult(id % functionsNumber);
  }

  void Executables() override {
    for (unsigned i = 0; i < functions.size(); ++i) {
      functions[i]->StartFunction();
      functions[i]->FunctionFormalOutputArguments();
      functions[i]->FunctionFormalInputArguments();
      functions[i]->StartFunctionBody();
      be.EmitStore(BRIG_SEGMENT_ARG, ResultType(), be.Value2Immed(FunctionResult(i), false), 
        be.Address(outArgs[i]->Variable()));
      functions[i]->EndFunction();
    }
    Test::Executables();
  }

  TypedReg Result() override {
    auto wiId = be.EmitWorkitemFlatAbsId(indexType == BRIG_TYPE_U64);
    be.EmitArith(BRIG_OPCODE_REM, wiId, wiId, be.Immed(wiId->Type(), functionsNumber));
    auto funcResult = be.AddTReg(ResultType());
    be.EmitMov(funcResult, 0xFFFFFFFF);
    auto ins = be.AddTRegList();
    auto outs = be.AddTRegList();
    outs->Add(funcResult);
    be.EmitScallSeq(wiId, functions, ins, outs);
    return funcResult;
  }
};

class ScallImmedTest : public ScallBasicTest {
private:
  unsigned indexValue;

public:
  ScallImmedTest(Location codeLocation_, Grid geometry_, unsigned functionsNumber_, unsigned indexValue_, BrigType indexType_)
    : ScallBasicTest(codeLocation_, geometry_, functionsNumber_, indexType_), indexValue(indexValue_) {}

  bool IsValid() const override {
    return ScallBasicTest::IsValid() 
      && indexValue < functionsNumber;
  }

  void Name(std::ostream& out) const override {
    out << functionsNumber << "_" << indexValue << type2str(indexType) << "/" << geometry << "_" << CodeLocationString();
  }

  Value ExpectedResult() const override {
    return FunctionResult(indexValue);
  }

  TypedReg Result() override {
    auto funcResult = be.AddTReg(ResultType());
    be.EmitMov(funcResult, 0xFFFFFFFF);
    auto ins = be.AddTRegList();
    auto outs = be.AddTRegList();
    outs->Add(funcResult);
    be.EmitScallSeq(indexType, be.Immed(indexType, indexValue), functions, ins, outs);
    return funcResult;
  }
};

class ScallRepeatingFunctions : public ScallBasicTest {
private:
  unsigned numberRepeating;
  std::vector<EFunction*> callFunctions;

protected:
  virtual Value FunctionResult(uint64_t number) const {
    assert(number < functionsNumber);
    if (number < (functionsNumber - numberRepeating)) {
      return Value(Brig2ValueType(ResultType()), number);
    } else {
      return Value(Brig2ValueType(ResultType()), 0);
    }
  } 

  virtual void InitFunctions() {
    EFunction* func;
    for (unsigned i = 0; i < functionsNumber - numberRepeating; ++i) {
      func = te->NewFunction("func" + std::to_string(i));
      outArgs.push_back(func->NewVariable("out", BRIG_SEGMENT_ARG, ResultType(), Location::AUTO, 
        BRIG_ALIGNMENT_NONE, 0, false, true));
      functions.push_back(func);
    }
    callFunctions = functions;
    for (unsigned i = 0; i < numberRepeating; ++i) {
      callFunctions.push_back(functions[0]);
    }
  }

public:
  ScallRepeatingFunctions(Location codeLocation_, Grid geometry_, unsigned functionsNumber_, unsigned numberRepeating_)
    : ScallBasicTest(codeLocation_, geometry_, functionsNumber_),  numberRepeating(numberRepeating_) {}

  void Name(std::ostream& out) const override {
    out << functionsNumber << "_" << numberRepeating << "/" << geometry << "_" << CodeLocationString();
  }

  bool IsValid() const override {
    return ScallBasicTest::IsValid()
      && functionsNumber > numberRepeating;
  }

  TypedReg Result() override {
    auto wiId = be.EmitWorkitemFlatAbsId(indexType == BRIG_TYPE_U64);
    be.EmitArith(BRIG_OPCODE_REM, wiId, wiId, be.Immed(wiId->Type(), functionsNumber));
    auto funcResult = be.AddTReg(ResultType());
    be.EmitMov(funcResult, be.Immed(ResultType(), 0xFFFFFFFF));
    auto ins = be.AddTRegList();
    auto outs = be.AddTRegList();
    outs->Add(funcResult);
    be.EmitScallSeq(wiId, callFunctions, ins, outs);
    return funcResult;
  }
};

class ScallCollatzecursiveTest : public CollatzRecursiveTest {
protected:
  void EmitSwitchCall(TypedReg index, EFunction* first, EFunction* second, TypedRegList inArgs, TypedRegList outArgs) override {
    std::vector<EFunction*> functions;
    functions.push_back(first);
    functions.push_back(second);
    be.EmitScallSeq(index, functions, inArgs, outArgs);
  }

public:
  ScallCollatzecursiveTest(Grid geometry_) : CollatzRecursiveTest(geometry_) {}
};

class ScallIncrementRecursiveTest : public IncrementRecursiveTest {
protected:
  void EmitSwitchCall(TypedReg index, EFunction* first, EFunction* second, TypedRegList inArgs, TypedRegList outArgs) override {
    std::vector<EFunction*> functions;
    functions.push_back(first);
    functions.push_back(second);
    be.EmitScallSeq(index, functions, inArgs, outArgs);
  }

public:
  ScallIncrementRecursiveTest(Grid geometry_) : IncrementRecursiveTest(geometry_) {}
};

void FunctionsTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<FunctionArguments>(ap, it, "functions/arguments/1arg", cc->Variables().ByTypeDimensionAlign(BRIG_SEGMENT_ARG), Bools::All(), Bools::All());
  TestForEach<RecursiveFactorial>(ap, it, "functions/recursion/factorial", cc->Types().CompoundIntegral());
  TestForEach<RecursiveFibonacci>(ap, it, "functions/recursion/fibonacci", cc->Types().CompoundIntegral());
  TestForEach<CallCollatzRecursiveTest>(ap, it, "functions/recursion/collatz", cc->Grids().DefaultGeometrySet());
  TestForEach<CallIncrementRecursiveTest>(ap, it, "functions/recursion/increment", cc->Grids().DefaultGeometrySet());
  //TestForEach<VariadicSum>(ap, it, "functions/variadic/sum", cc->Grids().DefaultGeometrySet(), cc->Types().CompoundIntegral());

  TestForEach<ScallBasicTest>(ap, it, "functions/scall/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Functions().ScallFunctionsNumber(), cc->Functions().ScallIndexType(), cc->Types().Compound());
  //TestForEach<ScallImmedTest>(ap, it, "functions/scall/immed", CodeLocations(), cc->Grids().SimpleSet(), cc->Functions().ScallFunctionsNumber(), cc->Functions().ScallIndexValue(), cc->Functions().ScallIndexType());
  TestForEach<ScallRepeatingFunctions>(ap, it, "functions/scall/repeating", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->Functions().ScallFunctionsNumber(), cc->Functions().ScallNumberRepeating());
  //TestForEach<ScallCollatzRecursiveTest>(ap, it, "functions/scall/recursion/collatz", cc->Grids().DefaultGeometrySet());
  //TestForEach<ScallIncrementRecursiveTest>(ap, it, "functions/scall/recursion/increment", cc->Grids().SimpleSet());
}

}
