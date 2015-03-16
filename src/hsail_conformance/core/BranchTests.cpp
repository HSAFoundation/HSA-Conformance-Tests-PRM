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

#include "BranchTests.hpp"
#include "BrigEmitter.hpp"
#include "BasicHexlTests.hpp"
#include "HCTests.hpp"
#include "Brig.h"
#include <sstream>

namespace hsail_conformance {
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

class BrBasicTest : public Test {
public:
  BrBasicTest(Location location, Grid geometry = 0)
  : Test(location, geometry), baseName("br/basic"), s_then("@then"), s_endif("@endif") {
    result = be.AddTReg(ResultType());
    immSuccess = be.Immed(ResultType(), success);
    immError = be.Immed(ResultType(), error);
 }

  void Name(std::ostream& out) const { out << baseName << "/" << CodeLocationString(); }

protected:
  std::string baseName;
  std::string s_then;
  std::string s_endif;
  static const uint64_t success = 1; // expected value
  static const uint64_t error = 2; // error code
  TypedReg result;
  OperandConstantBytes immSuccess;
  OperandConstantBytes immError;

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const { return Value(MV_UINT32, success); }

  TypedReg Result() {
    // mov_b32 $s0, 1;
    be.EmitMov(result->Reg(), immSuccess, result->TypeSizeBits());
    // br @then;
    be.EmitBr(s_then);
    // mov_b32 $s0, 2;
    be.EmitMov(result->Reg(), immError, result->TypeSizeBits());
    // @then:
    be.EmitLabel(s_then);
    return result;
  }
};

class ConditionTestBase : public Test {
protected:
  Condition cond;

public:
  ConditionTestBase(Location codeLocation, Grid geometry, Condition cond_)
    : Test(codeLocation, geometry), cond(cond_)
  {
    specList.Add(cond);
  }

  void Name(std::ostream& out) const {
    out << "cbr/basic/" << CodeLocationString() << "_" << cond;
  }

  BrigType ResultType() const { return BRIG_TYPE_U32; }
};

class CbrBasicTest : public ConditionTestBase {
public:
  CbrBasicTest(Location location, Grid geometry, Condition cond)
    : ConditionTestBase(location, geometry, cond) { }

  Value ExpectedResult(uint64_t i) const {
    return Value(MV_UINT32, cond->ExpectThenPath(i) ? 1 : 2);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(result->Type(), 2));
    cond->EmitIfThenStart();
    be.EmitMov(result, be.Immed(result->Type(), 1));
    cond->EmitIfThenEnd();
    return result;
  }
};

class CbrIfThenElseTest : public ConditionTestBase {

public:
  CbrIfThenElseTest(Location location, Grid geometry, Condition cond_)
    : ConditionTestBase(location, geometry, cond_) {}

  void Name(std::ostream& out) const {
    out << "cbr/ifthenelse/" << CodeLocationString() << "/" << cond;
  }

protected:
  Value ExpectedResult(uint64_t i) const {
    uint32_t eresult;
    if (cond->ExpectThenPath(i)) {
        eresult = 1;
    } else {
      eresult = 2;
    }
    return Value(MV_UINT32, eresult);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(result->Type(), 3));
    cond->EmitIfThenElseStart();
    be.EmitMov(result, be.Immed(result->Type(), 1));
    cond->EmitIfThenElseOtherwise();
    be.EmitMov(result, be.Immed(result->Type(), 2));
    cond->EmitIfThenElseEnd();
    return result;
  }
};

class CbrNestedTest : public ConditionTestBase {
protected:
  Condition cond2;

public:
  CbrNestedTest(Location location, Grid geometry, Condition cond_, Condition cond2_)
    : ConditionTestBase(location, geometry, cond_),
    cond2(cond2_)
  {
    specList.Add(cond2);
  }

