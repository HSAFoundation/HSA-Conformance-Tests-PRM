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

#include "LibrariesTests.hpp"
#include "HCTests.hpp"
#include "BrigEmitter.hpp"
#include "CoreConfig.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace HSAIL_ASM;

namespace hsail_conformance {

class ModuleScopeVariableLinkageTest : public Test {
private:
  BrigLinkage linkage;
  BrigSegment segment;
  Variable def;   // variable definition
  Variable decl;  // variable declaration

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

public:
  ModuleScopeVariableLinkageTest(BrigLinkage linkage_, BrigSegment segment_) 
    : Test(Location::KERNEL), linkage(linkage_), segment(segment_) {}

  bool IsValid() const override {
    return (linkage == BRIG_LINKAGE_PROGRAM || linkage == BRIG_LINKAGE_MODULE)
        && (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_GROUP 
            || segment == BRIG_SEGMENT_PRIVATE || segment == BRIG_SEGMENT_READONLY);
  }
  
  void Name(std::ostream& out) const override {
    out << linkage2str(linkage) << "_" 
        << segment2str(segment);
  }

  void Init() override {
    Test::Init();
    decl = te->NewVariable("var", segment, VALUE_TYPE, Location::MODULE);
    def = te->NewVariable("var", segment, VALUE_TYPE, Location::MODULE);
    if (segment == BRIG_SEGMENT_READONLY) {
      def->PushBack(ExpectedResult());
    }
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  TypedReg Result() override {
    // store VALUE in def
    if (segment != BRIG_SEGMENT_READONLY) {
      be.EmitStore(def->Segment(), VALUE_TYPE, be.Immed(VALUE_TYPE, VALUE), be.Address(def->Variable()));
    }

    // load from def
    auto result = be.AddTReg(VALUE_TYPE);
    be.EmitLoad(def->Segment(), result, be.Address(def->Variable()));
    return result;
  }

  void ModuleVariables() override {
    decl->ModuleVariables();
    def->ModuleVariables();
  }

  void EndProgram() override {
    decl->Variable().linkage() = linkage;
    decl->Variable().modifier().isDefinition() = false;
    def->Variable().linkage() = linkage;
    Test::EndProgram();
  }
};

class ModuleScopeFunctionLinkageTest : public Test {
private:
  EFunction* def;
  EFunction* decl;
  Variable defArg;
  Variable declArg;

  BrigLinkage linkage;

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

public:
  ModuleScopeFunctionLinkageTest(BrigLinkage linkage_) : Test(Location::KERNEL), linkage(linkage_) {}

  void Name(std::ostream& out) const override {
    out << linkage2str(linkage);
  }

  void Init() override {
    Test::Init();
    decl = te->NewFunction("func");
    declArg = decl->NewVariable("arg", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, true);
    def = te->NewFunction("func");
    defArg = def->NewVariable("arg", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, true);
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void Executables() override {
    def->Declaration();
    def->StartFunctionBody();
    be.EmitStore(BRIG_SEGMENT_ARG, VALUE_TYPE, be.Immed(VALUE_TYPE, VALUE), be.Address(defArg->Variable()));
    def->EndFunction();
    decl->Declaration();
    Test::Executables();
  }

  TypedReg Result() override {
    // call function definition and return it result
    auto result = be.AddTReg(VALUE_TYPE);
    auto inArgs = be.AddTRegList();
    auto outArgs = be.AddTRegList();
    outArgs->Add(result);
    be.EmitCallSeq(def, inArgs, outArgs);
    return result;
  }
   
  void EndProgram() override {
    decl->Directive().name() = "&func";
    decl->Directive().linkage() = linkage;
    def->Directive().name() = "&func";
    def->Directive().linkage() = linkage;
    Test::EndProgram();
  }
};

class ModuleScopeKernelLinkageTest : public Test {
private:
  EKernel* decl;
  Variable declArg;

