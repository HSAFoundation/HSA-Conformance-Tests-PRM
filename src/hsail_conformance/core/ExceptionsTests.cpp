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

#include "ExceptionsTests.hpp"
#include "HCTests.hpp"
#include "BrigEmitter.hpp"
#include "CoreConfig.hpp"
#include "UtilTests.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace HSAIL_ASM;
using namespace hsail_conformance::utils;

namespace hsail_conformance {

class ClearDetectTest : public Test {
private:
  uint32_t exceptions;
  
public:
  ClearDetectTest(uint32_t exceptions_) : Test(Location::KERNEL), exceptions(exceptions_) {}
  
  bool IsValid() const override {
    return Test::IsValid() && 
           exceptions <= 0x1F;
  }

  void Name(std::ostream& out) const override {
    out << ExceptionsNumber2Str(exceptions);
  }

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 0); }

  TypedReg Result() override {
    // set exceptions
    be.EmitSetDetectExcept(exceptions);

    // clear exceptions
    be.EmitClearDetectExcept(exceptions);

    // get exceptions and return them
    auto detect = be.AddTReg(ResultType());
    be.EmitGetDetectExcept(detect);
    return detect;
  }

  void KernelDirectives() override {
    be.EmitEnableExceptionDirective(false, exceptions);
  }

  void SetupDispatch(const std::string & dispatchId) override {
    te->TestScenario()->Commands()->IsDetectSupported();
    Test::SetupDispatch(dispatchId);
  }
};

class SetDetectTest : public Test {
private:
  uint32_t exceptions;
  
public:
  SetDetectTest(uint32_t exceptions_) : Test(Location::KERNEL), exceptions(exceptions_) {}
  
  bool IsValid() const override {
    return Test::IsValid() && 
           exceptions <= 0x1F;
  }

  void Name(std::ostream& out) const override {
    out << ExceptionsNumber2Str(exceptions);
  }

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, exceptions); }

  TypedReg Result() override {
    // set exceptions
    be.EmitSetDetectExcept(exceptions);

    // get exceptions and return them
    auto detect = be.AddTReg(ResultType());
    be.EmitGetDetectExcept(detect);
    return detect;
  }

  void KernelDirectives() override {
    be.EmitEnableExceptionDirective(false, exceptions);
  }

  void SetupDispatch(const std::string & dispatchId) override {
    te->TestScenario()->Commands()->IsDetectSupported();
    Test::SetupDispatch(dispatchId);
  }
};

class GetDetectTest : public Test {
private:
  static const uint32_t ENABLED_EXCEPTIONS = 0x1F;
  static const uint32_t DIVIDE_BY_ZERO = 0x02;

public:
  GetDetectTest(bool) : Test(Location::KERNEL) {}
  
  void Name(std::ostream& out) const override {}

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, DIVIDE_BY_ZERO); }

  TypedReg Result() override {
    // raise "Divide by Zero" exception
    auto div = be.AddTReg(BRIG_TYPE_F32);
    be.EmitTypedMov(div->Type(), div->Reg(), be.Immed(1.0F));
    be.EmitArith(BRIG_OPCODE_DIV, div, div, be.Immed(0.0F));

    // get exceptions and return them
    auto detect = be.AddTReg(ResultType());
    be.EmitGetDetectExcept(detect);
    return detect;
  }

  void KernelDirectives() override {
    be.EmitEnableExceptionDirective(false, ENABLED_EXCEPTIONS);
  }

  void SetupDispatch(const std::string & dispatchId) override {
    te->TestScenario()->Commands()->IsDetectSupported();
    Test::SetupDispatch(dispatchId);
  }
};

void ExceptionsTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string path;
  Arena* ap = cc->Ap();

  TestForEach<ClearDetectTest>(ap, it, "exception/cleardetect", cc->Directives().ValidExceptionNumbers());
  TestForEach<SetDetectTest>(ap, it, "exception/setdetect", cc->Directives().ValidExceptionNumbers());
  TestForEach<GetDetectTest>(ap, it, "exception/getdetect", Bools::Value(true));
}

}