  void Name(std::ostream& out) const {
    out << "cbr/nested/" << CodeLocationString() << "/" << cond << "_" << cond2;
  }

protected:
  Value ExpectedResult(uint64_t i) const {
    uint32_t eresult;
    if (cond->ExpectThenPath(i)) {
      if (cond2->ExpectThenPath(i)) {
        eresult = 1;
      } else {
        eresult = 2;
      }
    } else {
      eresult = 3;
    }
    return Value(MV_UINT32, eresult);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(result->Type(), 3));
    cond->EmitIfThenStart();
    be.EmitMov(result, be.Immed(result->Type(), 2));
    cond2->EmitIfThenStart();
    be.EmitMov(result, be.Immed(result->Type(), 1));
    cond2->EmitIfThenEnd();
    cond->EmitIfThenEnd();
    return result;
  }
};

class CbrIfThenElseNestedInThenTest : public CbrNestedTest {

public:
  CbrIfThenElseNestedInThenTest(Location location, Grid geometry, Condition cond_, Condition cond2_)
    : CbrNestedTest(location, geometry, cond_, cond2_) {}

  void Name(std::ostream& out) const {
    out << "cbr/ifthenelse/nested/inthen/" << CodeLocationString() << "/" << cond << "_" << cond2;
  }

protected:
  Value ExpectedResult(uint64_t i) const {
    uint32_t eresult;
    if (cond->ExpectThenPath(i)) {
      if (cond2->ExpectThenPath(i)) {
        eresult = 1;
      } else {
        eresult = 2;
      }
    } else {
      eresult = 3;
    }
    return Value(MV_UINT32, eresult);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(result->Type(), 4));
    cond->EmitIfThenElseStart();

      cond2->EmitIfThenElseStart();
      be.EmitMov(result, be.Immed(result->Type(), 1));
      cond2->EmitIfThenElseOtherwise();
      be.EmitMov(result, be.Immed(result->Type(), 2));
      cond2->EmitIfThenElseEnd();

    cond->EmitIfThenElseOtherwise();
    be.EmitMov(result, be.Immed(result->Type(), 3));
    cond->EmitIfThenElseEnd();
    return result;
  }
};

class CbrIfThenElseNestedInElseTest : public CbrNestedTest {

public:
  CbrIfThenElseNestedInElseTest(Location location, Grid geometry, Condition cond_, Condition cond2_)
    : CbrNestedTest(location, geometry, cond_, cond2_) {}

  void Name(std::ostream& out) const {
    out << "cbr/ifthenelse/nested/inelse/" << CodeLocationString() << "/" << cond << "_" << cond2;
  }

protected:
  Value ExpectedResult(uint64_t i) const {
    uint32_t eresult;
    if (cond->ExpectThenPath(i)) {
      eresult = 1;
    } else {
      if (cond2->ExpectThenPath(i)) {
        eresult = 2;
      } else {
        eresult = 3;
      }
    }
    return Value(MV_UINT32, eresult);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(result->Type(), 4));
    cond->EmitIfThenElseStart();
    be.EmitMov(result, be.Immed(result->Type(), 1));
    cond->EmitIfThenElseOtherwise();

      cond2->EmitIfThenElseStart();
      be.EmitMov(result, be.Immed(result->Type(), 2));
      cond2->EmitIfThenElseOtherwise();
      be.EmitMov(result, be.Immed(result->Type(), 3));
      cond2->EmitIfThenElseEnd();

    cond->EmitIfThenElseEnd();
    return result;
  }
};

