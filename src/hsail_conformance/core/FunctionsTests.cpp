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
using namespace Brig;

namespace hsail_conformance {

class FunctionArgumentsTest : public Test {
private:
  VariableSpec argSpec;
  Variable functionArg;
  Buffer input;
  bool useVectorInstructionsForFormals;
  bool useVectorInstructionsForActuals;

  static const uint32_t length = 64;

public:
  FunctionArgumentsTest(VariableSpec argSpec_,
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
    for (unsigned i = 0; i < input->Size(); ++i) { input->AddData(Value(argSpec->VType(), i)); }
  }

  TypedReg Result() {
    TypedReg in = functionArg->AddDataReg();
    TypedReg out = functionResult->AddDataReg();
    functionArg->EmitLoadTo(in);
    be.EmitMov(out, in);
    return out;
  }

  void KernelCode() {
    TypedReg kernArgInReg = functionArg->AddDataReg();
    Test::KernelCode();
  }

  void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs)
  {
    TypedReg indata = functionArg->AddDataReg();
    input->EmitLoadData(indata);
    Test::ActualCallArguments(inputs, outputs);
    inputs->Add(indata);
  }

  /*
  void Executables() {
    DirectiveFunction f = be.StartFunction();
    specList.FunctionFormalOutputArguments(); specList.FunctionFormalInputArguments();
    be.StartBody();
    be.EndFunction();

    DirectiveKernel k = be.StartKernel();
    specList.KernelArguments();
    be.StartBody();
    TypedReg kernArgInReg = argIn->AddDataReg();
    TypedReg kernArgOutReg = argOut->AddDataReg();
    TypedRegList kernArgInRegs = be.AddTRegList(), kernArgOutRegs = be.AddTRegList();
    kernArgInRegs->Add(kernArgInReg);
    kernArgOutRegs->Add(kernArgOutReg);
    input->EmitLoadData(kernArgInReg);
    be.EmitCallSeq(f, kernArgInRegs, kernArgOutRegs, useVectorInstructionsForActuals);
    output->EmitStoreData(kernArgOutReg);
    be.EndBody();
  }
  */

  BrigTypeX ResultType() const {
    return argSpec->Type();
  }

  uint64_t ResultDim() const {
    return argSpec->Dim();
  }

  Value ExpectedResult(uint64_t i) const {
    return Value(argSpec->VType(), i);
  }
};

void FunctionsTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<FunctionArgumentsTest>(ap, it, "functions/arguments/1arg", cc->Variables().ByTypeDimensionAlign(BRIG_SEGMENT_ARG), Bools::All(), Bools::All());
}

}
