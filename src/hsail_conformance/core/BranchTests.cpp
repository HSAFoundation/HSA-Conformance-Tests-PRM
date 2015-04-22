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
  : Test(location, geometry) { }

  void Name(std::ostream& out) const { out << "br/basic/" << CodeLocationString(); }

protected:
  static const uint64_t success = 1; // expected value
  static const uint64_t error = 2; // error code

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const { return Value(MV_UINT32, success); }

  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    // mov_b32 $s0, 1;
    be.EmitMov(result, success);
    // br @then;
    be.EmitBr("@then");
    // mov_b32 $s0, 2;
    be.EmitMov(result, error);
    // @then:
    be.EmitLabel("@then");
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
    out << "cbr/basic/" << CodeLocationString() << "/" << cond;
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
    be.EmitMov(result, 2);
    cond->EmitIfThenStart();
    be.EmitMov(result, 1);
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
    be.EmitMov(result, 3);
    cond->EmitIfThenElseStart();
    be.EmitMov(result, 1);
    cond->EmitIfThenElseOtherwise();
    be.EmitMov(result, 2);
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
    be.EmitMov(result, 3);
    cond->EmitIfThenStart();
    be.EmitMov(result, 2);
    cond2->EmitIfThenStart();
    be.EmitMov(result, 1);
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
    be.EmitMov(result, 4);
    cond->EmitIfThenElseStart();

      cond2->EmitIfThenElseStart();
      be.EmitMov(result, 1);
      cond2->EmitIfThenElseOtherwise();
      be.EmitMov(result, 2);
      cond2->EmitIfThenElseEnd();

    cond->EmitIfThenElseOtherwise();
    be.EmitMov(result, 3);
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
    be.EmitMov(result, 4);
    cond->EmitIfThenElseStart();
    be.EmitMov(result, 1);
    cond->EmitIfThenElseOtherwise();

      cond2->EmitIfThenElseStart();
      be.EmitMov(result, 2);
      cond2->EmitIfThenElseOtherwise();
      be.EmitMov(result, 3);
      cond2->EmitIfThenElseEnd();

    cond->EmitIfThenElseEnd();
    return result;
  }
};

class CbrIfThenElseNestedTest : public CbrNestedTest {
protected:
  Condition cond3;

public:
  CbrIfThenElseNestedTest(Location location, Grid geometry, Condition cond_, Condition cond2_, Condition cond3_)
    : CbrNestedTest(location, geometry, cond_, cond2_),
    cond3(cond3_)
  {
    specList.Add(cond3);
  }

  void Name(std::ostream& out) const {
    out << "cbr/ifthenelse/nested/inboth/" << CodeLocationString() << "/" << cond << "_" << cond2 << "_" << cond3;
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
      if (cond3->ExpectThenPath(i)) {
        eresult = 3;
      } else {
        eresult = 4;
      }
    }
    return Value(MV_UINT32, eresult);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, 5);
    cond->EmitIfThenElseStart();

      cond2->EmitIfThenElseStart();
      be.EmitMov(result, 1);
      cond2->EmitIfThenElseOtherwise();
      be.EmitMov(result, 2);
      cond2->EmitIfThenElseEnd();

    cond->EmitIfThenElseOtherwise();

      cond3->EmitIfThenElseStart();
      be.EmitMov(result, 3);
      cond3->EmitIfThenElseOtherwise();
      be.EmitMov(result, 4);
      cond3->EmitIfThenElseEnd();

    cond->EmitIfThenElseEnd();
    return result;
  }
};

class CbrSandTest : public ConditionTestBase {
protected:
  Condition cond2;

public:
  CbrSandTest(Location location, Grid geometry, Condition cond_, Condition cond2_)
    : ConditionTestBase(location, geometry, cond_),
    cond2(cond2_)
  {
    specList.Add(cond2);
  }

  void Name(std::ostream& out) const {
    out << "cbr/sand/" << CodeLocationString() << "/" << cond << "_" << cond2;
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
    be.EmitMov(result, 3);
    cond->EmitIfThenStart();
    be.EmitMov(result, 2);
    cond2->EmitIfThenStartSand(cond);
    be.EmitMov(result, 1);
    cond->EmitIfThenEnd();
    return result;
  }
};

class CbrSorTest : public ConditionTestBase {
protected:
  Condition cond2;

public:
  CbrSorTest(Location location, Grid geometry, Condition cond_, Condition cond2_)
    : ConditionTestBase(location, geometry, cond_),
    cond2(cond2_)
  {
    specList.Add(cond2);
  }