/*
class CbrSandTest : public CbrNestedTest {
public:
  CbrSandTest(Location location, Grid geometry, Memory::Type memoryType_, BrigWidth width_, bool testThenBranch_, bool testSecondThenBranch_)
  : CbrNestedTest(location, geometry, memoryType_, width_, testThenBranch_),
    testSecondThenBranch(testSecondThenBranch_), s_success("@success"), s_error("@error") {
    baseName = "cbr/sand";
    c_y = be.AddTReg(GetBranchInstrType());
  }

  void Name(std::ostream& out) const { CbrNestedTest::Name(out); out << "_" << (testSecondThenBranch ? "then" : "else"); }

protected:
  bool testSecondThenBranch;
  std::string s_success;
  std::string s_error;
  TypedReg c_y;
  Operand src_y;

  Value ExpectedResult(uint64_t id) const { return ExpectedResult(); }
  Value ExpectedResult() const { return Value(MV_UINT32, success); }

  bool IsValid() const {
    if (Location::FUNCTION == codeLocation) return false;
    switch (memoryType) {
      // WAVESIZE is constant > 0, so cbr will always go by "then" branch and never by "else"
      // testThenBranch is performed in CbrNestedTest, thus no WAVESIZE tests in S-circuit test at all
      case MT_WAVESIZE:
      // conditional expressions are meaningless on constants
      case MT_IMMEDIATE: return false;
      default: break;
    }
    return true;
  }

  TypedReg Result() {
    uint64_t then_result = success;
    uint64_t else_result = error;
    // if "THEN" branch is testing
    uint64_t then_imm = 1;
    // if "ELSE" branch is testing
    if (!testThenBranch) then_imm = 0;
    // if second (Y) "THEN" branch is testing
    uint64_t then_imm_y = 1;
    // if second (Y) "ELSE" branch is testing
    if (!testSecondThenBranch) then_imm_y = 0;
    // SAND
    if (then_imm + then_imm_y == 2) {
      then_result = success;
      else_result = error;
    } else {
      then_result = error;
      else_result = success;
      if (0 == then_imm) {
        std::string s_temp = s_success;
        s_success = s_error;
        s_error = s_temp;
      }
    }
    OperandData thenImmOp = be.Brigantine().createImmed(then_imm, GetBranchInstrType());
    OperandData thenImmOp_y = be.Brigantine().createImmed(then_imm_y, GetBranchInstrType());
    Operand src = c->Reg();
    // mov_b1 $c0, then_imm; // then_imm = {1,0}
    be.EmitMov(src, thenImmOp, c->TypeSizeBits());
    src_y = c_y->Reg();
    // mov_b1 $c1, then_imm_y; // then_imm_y = {1,0}
    be.EmitMov(src_y, thenImmOp_y, c->TypeSizeBits());
    // cbr $c0, @then;
    be.EmitCbr(src, s_then, width);
    // br @error;
    be.EmitBr(s_error);
    // @then:
    be.Brigantine().addLabel(s_then);
    // cbr $c1, @success;
    be.EmitCbr(src_y, s_success, width);
    // @error:
    be.Brigantine().addLabel(s_error);
    // mov_b32 $s0, else_result; // else_result = {2,1}
    be.EmitMov(result->Reg(), be.Brigantine().createImmed(else_result, result->Type()), result->TypeSizeBits());
    // br @end_if;
    be.EmitBr(s_endif);
    // @success:
    be.Brigantine().addLabel(s_success);
    // mov_b32 $s0, then_result; // then_result = {1,2}
    be.EmitMov(result->Reg(), be.Brigantine().createImmed(then_result, result->Type()), result->TypeSizeBits());
    // @endif:
    be.Brigantine().addLabel(s_endif);
    return result;
  }
};

class CbrSorTest : public CbrSandTest {
public:
  CbrSorTest(Location location, Grid geometry, Memory::Type memoryType_, BrigWidth width_, bool testThenBranch_, bool testSecondThenBranch_)
  : CbrSandTest(location, geometry, memoryType_, width_, testThenBranch_, testSecondThenBranch_) { baseName = "cbr/sor"; }

  TypedReg Result() {
    // if "THEN" branch is testing
    uint64_t then_imm = 1;
    // if "ELSE" branch is testing
    if (!testThenBranch) then_imm = 0;
    // if second (Y) "THEN" branch is testing
    uint64_t then_imm_y = 1;
    // if second (Y) "ELSE" branch is testing
    if (!testSecondThenBranch) then_imm_y = 0;
    // SOR
    std::string s_else;
    if (then_imm == 1) {
      s_then = s_success;
      s_else = s_error;
    } else {
      s_then = s_error;
      if (then_imm_y) s_else = s_success;
      else            s_else = s_error;
    }
    if (then_imm + then_imm_y == 0)
      be.EmitMov(result->Reg(), be.Brigantine().createImmed(success, result->Type()), result->TypeSizeBits());
    else
      be.EmitMov(result->Reg(), be.Brigantine().createImmed(error, result->Type()), result->TypeSizeBits());
    OperandData thenImmOp = be.Brigantine().createImmed(then_imm, GetBranchInstrType());
    OperandData thenImmOp_y = be.Brigantine().createImmed(then_imm_y, GetBranchInstrType());
    Operand src = c->Reg();
    // mov_b1 $c0, then_imm; // then_imm = {1,0}
    be.EmitMov(src, thenImmOp, c->TypeSizeBits());
    src_y = c_y->Reg();
    // mov_b1 $c1, then_imm_y; // then_imm_y = {1,0}
    be.EmitMov(src_y, thenImmOp_y, c->TypeSizeBits());
    // cbr $c0, @success;
    be.EmitCbr(src, s_then, width);
    // cbr $c1, @success;
    be.EmitCbr(src_y, s_else, width);
    // br @end_if;
    be.EmitBr(s_endif);
    // @error:
    be.Brigantine().addLabel(s_error);
    // mov_b32 $s0, 2;
    be.EmitMov(result->Reg(), be.Brigantine().createImmed(error, result->Type()), result->TypeSizeBits());
    // br @end_if;
    be.EmitBr(s_endif);
    // @success:
    be.Brigantine().addLabel(s_success);
    // mov_b32 $s0, 1;
    be.EmitMov(result->Reg(), be.Brigantine().createImmed(success, result->Type()), result->TypeSizeBits());
    // @endif:
    be.Brigantine().addLabel(s_endif);
    return result;
  }
};
*/

