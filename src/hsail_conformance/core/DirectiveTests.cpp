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

#include "DirectiveTests.hpp"
#include "HCTests.hpp"
#include "BrigEmitter.hpp"
#include "CoreConfig.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace Brig;
using namespace HSAIL_ASM;

namespace hsail_conformance {

enum AnnotationLocation {
  ANNOTATION_LOCATION_BEGIN = 0,
  BEFORE_VERSION = ANNOTATION_LOCATION_BEGIN,
  AFTER_VERSION,
  END_MODULE,
  BEFORE_MODULE_VARIABLE,
  AFTER_MODULE_VARIABLE,
  START_KERNEL,
  END_KERNEL,
  MIDDLE_KERNEL,
  START_FUNCTION,
  END_FUNCTION,
  MIDDLE_FUNCTION,
  START_ARG_BLOCK,
  END_ARG_BLOCK,
  MIDDLE_ARG_BLOCK,
  ANNOTATION_LOCATION_END
};

std::string AnnotationLocationString(AnnotationLocation location) {
  switch (location)
  {
  case AnnotationLocation::BEFORE_VERSION: return "before_version";
  case AnnotationLocation::AFTER_VERSION: return "after_version";
  case AnnotationLocation::END_MODULE: return "end_module";
  case AnnotationLocation::BEFORE_MODULE_VARIABLE: return "before_module_variable";
  case AnnotationLocation::AFTER_MODULE_VARIABLE: return "after_module_variable";
  case AnnotationLocation::START_KERNEL: return "start_kernel";
  case AnnotationLocation::END_KERNEL: return "end_kernel";
  case AnnotationLocation::MIDDLE_KERNEL: return "middle_of_kernel";
  case AnnotationLocation::START_FUNCTION: return "start_function";
  case AnnotationLocation::END_FUNCTION: return "end_function";
  case AnnotationLocation::MIDDLE_FUNCTION: return "middle_of_function";
  case AnnotationLocation::START_ARG_BLOCK: return "start_arg_block";
  case AnnotationLocation::END_ARG_BLOCK: return "end_arg_block";
  case AnnotationLocation::MIDDLE_ARG_BLOCK: return "middle_of_arg_block";
  default: assert(false); return "";
  }
}

bool NeedsArgBlock(AnnotationLocation location) {
  if (location == AnnotationLocation::START_ARG_BLOCK || 
      location == AnnotationLocation::END_ARG_BLOCK ||
      location == AnnotationLocation::MIDDLE_ARG_BLOCK) 
  {
    return true;
  } else {
    return false;
  }
}

Sequence<AnnotationLocation>* AnnotationLocations() {
  static EnumSequence<AnnotationLocation> sequence(AnnotationLocation::ANNOTATION_LOCATION_BEGIN, 
                                                   AnnotationLocation::ANNOTATION_LOCATION_END);
  return &sequence;
}


class SkipTest : public Test {
public:
  SkipTest(Location codeLocation, Grid geometry = 0)
    : Test(codeLocation, geometry) { }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 0); }

  TypedReg Result() override {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(BRIG_TYPE_U32, 0));
    return result;
  }
};


class AnnotationLocationTest : public SkipTest {
private:
  AnnotationLocation annotationLocation;
  Variable var1;
  Variable var2;
  EFunction* empty_function;


  static Location CodeLocation(AnnotationLocation locLocation) {
    switch (locLocation)
    {
    case AnnotationLocation::BEFORE_VERSION: return Location::KERNEL;
    case AnnotationLocation::AFTER_VERSION: return Location::KERNEL;
    case AnnotationLocation::END_MODULE: return Location::KERNEL;
    case AnnotationLocation::BEFORE_MODULE_VARIABLE: return Location::KERNEL;
    case AnnotationLocation::AFTER_MODULE_VARIABLE: return Location::KERNEL;
    case AnnotationLocation::START_KERNEL: return Location::KERNEL;
    case AnnotationLocation::END_KERNEL: return Location::KERNEL;
    case AnnotationLocation::MIDDLE_KERNEL: return Location::KERNEL;
    case AnnotationLocation::START_FUNCTION: return Location::FUNCTION;
    case AnnotationLocation::END_FUNCTION: return Location::FUNCTION;
    case AnnotationLocation::MIDDLE_FUNCTION: return Location::FUNCTION;
    case AnnotationLocation::START_ARG_BLOCK: return Location::KERNEL;
    case AnnotationLocation::END_ARG_BLOCK: return Location::KERNEL;
    case AnnotationLocation::MIDDLE_ARG_BLOCK: return Location::KERNEL;
    default: assert(false); return Location::KERNEL;
    }
  }

  void EmitArgBlock(TypedReg result) {
    be.StartArgScope();
    if (annotationLocation == AnnotationLocation::START_ARG_BLOCK) {
      EmitAnnotation();
    }

    // emit some dumb code that do nothing (in case we know that result contain 0)
    auto tmp = be.AddTReg(result->Type());
    be.EmitArith(BRIG_OPCODE_MUL, tmp, result, be.Immed(result->Type(), 123456789));
    be.EmitNop();

    if (annotationLocation == AnnotationLocation::MIDDLE_ARG_BLOCK) {
      EmitAnnotation();
    }

    be.EmitArith(BRIG_OPCODE_ADD, tmp, tmp, result->Reg());
    be.EmitMov(result, tmp);
    be.EmitCall(empty_function->Directive(), ItemList(), ItemList());

    if (annotationLocation == AnnotationLocation::END_ARG_BLOCK) {
      EmitAnnotation();
    }
    be.EndArgScope();
  }

protected:
  virtual void EmitAnnotation() = 0;
  AnnotationLocation GetAnnotationLocation() { return annotationLocation; }

public:
  AnnotationLocationTest(AnnotationLocation annotationLocation_)
    : SkipTest(CodeLocation(annotationLocation_)), annotationLocation(annotationLocation_) { }

