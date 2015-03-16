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
#include "UtilTests.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace HSAIL_ASM;
using namespace hsail_conformance::utils;

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

    // emit some dumb code that do nothing
    auto tmp = be.AddTReg(result->Type());
    be.EmitArith(BRIG_OPCODE_MUL, tmp, result, be.Immed(result->Type(), 123456789));
    be.EmitNop();

    if (annotationLocation == AnnotationLocation::MIDDLE_ARG_BLOCK) {
      EmitAnnotation();
    }

    be.EmitArith(BRIG_OPCODE_ADD, tmp, tmp, result->Reg());
    be.EmitArith(BRIG_OPCODE_SUB, tmp, tmp, tmp->Reg());
    be.EmitArith(BRIG_OPCODE_ADD, result, result, tmp->Reg());
    be.EmitCallSeq(empty_function, be.AddTRegList(), be.AddTRegList());

    if (annotationLocation == AnnotationLocation::END_ARG_BLOCK) {
      EmitAnnotation();
    }
    be.EndArgScope();
  }

protected:
  virtual void EmitAnnotation() = 0;
  AnnotationLocation GetAnnotationLocation() { return annotationLocation; }

public:
  explicit AnnotationLocationTest(AnnotationLocation annotationLocation_)
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

    SkipTest::ModuleVariables();
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
  explicit LocDirectiveLocationTest(AnnotationLocation locLocation_)
    : AnnotationLocationTest(locLocation_) { }
};


class PragmaGenerator {
private:
  uint64_t numberPragma;
  char charStr;
  char charName;

  static const int STR_LENGTH = 5;
public:
  PragmaGenerator(): numberPragma(1), charStr('a'), charName('a') {}

  uint64_t GenerateNumber() { return numberPragma++; }
  std::string GenerateString() { 
    std::string str(STR_LENGTH, charStr);
    ++charStr;
    if (charStr > 'z') {
      charStr = 'a';
    }
    return str;
  }
  std::string GenerateIdentified() { 
    std::string name(STR_LENGTH, charName);
    ++charName;
    if (charName > 'z') {
      charName = 'a';
    }
    return name;
  }
};

class PragmaDirectiveLocationTest : public AnnotationLocationTest {
private:
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


class PragmaOperandTypesTest : public SkipTest {
private:
  PragmaGenerator generator;
  BrigKind type1, type2, type3;
  HSAIL_ASM::Operand op1, op2, op3;
  Variable var1, var2, var3;

  HSAIL_ASM::Operand InitializeOperand(BrigKind type) {
    switch (type) {
    case BRIG_KIND_OPERAND_CODE_REF: return HSAIL_ASM::Operand();
    case BRIG_KIND_OPERAND_CONSTANT_BYTES: return be.Immed(BRIG_TYPE_U64, generator.GenerateNumber());
    case BRIG_KIND_OPERAND_STRING: return be.ImmedString(generator.GenerateString());
    default:
      assert(false); return HSAIL_ASM::Operand();
    }
  }

  static std::string OperandType2String(BrigKind type) {
    switch (type) {
    case BRIG_KIND_OPERAND_CODE_REF: return "identifier";
    case BRIG_KIND_OPERAND_CONSTANT_BYTES: return "integer";
    case BRIG_KIND_OPERAND_STRING: return "string";
    default:
      assert(false); return "";
    }
  }

public:
  PragmaOperandTypesTest(BrigKind type1_, BrigKind type2_, BrigKind type3_)
    : SkipTest(Location::KERNEL), type1(type1_), type2(type2_), type3(type3_) { }

  bool IsValid() const override {
    return SkipTest::IsValid() &&
           (type1 == BRIG_KIND_OPERAND_CONSTANT_BYTES || type1 == BRIG_KIND_OPERAND_STRING || type1 == BRIG_KIND_OPERAND_CODE_REF) &&
           (type2 == BRIG_KIND_OPERAND_CONSTANT_BYTES || type2 == BRIG_KIND_OPERAND_STRING || type2 == BRIG_KIND_OPERAND_CODE_REF) &&
           (type3 == BRIG_KIND_OPERAND_CONSTANT_BYTES || type3 == BRIG_KIND_OPERAND_STRING || type3 == BRIG_KIND_OPERAND_CODE_REF);
  }