  BrigLinkage linkage;

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

public:
  ModuleScopeKernelLinkageTest(BrigLinkage linkage_) : Test(Location::KERNEL), linkage(linkage_) {}

  void Name(std::ostream& out) const override {
    out << linkage2str(linkage);
  }

  void Init() override {
    Test::Init();
    decl = te->NewKernel(kernel->KernelName());
    declArg = decl->NewVariable(output->BufferName(), BRIG_SEGMENT_KERNARG, be.PointerType(), Location::KERNEL, BRIG_ALIGNMENT_NONE, 0, false, true);
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void Executables() override {
    decl->Declaration();
    Test::Executables();
  }

  TypedReg Result() override {
    // return VALUE from kernel definition
    return be.AddInitialTReg(VALUE_TYPE, VALUE);
  }
   
  void EndProgram() override {
    decl->Directive().modifier().isDefinition() = false;
    decl->Directive().name() = kernel->KernelName();
    decl->Directive().linkage() = linkage;
    kernel->Directive().linkage() = linkage;
    Test::EndProgram();
  }
};

class ModuleScopeFBarrierLinkageTest : public Test {
private:
  FBarrier decl;
  FBarrier def;

  BrigLinkage linkage;

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

public:
  ModuleScopeFBarrierLinkageTest(BrigLinkage linkage_) : Test(Location::KERNEL), linkage(linkage_) {}

  void Name(std::ostream& out) const override {
    out << linkage2str(linkage);
  }

  void Init() override {
    Test::Init();
    def = te->NewFBarrier("fbar", Location::MODULE);
    decl = te->NewFBarrier("fbar", Location::MODULE);
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void ModuleVariables() override {
    decl->ModuleVariables();
    def->ModuleVariables();
    Test::ModuleVariables();
  }

  TypedReg Result() override {
    def->EmitInitfbarInFirstWI();
    def->EmitJoinfbar();
    be.EmitBarrier();
    def->EmitWaitfbar();
    be.EmitBarrier();
    def->EmitLeavefbar();
    be.EmitBarrier();
    def->EmitReleasefbarInFirstWI();
    // return VALUE from kernel definition
    return be.AddInitialTReg(VALUE_TYPE, VALUE);
  }
   
  void EndProgram() override {
    decl->FBarrier().modifier().isDefinition() = false;
    decl->FBarrier().linkage() = linkage;
    def->FBarrier().linkage() = linkage;
    Test::EndProgram();
  }
};

class FunctionLinkageTest : public Test {
private:
  EKernel* secondKernel;
  EFunction* secondFunction;

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

protected:
  EKernel* SecondKernel() const { return secondKernel; }
  EFunction* SecondFunction() const { return secondFunction; }
  uint32_t ResultValue() const { return VALUE; }

  virtual void SecondKernelBody() {}
  virtual void SecondFunctionBody() {}

public:
  FunctionLinkageTest() : Test(Location::KERNEL) {}

  void Init() override {
    Test::Init();
    secondKernel = te->NewKernel("second_kernel");
    secondFunction = te->NewFunction("second_function");
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void Executables() override {
    // emit second function
    secondFunction->StartFunction();
    secondFunction->FunctionFormalOutputArguments();
    secondFunction->FunctionFormalInputArguments();
    secondFunction->StartFunctionBody();
    secondFunction->FunctionVariables();
    SecondFunctionBody();
    secondFunction->EndFunction();

    // emit second kernel
    // DO NO EMIT second kernel until conformance support it
    // secondKernel->StartKernel();
    // secondKernel->KernelArguments();
    // secondKernel->StartKernelBody();
    // secondKernel->KernelVariables();
    // SecondKernelBody();
    // secondKernel->EndKernel();

    Test::Executables();
  }
};

class FunctionLinkageVariableTest : public FunctionLinkageTest {
private:
  Variable firstKernelVar;
  Variable secondKernelVar;
  Variable secondFunctionVar;

