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
    BrigType rtype = (BrigType) getUnsignedType(in->RegSizeBits());
    functionArg->EmitLoadTo(in);

    OperandRegister c = be.AddCReg();
    be.EmitCmp(c, rtype, in->Reg(), be.Immed(rtype, (uint64_t) 0), BRIG_COMPARE_EQ);
    std::string zero = be.AddLabel();
    std::string end = be.AddLabel();
    be.EmitCbr(c, zero);
    be.EmitArith(BRIG_OPCODE_SUB, rtype, in1->Reg(), in->Reg(), be.Immed(rtype, 1));

    emitter::TypedRegList inputs = be.AddTRegList(), outputs = be.AddTRegList();
    inputs->Add(in1);
    outputs->Add(out);
    be.EmitCallSeq(function, inputs, outputs);

    be.EmitArith(BRIG_OPCODE_MUL, rtype, out->Reg(), out->Reg(), in->Reg());
    be.EmitBr(end);
    be.EmitLabel(zero);
    be.EmitMov(out, be.Immed(rtype, (uint64_t) 1));
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
    BrigType rtype = (BrigType) getUnsignedType(in->RegSizeBits());
    functionArg->EmitLoadTo(in);

    OperandRegister c = be.AddCReg();
    be.EmitCmp(c, rtype, in->Reg(), be.Immed(rtype, (uint64_t) 1), BRIG_COMPARE_LE);
    std::string zero = be.AddLabel();
    std::string end = be.AddLabel();
    be.EmitCbr(c, zero);
    be.EmitArith(BRIG_OPCODE_SUB, rtype, in1->Reg(), in->Reg(), be.Immed(rtype, 1));

    emitter::TypedRegList inputs = be.AddTRegList(), outputs = be.AddTRegList();
    inputs->Add(in1);
    outputs->Add(out);
    be.EmitCallSeq(function, inputs, outputs);

    be.EmitArith(BRIG_OPCODE_SUB, rtype, in1->Reg(), in1->Reg(), be.Immed(rtype, 1));
    emitter::TypedRegList inputs1 = be.AddTRegList(), outputs1 = be.AddTRegList();
    inputs1->Add(in1);
    outputs1->Add(out1);
    be.EmitCallSeq(function, inputs1, outputs1);

    be.EmitArith(BRIG_OPCODE_ADD, rtype, out->Reg(), out->Reg(), out1->Reg());
    be.EmitBr(end);
    be.EmitLabel(zero);
    be.EmitMov(out, be.Immed(rtype, (uint64_t) 1));
    be.EmitLabel(end);
    return out;
  }
};

void FunctionsTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<FunctionArguments>(ap, it, "functions/arguments/1arg", cc->Variables().ByTypeDimensionAlign(BRIG_SEGMENT_ARG), Bools::All(), Bools::All());
  TestForEach<RecursiveFactorial>(ap, it, "functions/recursion/factorial", cc->Types().CompoundIntegral());
  TestForEach<RecursiveFibonacci>(ap, it, "functions/recursion/fibonacci", cc->Types().CompoundIntegral());
}

}
