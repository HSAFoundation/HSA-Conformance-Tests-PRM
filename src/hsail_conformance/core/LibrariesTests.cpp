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
  Variable var;

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
    out << linkage2str(linkage) << "/" 
        << "variable/"
        << segment2str(segment);
  }

  void Init() override {
    Test::Init();
    var = te->NewVariable("var", segment, VALUE_TYPE, Location::MODULE);
    if (segment == BRIG_SEGMENT_READONLY) {
      var->AddData(ExpectedResult());
    }
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void ModuleVariables() override {
    var->EmitDeclaration();
    var->Variable().linkage() = linkage;
    Test::ModuleVariables();
  }

  TypedReg Result() override {
    // store VALUE in def
    if (segment != BRIG_SEGMENT_READONLY) {
      be.EmitStore(var->Segment(), VALUE_TYPE, be.Immed(VALUE_TYPE, VALUE), be.Address(var->Variable()));
    }

    // load from def
    auto result = be.AddTReg(VALUE_TYPE);
    be.EmitLoad(var->Segment(), result, be.Address(var->Variable()));
    return result;
  }

  void EndModule() override {
    var->EmitDefinition();
    var->Variable().linkage() = linkage;
    Test::EndModule();
  }
};

class ModuleScopeFunctionLinkageTest : public Test {
private:
  EFunction* func;
  Variable arg;

  BrigLinkage linkage;

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

public:
  ModuleScopeFunctionLinkageTest(BrigLinkage linkage_) : Test(Location::KERNEL), linkage(linkage_) {}

  void Name(std::ostream& out) const override {
    out << linkage2str(linkage) << "/function";
  }

  void Init() override {
    Test::Init();
    func = te->NewFunction("func");
    arg = func->NewVariable("arg", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::AUTO, BRIG_ALIGNMENT_NONE, 0, false, true);
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void Executables() override {
    func->Declaration();
    func->Directive().linkage() = linkage;
    Test::Executables();
  }

  TypedReg Result() override {
    // call function definition and return it result
    auto result = be.AddTReg(VALUE_TYPE);
    auto inArgs = be.AddTRegList();
    auto outArgs = be.AddTRegList();
    outArgs->Add(result);
    be.EmitCallSeq(func, inArgs, outArgs);
    return result;
  }

  void EndModule() override {
    func->Definition();
    func->Directive().linkage() = linkage;
    func->StartFunctionBody();
    be.EmitStore(BRIG_SEGMENT_ARG, VALUE_TYPE, be.Immed(VALUE_TYPE, VALUE), be.Address(arg->Variable()));
    func->EndFunction();
    Test::EndModule();
  }
};

class ModuleScopeKernelLinkageTest : public Test {
private:
  BrigLinkage linkage;

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

public:
  ModuleScopeKernelLinkageTest(BrigLinkage linkage_) : Test(Location::KERNEL), linkage(linkage_) {}

  void Name(std::ostream& out) const override {
    out << linkage2str(linkage) << "/kernel";
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void Executables() override {
    kernel->Declaration();
    kernel->Directive().linkage() = linkage;
    be.StartModuleScope();
    Test::Executables();
  }

  TypedReg Result() override {
    // return VALUE from kernel definition
    return be.AddInitialTReg(VALUE_TYPE, VALUE);
  }
   
  void EndProgram() override {
    kernel->Directive().linkage() = linkage;
    Test::EndProgram();
  }

  void SetupDispatch(const std::string& dispatchId) override {
    Test::SetupDispatch(dispatchId);
    if (linkage == BRIG_LINKAGE_MODULE) {
      te->InitialContext()->Put(dispatchId, "main_module_name", module->Id());
    }
  }
};

class ModuleScopeFBarrierLinkageTest : public Test {
private:
  FBarrier fbar;

  BrigLinkage linkage;

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

public:
  ModuleScopeFBarrierLinkageTest(BrigLinkage linkage_) : Test(Location::KERNEL), linkage(linkage_) {}

  void Name(std::ostream& out) const override {
    out << linkage2str(linkage) << "/fbarrier";
  }

