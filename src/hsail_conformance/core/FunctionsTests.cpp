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

  static const uint32_t length = 64;

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


void FunctionsTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<FunctionArguments>(ap, it, "functions/arguments/1arg", cc->Variables().ByTypeDimensionAlign(BRIG_SEGMENT_ARG), Bools::All(), Bools::All());
  TestForEach<RecursiveFactorial>(ap, it, "functions/recursion/factorial", cc->Types().CompoundIntegral());
  TestForEach<RecursiveFibonacci>(ap, it, "functions/recursion/fibonacci", cc->Types().CompoundIntegral());
  //TestForEach<VariadicSum>(ap, it, "functions/variadic/sum", cc->Grids().DefaultGeometrySet(), cc->Types().CompoundIntegral());
}

}