  void Init() override {
    SkipTest::Init();
    var1 = kernel->NewVariable(generator.GenerateIdentified(), BRIG_SEGMENT_GROUP, BRIG_TYPE_U64);
    var2 = kernel->NewVariable(generator.GenerateIdentified(), BRIG_SEGMENT_GROUP, BRIG_TYPE_U64);
    var3 = kernel->NewVariable(generator.GenerateIdentified(), BRIG_SEGMENT_GROUP, BRIG_TYPE_U64);
    op1 = InitializeOperand(type1);
    op2 = InitializeOperand(type2);
    op3 = InitializeOperand(type3);
  }

  void Name(std::ostream& out) const override {
    out << OperandType2String(type1) << "_" << 
           OperandType2String(type2) << "_" << 
           OperandType2String(type3);
  }

  TypedReg Result() override{
    if (type1 == BRIG_KIND_OPERAND_CODE_REF) {
      op1 = be.Brigantine().createCodeRef(var1->Variable());
    }
    if (type2 == BRIG_KIND_OPERAND_CODE_REF) {
      op2 = be.Brigantine().createCodeRef(var2->Variable());
    }
    if (type3 == BRIG_KIND_OPERAND_CODE_REF) {
      op3 = be.Brigantine().createCodeRef(var3->Variable());
    }
    be.EmitPragmaDirective(be.Operands(op1, op2, op3));
    return SkipTest::Result();
  }
};


class ControlDirectivesLocationTest: public SkipTest {
private:
  BrigControlDirective directive;

protected:
  virtual void EmitControlDirective() = 0;
  BrigControlDirective Directive() const { return directive; }

public:
  ControlDirectivesLocationTest(Location codeLocation_, BrigControlDirective directive_): 
    SkipTest(codeLocation_), directive(directive_) {}

  void Name(std::ostream& out) const override {
    out << Dir2Str(directive) << "_" << CodeLocationString();    
  }

  void KernelDirectives() override {
    if (codeLocation == Location::KERNEL) {
      EmitControlDirective();
    }
  }

  void FunctionDirectives() override {
    if (codeLocation == Location::FUNCTION) {
      EmitControlDirective();
    }
  }
};


class ExceptionDirectivesLocationTest: public ControlDirectivesLocationTest {
private:
  static const uint32_t EXCEPTION_NUMBER = 0x1F;

protected:
  void EmitControlDirective() override {
    be.EmitEnableExceptionDirective(Directive() == BRIG_CONTROL_ENABLEBREAKEXCEPTIONS, EXCEPTION_NUMBER);
  }

public:
  ExceptionDirectivesLocationTest(Location codeLocation_, BrigControlDirective directive_): 
    ControlDirectivesLocationTest(codeLocation_, directive_) {}

  void Name(std::ostream& out) const override {
    if (Directive() == BRIG_CONTROL_ENABLEBREAKEXCEPTIONS) { 
      out << "break_";
    } else { 
      out << "detect_";
    }
    out << CodeLocationString();    
  }

  bool IsValid() const override {
    return ControlDirectivesLocationTest::IsValid() &&
           (Directive() == BRIG_CONTROL_ENABLEBREAKEXCEPTIONS || Directive() == BRIG_CONTROL_ENABLEDETECTEXCEPTIONS);
  }
};


class ExceptionDirectivesArgumentTest : public SkipTest {
private:
  bool isBreak;
  uint32_t exceptionNumber;

public:
  ExceptionDirectivesArgumentTest(bool isBreak_, uint32_t exceptionNumber_)
    : SkipTest(Location::KERNEL), isBreak(isBreak_), exceptionNumber(exceptionNumber_) { }

  bool IsValid() const override {
    return SkipTest::IsValid() &&
           exceptionNumber <= 0x1F; // 0b11111
  }

  void Name(std::ostream& out) const override {
    if (isBreak) { 
      out << "break_";
    } else { 
      out << "detect_";
    }
    // 'v' - INVALID_OPERATION, 'd' - DIVIDE_BY_ZERO, 'o' - OVERFLOW, 'u' - underflow, 'e' - INEXACT
    if ((exceptionNumber & 0x10) != 0) { out << 'e'; }
    if ((exceptionNumber & 0x08) != 0) { out << 'u'; }
    if ((exceptionNumber & 0x04) != 0) { out << 'o'; }
    if ((exceptionNumber & 0x02) != 0) { out << 'd'; }
    if ((exceptionNumber & 0x01) != 0) { out << 'v'; }
    if (exceptionNumber == 0) { out << '0'; }
  }

  TypedReg Result() override{
    
    return SkipTest::Result();
  }