  void Init() override {
    Test::Init();
    fbar = te->NewFBarrier("fbar", Location::MODULE);
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void ModuleVariables() override {
    Test::ModuleVariables();
    fbar->EmitDeclaration();
    fbar->FBarrier().linkage() = linkage;
  }

  TypedReg Result() override {
    fbar->EmitInitfbarInFirstWI();
    fbar->EmitJoinfbar();
    be.EmitBarrier();
    fbar->EmitWaitfbar();
    be.EmitBarrier();
    fbar->EmitLeavefbar();
    be.EmitBarrier();
    fbar->EmitReleasefbarInFirstWI();
    // return VALUE from kernel definition
    return be.AddInitialTReg(VALUE_TYPE, VALUE);
  }

  void EndModule() override {
    fbar->EmitDefinition();
    fbar->FBarrier().linkage() = linkage;
    Test::EndModule();
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
  EFunction* FirstFunction() const { return function; }
  EFunction* SecondFunction() const { return secondFunction; }
  uint32_t ResultValue() const { return VALUE; }

  virtual void SecondKernelBody() {}
  virtual void FirstFunctionBody() {}
  virtual void SecondFunctionBody() {}

public:
  FunctionLinkageTest() : Test(Location::KERNEL) {}

  void Init() override {
    Test::Init();
    secondKernel = te->NewKernel("second_kernel");
    function = te->NewFunction("first_function");
    secondFunction = te->NewFunction("second_function");
  }

  void Name(std::ostream& out) const override {
    out << "function";
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void Executables() override {
    // emit first function
    function->Definition();
    function->StartFunctionBody();
    function->FunctionVariables();
    FirstFunctionBody();
    function->EndFunction();

    // emit second function
    secondFunction->Definition();
    secondFunction->StartFunctionBody();
    secondFunction->FunctionVariables();
    SecondFunctionBody();
    secondFunction->EndFunction();

    // emit second kernel
    secondKernel->Definition();
    secondKernel->StartKernelBody();
    secondKernel->KernelVariables();
    SecondKernelBody();
    secondKernel->EndKernel();

    Test::Executables();
  }
};

class FunctionLinkageVariableTest : public FunctionLinkageTest {
private:
  Variable firstKernelVar;
  Variable secondKernelVar;
  Variable firstFunctionVar;
  Variable secondFunctionVar;

  BrigSegment segment;

  static const uint32_t SECOND_VALUE = 987654321;
  static const uint32_t THIRD_VALUE = 456789123;
  static const uint32_t FOURTH_VALUE = 789456123;

protected:
  void SecondKernelBody() override {
    if (segment != BRIG_SEGMENT_READONLY) {
      be.EmitStore(segment, ResultType(), be.Immed(ResultType(), SECOND_VALUE), be.Address(secondKernelVar->Variable()));
    }
  }

  void FirstFunctionBody() override {
    if (segment != BRIG_SEGMENT_READONLY) {
      be.EmitStore(segment, ResultType(), be.Immed(ResultType(), THIRD_VALUE), be.Address(firstFunctionVar->Variable()));
    }
  }

  void SecondFunctionBody() override {
    if (segment != BRIG_SEGMENT_READONLY) {
      be.EmitStore(segment, ResultType(), be.Immed(ResultType(), FOURTH_VALUE), be.Address(secondFunctionVar->Variable()));
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
    FunctionLinkageTest::Name(out);
    out << "/variable/" << segment2str(segment);
  }

  void Init() override {
    FunctionLinkageTest::Init();
    firstKernelVar = kernel->NewVariable("var", segment, ResultType(), Location::KERNEL);
    secondKernelVar = SecondKernel()->NewVariable("var", segment, ResultType(), Location::KERNEL);
    firstFunctionVar = FirstFunction()->NewVariable("var", segment, ResultType(), Location::FUNCTION);
    secondFunctionVar = SecondFunction()->NewVariable("var", segment, ResultType(), Location::FUNCTION);
    if (segment == BRIG_SEGMENT_READONLY) {
      firstKernelVar->AddData(Value(Brig2ValueType(ResultType()), ResultValue()));
      secondKernelVar->AddData(Value(Brig2ValueType(ResultType()), SECOND_VALUE));
      firstFunctionVar->AddData(Value(Brig2ValueType(ResultType()), THIRD_VALUE));
      secondFunctionVar->AddData(Value(Brig2ValueType(ResultType()), FOURTH_VALUE));
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
    secondKernelVar->Variable().linkage() = BRIG_LINKAGE_FUNCTION;
    firstFunctionVar->Variable().linkage() = BRIG_LINKAGE_FUNCTION;
    secondFunctionVar->Variable().linkage() = BRIG_LINKAGE_FUNCTION;
    FunctionLinkageTest::EndProgram();
  }
};

class FunctionLinkageKernargTest : public FunctionLinkageTest {
private:
  Variable firstKernelArg;
  Variable secondKernelArg;

  static const uint32_t SECOND_VALUE = 987654321;

protected:
  void SecondKernelBody() override {
    auto tmp = be.AddTReg(ResultType());
    be.EmitLoad(BRIG_SEGMENT_KERNARG, tmp, be.Address(secondKernelArg->Variable()));
  }

public:
  FunctionLinkageKernargTest(bool) : FunctionLinkageTest() {}

  void Init() override {
    FunctionLinkageTest::Init();
    firstKernelArg = kernel->NewVariable("arg", BRIG_SEGMENT_KERNARG, ResultType(), Location::KERNEL);
    secondKernelArg = SecondKernel()->NewVariable("arg", BRIG_SEGMENT_KERNARG, ResultType(), Location::KERNEL);
    firstKernelArg->AddData(Value(Brig2ValueType(ResultType()), ResultValue()));
    secondKernelArg->AddData(Value(Brig2ValueType(ResultType()), SECOND_VALUE));
  }

  void Name(std::ostream& out) const override {
    FunctionLinkageTest::Name(out);
    out << "/variable/kernarg";
  }

  TypedReg Result() override {
    // load from variable and return it
    auto result = be.AddTReg(ResultType());
    be.EmitLoad(BRIG_SEGMENT_KERNARG, result, be.Address(firstKernelArg->Variable()));
    return result;
  }

  void EndProgram() override {
    firstKernelArg->Variable().linkage() = BRIG_LINKAGE_FUNCTION;
    secondKernelArg->Variable().linkage() = BRIG_LINKAGE_FUNCTION;
    FunctionLinkageTest::EndProgram();
  }
};

class FunctionLinkageArgTest : public FunctionLinkageTest {
private:
  Variable firstFunctionOutArg;
  Variable secondFunctionOutArg;

  static const uint32_t SECOND_VALUE = 987654321;

protected:
  void FirstFunctionBody() override {
    // return VALUE from first function
    be.EmitStore(BRIG_SEGMENT_ARG, ResultType(), be.Immed(ResultType(), ResultValue()), be.Address(firstFunctionOutArg->Variable()));
  }

  void SecondFunctionBody() override {
    // return SECOND_VALUE from first function
    be.EmitStore(BRIG_SEGMENT_ARG, ResultType(), be.Immed(ResultType(), SECOND_VALUE), be.Address(secondFunctionOutArg->Variable()));
  }

public:
  FunctionLinkageArgTest(bool) : FunctionLinkageTest() {}

  void Init() override {
    FunctionLinkageTest::Init();
    firstFunctionOutArg = FirstFunction()->NewVariable("out", BRIG_SEGMENT_ARG, ResultType(), Location::FUNCTION, BRIG_ALIGNMENT_NONE, 0, false, true);
    secondFunctionOutArg = SecondFunction()->NewVariable("out", BRIG_SEGMENT_ARG, ResultType(), Location::FUNCTION, BRIG_ALIGNMENT_NONE, 0, false, true);
  }

  void Name(std::ostream& out) const override {
    FunctionLinkageTest::Name(out);
    out << "/variable/arg";
  }

  TypedReg Result() override {
    auto result = be.AddTReg(ResultType());
    auto inArgs = be.AddTRegList();
    auto outArgs = be.AddTRegList();
    outArgs->Add(result);
    be.EmitCallSeq(FirstFunction(), inArgs, outArgs);
    return result;
  }

  void EndProgram() override {
    firstFunctionOutArg->Variable().linkage() = BRIG_LINKAGE_FUNCTION;
    secondFunctionOutArg->Variable().linkage() = BRIG_LINKAGE_FUNCTION;
    FunctionLinkageTest::EndProgram();
  }
};

class FunctionLinkageFBarrierTest : public FunctionLinkageTest {
private:
  FBarrier firstFunctionFBarrier;
  Variable firstFunctionFBarrierOutArg;
  FBarrier secondFunctionFBarrier;
  Variable secondFunctionFBarrierOutArg;

  static const uint32_t SECOND_VALUE = 987654321;

protected:
  void FirstFunctionBody() override {
    firstFunctionFBarrier->EmitInitfbarInFirstWI();
    firstFunctionFBarrier->EmitJoinfbar();
    be.EmitBarrier();
    firstFunctionFBarrier->EmitWaitfbar();
    auto addr = be.AddTReg(BRIG_TYPE_U32);
    firstFunctionFBarrier->EmitLdf(addr);
    be.EmitStore(BRIG_SEGMENT_ARG, addr, be.Address(firstFunctionFBarrierOutArg->Variable()));
  }

  void SecondFunctionBody() override {
    secondFunctionFBarrier->EmitInitfbarInFirstWI();
    secondFunctionFBarrier->EmitJoinfbar();
    be.EmitBarrier();
    secondFunctionFBarrier->EmitWaitfbar();
    be.EmitBarrier();
    secondFunctionFBarrier->EmitLeavefbar();
    be.EmitBarrier();
    secondFunctionFBarrier->EmitReleasefbarInFirstWI();
    auto addr = be.AddTReg(BRIG_TYPE_U32);
    secondFunctionFBarrier->EmitLdf(addr);
    be.EmitStore(BRIG_SEGMENT_ARG, addr, be.Address(secondFunctionFBarrierOutArg->Variable()));
  }

public:
  FunctionLinkageFBarrierTest(bool) : FunctionLinkageTest() {}

  void Init() override {
    FunctionLinkageTest::Init();
    firstFunctionFBarrier = FirstFunction()->NewFBarrier("fbar", Location::FUNCTION);
    firstFunctionFBarrierOutArg = FirstFunction()->NewVariable("fbar_addr", BRIG_SEGMENT_ARG, 
      BRIG_TYPE_U32, Location::FUNCTION, BRIG_ALIGNMENT_NONE, 0, false, true);
    secondFunctionFBarrier = SecondFunction()->NewFBarrier("fbar", Location::FUNCTION);
    secondFunctionFBarrierOutArg = SecondFunction()->NewVariable("fbar_addr", BRIG_SEGMENT_ARG, 
      BRIG_TYPE_U32, Location::FUNCTION, BRIG_ALIGNMENT_NONE, 0, false, true);
  }

  void Name(std::ostream& out) const override {
    FunctionLinkageTest::Name(out);
    out << "/fbarrier";
  }

  TypedReg Result() override {
    // call first function
    auto fbarAddr = be.AddTReg(BRIG_TYPE_U32);
    auto inArgs = be.AddTRegList();
    auto outArgs = be.AddTRegList();
    outArgs->Add(fbarAddr);
    be.EmitCallSeq(FirstFunction(), inArgs, outArgs);

    // leave and release fbrarier
    be.EmitBarrier();
    be.EmitLeavefbar(fbarAddr);
    be.EmitBarrier();
    be.EmitReleasefbarInFirstWI(fbarAddr);
    return be.AddInitialTReg(ResultType(), ResultValue());
  }

  void EndProgram() override {
    firstFunctionFBarrier->FBarrier().linkage() = BRIG_LINKAGE_FUNCTION;
    secondFunctionFBarrier->FBarrier().linkage() = BRIG_LINKAGE_FUNCTION;
    FunctionLinkageTest::EndProgram();
  }
};

class ArgLinkageTest: public Test {
private:
  Variable firstVar;
  Variable secondVar;
  Variable functionInput;
  Variable functionVar;
  
  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;
  static const uint32_t SECOND_VALUE = 987654321;
  static const uint32_t THIRD_VALUE = 456789123;
  
public:
  ArgLinkageTest(bool): Test(Location::KERNEL) {}

  void Init() override {
    Test::Init();
    function = te->NewFunction("function");
    functionInput = function->NewVariable("input", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::FUNCTION);
    functionVar = function->NewVariable("var", BRIG_SEGMENT_GLOBAL, VALUE_TYPE, Location::FUNCTION);
    functionVar->AddData(Value(Brig2ValueType(VALUE_TYPE), THIRD_VALUE));
    firstVar = te->NewVariable("var", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::ARGSCOPE);
    secondVar = te->NewVariable("var", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::ARGSCOPE);
  } 

  void Name(std::ostream& out) const override {
    out << "arg";
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void Executables() override {
    //emit function
    function->Definition();
    function->StartFunctionBody();
    function->FunctionVariables();
    function->EndFunction();
    Test::Executables();
  }

  TypedReg Result() override {
    auto result = be.AddTReg(VALUE_TYPE);

    // first arg scope: store VALUE in result
    be.StartArgScope();
    firstVar->EmitDefinition();
    be.EmitStore(firstVar->Segment(), VALUE_TYPE, be.Immed(VALUE_TYPE, VALUE), be.Address(firstVar->Variable()));
    be.EmitLoad(firstVar->Segment(), result, be.Address(firstVar->Variable()));
    ItemList ins;
    ins.push_back(firstVar->Variable());
    be.EmitCall(function->Directive(), ins, ItemList());
    be.EndArgScope();

    // second arg scope
    be.StartArgScope();
    secondVar->EmitDefinition();
    be.EmitStore(secondVar->Segment(), VALUE_TYPE, be.Immed(VALUE_TYPE, SECOND_VALUE), be.Address(secondVar->Variable()));
    ins.clear();
    ins.push_back(secondVar->Variable());
    be.EmitCall(function->Directive(), ins, ItemList());
    be.EndArgScope();

    return result;
  }

  void EndProgram() override {
    firstVar->Variable().linkage() = BRIG_LINKAGE_ARG;
    secondVar->Variable().linkage() = BRIG_LINKAGE_ARG;
    Test::EndProgram();
  }
};

class NoneLinkageTest: public Test {
private:
  Variable firstArg;
  Variable secondArg;

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;

public:
  NoneLinkageTest(bool) : Test(Location::KERNEL) {}

  void Name(std::ostream& out) const override {
    out << "none";
  }

  void Init() override {
    Test::Init();
    firstArg = te->NewVariable("arg", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::FUNCTION);
    secondArg = te->NewVariable("arg", BRIG_SEGMENT_ARG, VALUE_TYPE, Location::FUNCTION);
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }

  void Executables() override {
    // emit first signature
    be.Brigantine().declSignature("&first_sig");
    be.StartFunctioinArgScope();
    firstArg->EmitDefinition();

    // emit second signature
    auto signature = be.Brigantine().declSignature("&second_sig");
    (void)signature; /// \todo -warn
    be.StartFunctioinArgScope();
    secondArg->EmitDefinition();
    be.StartModuleScope();
    Test::Executables();
  }

  TypedReg Result() override {
    return be.AddInitialTReg(VALUE_TYPE, VALUE);
  }
};


void LibrariesTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string path;
  Arena* ap = cc->Ap();

  TestForEach<ModuleScopeVariableLinkageTest>(ap, it, "linkage", cc->Variables().ModuleScopeLinkage(), cc->Segments().ModuleScopeVariableSegments());
  TestForEach<ModuleScopeFunctionLinkageTest>(ap, it, "linkage", cc->Variables().ModuleScopeLinkage());
  TestForEach<ModuleScopeKernelLinkageTest>(ap, it, "linkage", cc->Variables().ModuleScopeLinkage());
  TestForEach<ModuleScopeFBarrierLinkageTest>(ap, it, "linkage", cc->Variables().ModuleScopeLinkage());

  TestForEach<FunctionLinkageVariableTest>(ap, it, "linkage", cc->Segments().FunctionScopeVariableSegments());
  TestForEach<FunctionLinkageKernargTest>(ap, it, "linkage", Bools::Value(true));
  TestForEach<FunctionLinkageArgTest>(ap, it, "linkage", Bools::Value(true));
  TestForEach<FunctionLinkageFBarrierTest>(ap, it, "linkage", Bools::Value(true));

  TestForEach<ArgLinkageTest>(ap, it, "linkage", Bools::Value(true));

  TestForEach<NoneLinkageTest>(ap, it, "linkage", Bools::Value(true));
}

}