class SbrBasicTest : public ConditionTestBase {
public:
  SbrBasicTest(Location location, Grid geometry, Condition cond)
    : ConditionTestBase(location, geometry, cond)
  {
  }

  void Name(std::ostream& out) const {
    out << "sbr/switch/" << CodeLocationString() << "_" << cond;
  }

protected:
  Value ExpectedResult(uint64_t i) const {
    return Value(MV_UINT32, cond->ExpectedSwitchPath(i));
  }
  
  TypedReg Result() {
    unsigned branchCount = cond->SwitchBranchCount();
    BrigType type = ResultType();
    TypedReg result = be.AddTReg(type);
    be.EmitMov(result, be.Immed(type, branchCount + 1));
    cond->EmitSwitchStart();
    be.EmitMov(result, be.Immed(type, branchCount + 2));
    for (unsigned i = 0; i < branchCount; ++i) {
      cond->EmitSwitchBranchStart(i);
      be.EmitMov(result, be.Immed(type, i + 1));
    }
    cond->EmitSwitchEnd();
    return result;
  }
};

void BranchTests::Iterate(TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  std::string base = "branch";
  Arena* ap = cc->Ap();
  TestForEach<BrBasicTest>(ap, it, base, CodeLocations());
  TestForEach<CbrBasicTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().BinaryConditions());
  TestForEach<CbrNestedTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().BinaryConditions(), cc->ControlFlow().BinaryConditions());
  TestForEach<CbrIfThenElseTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().BinaryConditions());
  TestForEach<CbrIfThenElseNestedInThenTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().BinaryConditions(), cc->ControlFlow().BinaryConditions());
  TestForEach<CbrIfThenElseNestedInElseTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().BinaryConditions(), cc->ControlFlow().BinaryConditions());
  //TestForEach<CbrSandTest>(it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().BinaryConditions(), Bools::All(), Bools::All());
  //TestForEach<CbrSorTest>(it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().BinaryConditions(), Bools::All(), Bools::All());
  
  TestForEach<SbrBasicTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().SwitchConditions());
}

}