  void Name(std::ostream& out) const {
    out << "cbr/sor/" << CodeLocationString() << "/" << cond << "_" << cond2;
  }

protected:
  Value ExpectedResult(uint64_t i) const {
    uint32_t eresult;
    if (cond->ExpectThenPath(i) || cond2->ExpectThenPath(i)) {
        eresult = 1;
    } else {
        eresult = 2;
    }
    return Value(MV_UINT32, eresult);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, 3);
    cond->EmitIfThenStartSor();
    be.EmitMov(result, 2);
    cond2->EmitIfThenStartSor(cond);
    be.EmitMov(result, 1);
    cond2->EmitIfThenEnd();
    return result;
  }
};

class SbrBasicTest : public ConditionTestBase {
public:
  SbrBasicTest(Location location, Grid geometry, Condition cond)
    : ConditionTestBase(location, geometry, cond)
  {
  }

  void Name(std::ostream& out) const {
    out << "sbr/switch/" << CodeLocationString() << "/" << cond;
  }

protected:
  Value ExpectedResult(uint64_t i) const {
    return Value(MV_UINT32, cond->ExpectedSwitchPath(i));
  }
  
  TypedReg Result() {
    unsigned branchCount = cond->SwitchBranchCount();
    BrigType type = ResultType();
    TypedReg result = be.AddTReg(type);
    be.EmitMov(result, branchCount + 1);
    cond->EmitSwitchStart();
    be.EmitMov(result, branchCount + 2);
    for (unsigned i = 0; i < branchCount; ++i) {
      cond->EmitSwitchBranchStart(i);
      be.EmitMov(result, i + 1);
    }
    cond->EmitSwitchEnd();
    return result;
  }
};

class SbrNestedTest : public ConditionTestBase {
protected:
  Condition cond2;

public:
  SbrNestedTest(Location location, Grid geometry, Condition cond, Condition cond2_)
    : ConditionTestBase(location, geometry, cond),
      cond2(cond2_)
  {
    specList.Add(cond2);
  }

  void Name(std::ostream& out) const {
    out << "sbr/nested/" << CodeLocationString() << "/" << cond << "_" << cond2;
  }

protected:
  Value ExpectedResult(uint64_t i) const {
    return Value(MV_UINT32, cond->ExpectedSwitchPath(i));
  }

  bool IsValid() const {
    if (cond->Input() != cond2->Input())
      return false;
    if (cond->Width() != cond2->Width())
      return false;
    if (cond->Type() != cond2->Type())
      return false;
    if (cond->IType() != cond2->IType())
      return false;
    return true;
  }

  TypedReg Result() {
    unsigned branchCount = cond->SwitchBranchCount();
    unsigned branchCount2 = cond2->SwitchBranchCount();
    BrigType type = ResultType();
    TypedReg result = be.AddTReg(type);
    be.EmitMov(result, branchCount + 1);
    cond->EmitSwitchStart();
    be.EmitMov(result, branchCount + 2);
    for (unsigned i = 0; i < branchCount; ++i) {
      cond->EmitSwitchBranchStart(i);
      be.EmitMov(result, i + 1);
      if (COND_WAVESIZE == cond->Input() && i != branchCount-1) continue;
      // nested sbr
      cond2->EmitSwitchStart();
      be.EmitMov(result, branchCount + 2);
      for (unsigned j = 0; j < branchCount2; ++j) {
        cond2->EmitSwitchBranchStart(j);
        be.EmitMov(result, j + 1);
      }
      cond2->EmitSwitchEnd();
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
  TestForEach<CbrNestedTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().NestedConditions(), cc->ControlFlow().NestedConditions());
  TestForEach<CbrIfThenElseTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().BinaryConditions());
  TestForEach<CbrIfThenElseNestedInThenTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().NestedConditions(), cc->ControlFlow().NestedConditions());
  TestForEach<CbrIfThenElseNestedInElseTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().NestedConditions(), cc->ControlFlow().NestedConditions());
  TestForEach<CbrIfThenElseNestedTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().NestedConditions(), cc->ControlFlow().NestedConditions(), cc->ControlFlow().NestedConditions());
  TestForEach<CbrSandTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().NestedConditions(), cc->ControlFlow().NestedConditions());
  TestForEach<CbrSorTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().NestedConditions(), cc->ControlFlow().NestedConditions());

  TestForEach<SbrBasicTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().SwitchConditions());
  TestForEach<SbrNestedTest>(ap, it, base, CodeLocations(), cc->Grids().SeveralWavesSet(), cc->ControlFlow().NestedSwitchConditions(), cc->ControlFlow().NestedSwitchConditions());
}

}