  void Init() override {
    SkipTest::Init();
    var1 = kernel->NewVariable("var1", BRIG_SEGMENT_GLOBAL, BRIG_TYPE_U32); 
    var2 = kernel->NewVariable("var2", BRIG_SEGMENT_GLOBAL, BRIG_TYPE_U32);
    empty_function = te->NewFunction("empty_function");
  }

  void Name(std::ostream& out) const override {
    out << AnnotationLocationString(annotationLocation);
  }

  TypedReg Result() override {
    auto result = SkipTest::Result();
    if (NeedsArgBlock(annotationLocation)) {
      EmitArgBlock(result);
    }
    if (annotationLocation == AnnotationLocation::MIDDLE_KERNEL || annotationLocation == AnnotationLocation::MIDDLE_FUNCTION) {
      EmitAnnotation();
    }
    return result;
  }

  void StartModule() override {
    if (annotationLocation == AnnotationLocation::BEFORE_VERSION) {
      EmitAnnotation();
    }
    SkipTest::StartModule();
    if (annotationLocation == AnnotationLocation::AFTER_VERSION) {
      EmitAnnotation();
    }
  }

  void EndModule() override {
    SkipTest::EndModule();
    if (annotationLocation == AnnotationLocation::END_MODULE) {
      EmitAnnotation();
    }
  }

  void StartKernel() override {
    // emit empty function if needed
    if (NeedsArgBlock(annotationLocation)) {
      empty_function->StartFunction();
      empty_function->StartFunctionBody();
      empty_function->EndFunction();
    }
    SkipTest::StartKernel();
  }

  void EndKernel() override {
    if (annotationLocation == AnnotationLocation::END_KERNEL) {
      EmitAnnotation();
    }
    SkipTest::EndKernel();
  }

  void StartKernelBody() override {
    SkipTest::StartKernelBody();
    if (annotationLocation == AnnotationLocation::START_KERNEL) {
      EmitAnnotation();
    }
  }

  void StartFunctionBody() override {
    SkipTest::StartFunctionBody();
    if (annotationLocation == AnnotationLocation::START_FUNCTION) {
      EmitAnnotation();
    }
  }

  void EndFunction() override {
    if (annotationLocation == AnnotationLocation::END_FUNCTION) {
      EmitAnnotation();
    }
    SkipTest::EndFunction();
  }

  void ModuleVariables() override {
    if (annotationLocation == AnnotationLocation::BEFORE_MODULE_VARIABLE) {
      EmitAnnotation();
    }

    var1->EmitDefinition();
    
    if (annotationLocation == AnnotationLocation::AFTER_MODULE_VARIABLE) {
      EmitAnnotation();
    }

    var2->EmitDefinition();
  }
};



class LocDirectiveLocationTest : public AnnotationLocationTest {
private:
  class LocGenerator {
  private:
    uint32_t lineCounter;
    uint32_t columnCounter;
    char charName;

    static const int NAME_LENGTH = 5;
    
  public:
    LocGenerator(): lineCounter(1), columnCounter(1), charName('a') {}

    uint32_t GenerateLineNum() { return lineCounter++; }
    uint32_t GenerateColumn() { return columnCounter++; }
    std::string GenerateFileName() { 
      std::string fileName(NAME_LENGTH, charName);
      ++charName;
      if (charName > 'z') {
        charName = 'a';
      }
      return fileName;
    }
  };

  LocGenerator generator;

protected:
  void EmitAnnotation() override {
    be.EmitLocDirective(generator.GenerateLineNum(), 
                        generator.GenerateColumn(), 
                        generator.GenerateFileName());
  }

public:
  LocDirectiveLocationTest(AnnotationLocation locLocation_)
    : AnnotationLocationTest(locLocation_) { }
};


class PragmaDirectiveLocationTest : public AnnotationLocationTest {
private:
  class PragmaGenerator {
  private:
    uint64_t numberPragma;
    char charStr;

    static const int STR_LENGTH = 5;
  public:
    PragmaGenerator(): numberPragma(1), charStr('a') {}

    uint32_t GenerateNumber() { return numberPragma++; }
    std::string GenerateString() { 
      std::string str(STR_LENGTH, charStr);
      ++charStr;
      if (charStr > 'z') {
        charStr = 'a';
      }
      return str;
    }
  };

  PragmaGenerator generator;

protected:
  void EmitAnnotation() override {
    be.EmitPragmaDirective(be.Operands(be.Immed(BRIG_TYPE_U64, generator.GenerateNumber()), 
                                       be.ImmedString(generator.GenerateString())));
  }

public:
  PragmaDirectiveLocationTest(AnnotationLocation pragmaLocation_)
    : AnnotationLocationTest(pragmaLocation_), generator() { }
};


void DirectiveTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string path;
  Arena* ap = cc->Ap();

  TestForEach<LocDirectiveLocationTest>(ap, it, "loc", AnnotationLocations());
  TestForEach<PragmaDirectiveLocationTest>(ap, it, "pragma", AnnotationLocations());
}

}