  void KernelDirectives() override {
    be.EmitEnableExceptionDirective(isBreak, exceptionNumber);
  }
};


class GeometryDirectivesLocationTest: public ControlDirectivesLocationTest {
protected:
  void EmitControlDirective() override {
    be.EmitControlDirectiveGeometry(Directive(), geometry);
  }

public:
  GeometryDirectivesLocationTest(Location codeLocation_, BrigControlDirective directive_): 
    ControlDirectivesLocationTest(codeLocation_, directive_) {}

  bool IsValid() const override {
    return ControlDirectivesLocationTest::IsValid() &&
           (Directive() == BRIG_CONTROL_MAXFLATGRIDSIZE || 
            Directive() == BRIG_CONTROL_MAXFLATWORKGROUPSIZE ||
            Directive() == BRIG_CONTROL_REQUIREDDIM ||
            Directive() == BRIG_CONTROL_REQUIREDGRIDSIZE ||
            Directive() == BRIG_CONTROL_REQUIREDWORKGROUPSIZE ||
            Directive() == BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS);
  }
};

class MaxDynamicGroupSizeLocationTest: public ControlDirectivesLocationTest {
protected:
  void EmitControlDirective() override {
    be.EmitDynamicMemoryDirective(0);
  }

public:
  MaxDynamicGroupSizeLocationTest(Location codeLocation_): 
    ControlDirectivesLocationTest(codeLocation_, BRIG_CONTROL_MAXDYNAMICGROUPSIZE) {}
};


class ExtensionDirectiveTest: public SkipTest {
private:
  std::string extensionName;

public:
  explicit ExtensionDirectiveTest(std::string extensionName_): 
    SkipTest(Location::KERNEL), extensionName(extensionName_) {}

  bool IsValid() const override {
    return SkipTest::IsValid() &&
           (extensionName == "CORE" || extensionName == "IMAGE" || extensionName == "");
  }

  void Name(std::ostream& out) const override {
    if (extensionName.empty()) {
      out << "empty";
    } else {
      out << extensionName;
    }
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective(extensionName);
  }
};


class ExtensionDirectivePairTest: public SkipTest {
private:
  std::string extensionName1;
  std::string extensionName2;

public:
  ExtensionDirectivePairTest(std::string extensionName1_, std::string extensionName2_): 
    SkipTest(Location::KERNEL), extensionName1(extensionName1_), extensionName2(extensionName2_) {}

  bool IsValid() const override {
    return SkipTest::IsValid() &&
           (extensionName1 == "IMAGE" || extensionName1 == "") &&
           (extensionName2 == "IMAGE" || extensionName2 == "");
  }

  void Name(std::ostream& out) const override {
    if (extensionName1.empty()) {
      out << "empty_";
    } else {
      out << extensionName1 << "_";
    }
    if (extensionName2.empty()) {
      out << "empty";
    } else {
      out << extensionName2;
    }
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective(extensionName1);
    be.EmitExtensionDirective(extensionName2);
  }
};


void DirectiveTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string path;
  Arena* ap = cc->Ap();

  TestForEach<LocDirectiveLocationTest>(ap, it, "loc/locations", AnnotationLocations());
  TestForEach<PragmaDirectiveLocationTest>(ap, it, "pragma/locations", AnnotationLocations());
  TestForEach<PragmaOperandTypesTest>(ap, it, "pragma/optypes", cc->Directives().PragmaOperandTypes(), cc->Directives().PragmaOperandTypes(), cc->Directives().PragmaOperandTypes());

  TestForEach<GeometryDirectivesLocationTest>(ap, it, "control/geometry/location", CodeLocations(), cc->Directives().GeometryDirectives());
  TestForEach<ExceptionDirectivesLocationTest>(ap, it, "control/exception/location", CodeLocations(), cc->Directives().ExceptionDirectives());
  TestForEach<MaxDynamicGroupSizeLocationTest>(ap, it, "control/maxdynamic/location", CodeLocations());

  TestForEach<ExceptionDirectivesArgumentTest>(ap, it, "control/exception/argument", Bools::All(), cc->Directives().ValidExceptionNumbers());

  TestForEach<ExtensionDirectiveTest>(ap, it, "extension/names", cc->Directives().ValidExtensions());
  TestForEach<ExtensionDirectivePairTest>(ap, it, "extension/pair", cc->Directives().ValidExtensions(), cc->Directives().ValidExtensions());
}

}