  BrigSegment segment;

  static const uint32_t SECOND_VALUE = 987654321;
  static const uint32_t THIRD_VALUE = 456789123;

protected:
  void SecondKernelBody() override {
    if (segment != BRIG_SEGMENT_READONLY) {
      be.EmitStore(segment, ResultType(), be.Immed(ResultType(), SECOND_VALUE), be.Address(secondKernelVar->Variable()));
    }
  }

  void SecondFunctionBody() override {
    if (segment != BRIG_SEGMENT_READONLY) {
      be.EmitStore(segment, ResultType(), be.Immed(ResultType(), THIRD_VALUE), be.Address(secondFunctionVar->Variable()));
    }
  }

public:
  FunctionLinkageVariableTest(BrigSegment segment_) : FunctionLinkageTest(), segment(segment_) {}

  bool IsValid() const override {
    return FunctionLinkageTest::IsValid()
        && (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_GROUP || 
            segment == BRIG_SEGMENT_PRIVATE || segment == BRIG_SEGMENT_SPILL || 
            segment == BRIG_SEGMENT_READONLY);
  }

  void Name(std::ostream& out) const override {
    out << segment2str(segment);
  }

  void Init() override {
    FunctionLinkageTest::Init();
    firstKernelVar = kernel->NewVariable("var1", segment, ResultType(), Location::KERNEL);
    secondKernelVar = SecondKernel()->NewVariable("var2", segment, ResultType(), Location::KERNEL);
    secondFunctionVar = SecondFunction()->NewVariable("var3", segment, ResultType(), Location::FUNCTION);
    if (segment == BRIG_SEGMENT_READONLY) {
      firstKernelVar->PushBack(Value(Brig2ValueType(ResultType()), ResultValue()));
      secondKernelVar->PushBack(Value(Brig2ValueType(ResultType()), SECOND_VALUE));
      secondFunctionVar->PushBack(Value(Brig2ValueType(ResultType()), THIRD_VALUE));
    }
  }

  TypedReg Result() override {
    // emit call to second function
    auto emptyArgs = be.AddTRegList();
    be.EmitCallSeq(SecondFunction(), emptyArgs, emptyArgs);

    // store result value in variable
    if (segment != BRIG_SEGMENT_READONLY) {
      be.EmitStore(segment, ResultType(), be.Immed(ResultType(), ResultValue()), be.Address(firstKernelVar->Variable()));
    }

    // load from variable and return it
    auto result = be.AddTReg(ResultType());
    be.EmitLoad(segment, result, be.Address(firstKernelVar->Variable()));
    return result;
  }

  void EndProgram() override {
    firstKernelVar->Variable().linkage() = BRIG_LINKAGE_FUNCTION;

    // DO NO DO THIS until conformance support multiple kernels
    //secondKernelVar->Variable().linkage() = BRIG_LINKAGE_FUNCTION;

    secondFunctionVar->Variable().linkage() = BRIG_LINKAGE_FUNCTION;
    FunctionLinkageTest::EndProgram();
  }
};

void LibrariesTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string path;
  Arena* ap = cc->Ap();

  TestForEach<ModuleScopeVariableLinkageTest>(ap, it, "linkage/variable", cc->Variables().ModuleScopeLinkage(), cc->Segments().ModuleScopeVariableSegments());
  TestForEach<ModuleScopeFunctionLinkageTest>(ap, it, "linkage/function", cc->Variables().ModuleScopeLinkage());
  TestForEach<ModuleScopeKernelLinkageTest>(ap, it, "linkage/kernel", cc->Variables().ModuleScopeLinkage());
  TestForEach<ModuleScopeFBarrierLinkageTest>(ap, it, "linkage/fbarrier", cc->Variables().ModuleScopeLinkage());

  TestForEach<FunctionLinkageVariableTest>(ap, it, "linkage/variable/function", cc->Segments().FunctionScopeVariableSegments());
}

}
