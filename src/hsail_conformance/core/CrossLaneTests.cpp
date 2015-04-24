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

#include "CrossLaneTests.hpp"
#include "HCTests.hpp"

using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

  class CrossLaneTestBase : public Test {
  public:
    CrossLaneTestBase(Location codeLocation, Grid geometry)
      : Test(codeLocation, geometry) { }

    void Name(std::ostream& out) const {
      out << OperationName() << "/" << TestcaseName() << "/" << CodeLocationString() << "_" << geometry
          << "_"; NameParams(out);
    }

    virtual const char *OperationName() const = 0;
    virtual const char *TestcaseName() const = 0;
    virtual void NameParams(std::ostream& out) const = 0;
  };

  class ActiveLaneCountTest : public CrossLaneTestBase {
  protected:
    Condition src;

  public:
    ActiveLaneCountTest(Location codeLocation, Grid geometry, Condition src_)
      : CrossLaneTestBase(codeLocation, geometry), src(src_)
    {
      specList.Add(src);
    }

    BrigType ResultType() const { return BRIG_TYPE_U32; }

    const char *OperationName() const { return "activelanecount"; }

    void NameParams(std::ostream& out) const { out << src; }

    void EmitInit(TypedReg dest) const {
      te->Brig()->EmitMov(dest, UINT32_MAX);
    }

    Value InitValue() const {
      return Value(MV_UINT32, UINT32_MAX);
    }
  };

  class ActiveLaneCountNoDivergence : public ActiveLaneCountTest {
  public:
    ActiveLaneCountNoDivergence(Location codeLocation, Grid geometry, Condition src)
      : ActiveLaneCountTest(codeLocation, geometry, src) { }

    const char *TestcaseName() const { return "nodivergence"; }

    void ExpectedResults(Values* results) const {
      std::vector<uint32_t> counts(geometry->MaxWaveIndex(cc->Wavesize()), 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        if (src->IsTrueFor(*p)) {
          counts[geometry->WaveIndex(*p, cc->Wavesize())]++;
        }
      }
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        results->push_back(Value(MV_UINT32, counts[geometry->WaveIndex(*p, cc->Wavesize())]));
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_U32);
      te->Brig()->EmitActiveLaneCount(result, src->CondOperand());
      return result;
    }
  };

  class ActiveLaneCountIfThen : public ActiveLaneCountTest {
  private:
    Condition cond;

  public:
    ActiveLaneCountIfThen(Location codeLocation, Grid geometry, Condition src, Condition cond_)
      : ActiveLaneCountTest(codeLocation, geometry, src), cond(cond_) { specList.Add(cond); }

    const char *TestcaseName() const { return "ifthen"; }

    void NameParams(std::ostream& out) const { out << src << "_" << cond; }

    void ExpectedResults(Values* results) const {
      std::vector<uint32_t> counts(geometry->MaxWaveIndex(cc->Wavesize()), 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        if (src->IsTrueFor(*p)) {
          if (cond->ExpectThenPath(*p)) {
            counts[geometry->WaveIndex(*p, cc->Wavesize())]++;
          }
        }
      }

      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        results->push_back(
          cond->ExpectThenPath(*p) ?
            Value(MV_UINT32, counts[geometry->WaveIndex(*p, cc->Wavesize())]) :
            InitValue());
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_U32);
      EmitInit(result);
      cond->EmitIfThenStart();
      te->Brig()->EmitActiveLaneCount(result, src->CondOperand());
      cond->EmitIfThenEnd();
      return result;
    }
  };

  class ActiveLaneCountIfThenElse : public ActiveLaneCountTest {
  private:
    Condition cond;

  public:
    ActiveLaneCountIfThenElse(Location codeLocation, Grid geometry, Condition src, Condition cond_)
      : ActiveLaneCountTest(codeLocation, geometry, src), cond(cond_) { specList.Add(cond); }

    const char *TestcaseName() const { return "ifthenelse"; }

    void NameParams(std::ostream& out) const { out << src << "_" << cond; }

    void ExpectedResults(Values* results) const {
      std::vector<uint32_t> countsThen(geometry->MaxWaveIndex(cc->Wavesize()), 0);
      std::vector<uint32_t> countsElse(geometry->MaxWaveIndex(cc->Wavesize()), 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        if (src->IsTrueFor(*p)) {
          if (cond->ExpectThenPath(*p)) {
            countsThen[geometry->WaveIndex(*p, cc->Wavesize())]++;
          } else {
            countsElse[geometry->WaveIndex(*p, cc->Wavesize())]++;
          }
        }
      }

      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        results->push_back(
          cond->ExpectThenPath(*p) ?
            Value(MV_UINT32, countsThen[geometry->WaveIndex(*p, cc->Wavesize())]) :
            Value(MV_UINT32, countsElse[geometry->WaveIndex(*p, cc->Wavesize())]));
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_U32);
      cond->EmitIfThenElseStart();
      te->Brig()->EmitActiveLaneCount(result, src->CondOperand());
      cond->EmitIfThenElseOtherwise();
      te->Brig()->EmitActiveLaneCount(result, src->CondOperand());
      cond->EmitIfThenElseEnd();
      return result;
    }
  };

  class ActiveLaneIdTest : public CrossLaneTestBase {
  public:
    ActiveLaneIdTest(Location codeLocation, Grid geometry)
      : CrossLaneTestBase(codeLocation, geometry) { }

    BrigType ResultType() const { return BRIG_TYPE_U32; }

    const char *OperationName() const { return "activelaneid"; }

    void NameParams(std::ostream& out) const { }

    void EmitInit(TypedReg dest) const {
      te->Brig()->EmitMov(dest, UINT32_MAX);
    }

    Value InitValue() const {
      return Value(MV_UINT32, UINT32_MAX);
    }
  };

  class ActiveLaneIdNoDivergence : public ActiveLaneIdTest {
  public:
    ActiveLaneIdNoDivergence(Location codeLocation, Grid geometry)
      : ActiveLaneIdTest(codeLocation, geometry) { }

    const char *TestcaseName() const { return "nodivergence"; }

    void ExpectedResults(Values* results) const {
      std::vector<uint32_t> id(geometry->MaxWaveIndex(cc->Wavesize()), 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        results->push_back(Value(MV_UINT32, id[geometry->WaveIndex(*p, cc->Wavesize())]++));
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_U32);
      te->Brig()->EmitActiveLaneId(result);
      return result;
    }
  };

  class ActiveLaneIdIfThen : public ActiveLaneIdTest {
  private:
    Condition cond;

  public:
    ActiveLaneIdIfThen(Location codeLocation, Grid geometry, Condition cond_)
      : ActiveLaneIdTest(codeLocation, geometry), cond(cond_) { specList.Add(cond); }

    const char *TestcaseName() const { return "ifthen"; }

    void NameParams(std::ostream& out) const { out << cond; }

    void ExpectedResults(Values* results) const {
      std::vector<uint32_t> id(geometry->MaxWaveIndex(cc->Wavesize()), 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        if (cond->ExpectThenPath(*p)) {
          results->push_back(Value(MV_UINT32, id[geometry->WaveIndex(*p, cc->Wavesize())]++));
        } else {
          results->push_back(InitValue());
        }
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_U32);
      EmitInit(result);
      cond->EmitIfThenStart();
      te->Brig()->EmitActiveLaneId(result);
      cond->EmitIfThenEnd();
      return result;
    }
  };

  class ActiveLaneIdIfThenElse : public ActiveLaneIdTest {
  private:
    Condition cond;

  public:
    ActiveLaneIdIfThenElse(Location codeLocation, Grid geometry, Condition cond_)
      : ActiveLaneIdTest(codeLocation, geometry), cond(cond_) { specList.Add(cond); }

    const char *TestcaseName() const { return "ifthenelse"; }

    void NameParams(std::ostream& out) const { out << cond; }

    void ExpectedResults(Values* results) const {
      std::vector<uint32_t> idThen(geometry->MaxWaveIndex(cc->Wavesize()), 0);
      std::vector<uint32_t> idElse(geometry->MaxWaveIndex(cc->Wavesize()), 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        if (cond->ExpectThenPath(*p)) {
          results->push_back(Value(MV_UINT32, idThen[geometry->WaveIndex(*p, cc->Wavesize())]++));
        } else {
          results->push_back(Value(MV_UINT32, idElse[geometry->WaveIndex(*p, cc->Wavesize())]++));
        }
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_U32);
      cond->EmitIfThenElseStart();
      te->Brig()->EmitActiveLaneId(result);
      cond->EmitIfThenElseOtherwise();
      te->Brig()->EmitActiveLaneId(result);
      cond->EmitIfThenElseEnd();
      return result;
    }
  };

  class ActiveLaneMaskTest : public CrossLaneTestBase {
  protected:
    Condition src;

    static const uint32_t initMask = 0xFEEDBEEF;

  public:
    ActiveLaneMaskTest(Location codeLocation, Grid geometry, Condition src_)
      : CrossLaneTestBase(codeLocation, geometry), src(src_) { specList.Add(src); }

    BrigType ResultType() const { return BRIG_TYPE_B64; }

    uint64_t ResultDim() const { return 4; }

    const char *OperationName() const { return "activelanemask"; }

    void NameParams(std::ostream& out) const { out << src; }

    void EmitInit(TypedReg dest) const {
      te->Brig()->EmitMov(dest, initMask);
    }

    Value InitValue() const {
      return Value(MV_UINT64, initMask);
    }
  };

  class ActiveLaneMaskNoDivergence : public ActiveLaneMaskTest {
  public:
    ActiveLaneMaskNoDivergence(Location codeLocation, Grid geometry, Condition src)
      : ActiveLaneMaskTest(codeLocation, geometry, src) { }

    const char *TestcaseName() const { return "nodivergence"; }

    void ExpectedResults(Values* results) const {
      std::vector<uint64_t> mask(geometry->MaxWaveIndex(cc->Wavesize()) * 4, 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        if (src->IsTrueFor(*p)) {
          uint32_t lid = geometry->LaneId(*p, cc->Wavesize());
          mask[geometry->WaveIndex(*p, cc->Wavesize()) * 4 + lid / 64] |= ((uint64_t) 1) << (lid % 64);
        }
      }
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        for (unsigned i = 0; i < 4; ++i) {
          results->push_back(Value(MV_UINT64, mask[geometry->WaveIndex(*p, cc->Wavesize()) * 4 + i]));
        }
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_B64, 4);
      te->Brig()->EmitActiveLaneMask(result, src->CondOperand());
      return result;
    }
  };

  class ActiveLaneMaskIfThen : public ActiveLaneMaskTest {
  private:
    Condition cond;

  public:
    ActiveLaneMaskIfThen(Location codeLocation, Grid geometry, Condition src, Condition cond_)
      : ActiveLaneMaskTest(codeLocation, geometry, src), cond(cond_) { specList.Add(cond); }

    const char *TestcaseName() const { return "ifthen"; }

    void NameParams(std::ostream& out) const { out << src << "_" << cond; }

    void ExpectedResults(Values* results) const {
      std::vector<uint64_t> mask(geometry->MaxWaveIndex(cc->Wavesize()) * 4, 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        if (src->IsTrueFor(*p)) {
          uint32_t lid = geometry->LaneId(*p, cc->Wavesize());
          if (cond->ExpectThenPath(*p)) {
            mask[geometry->WaveIndex(*p, cc->Wavesize()) * 4 + lid / 64] |= ((uint64_t) 1) << (lid % 64);
          }
        }
      }
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        for (unsigned i = 0; i < 4; ++i) {
          if (cond->ExpectThenPath(*p)) {
            results->push_back(Value(MV_UINT64, mask[geometry->WaveIndex(*p, cc->Wavesize()) * 4 + i]));
          } else {
            results->push_back(InitValue());
          }
        }
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_B64, 4);
      EmitInit(result);
      cond->EmitIfThenStart();
      te->Brig()->EmitActiveLaneMask(result, src->CondOperand());
      cond->EmitIfThenEnd();
      return result;
    }
  };

  class ActiveLaneMaskIfThenElse : public ActiveLaneMaskTest {
  private:
    Condition cond;

  public:
    ActiveLaneMaskIfThenElse(Location codeLocation, Grid geometry, Condition src, Condition cond_)
      : ActiveLaneMaskTest(codeLocation, geometry, src), cond(cond_) { specList.Add(cond); }

    const char *TestcaseName() const { return "ifthenelse"; }

    void NameParams(std::ostream& out) const { out << src << "_" << cond; }

    void ExpectedResults(Values* results) const {
      std::vector<uint64_t> maskThen(geometry->MaxWaveIndex(cc->Wavesize()) * 4, 0);
      std::vector<uint64_t> maskElse(geometry->MaxWaveIndex(cc->Wavesize()) * 4, 0);
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        if (src->IsTrueFor(*p)) {
          uint32_t lid = geometry->LaneId(*p, cc->Wavesize());
          if (cond->ExpectThenPath(*p)) {
            maskThen[geometry->WaveIndex(*p, cc->Wavesize()) * 4 + lid / 64] |= ((uint64_t) 1) << (lid % 64);
          } else {
            maskElse[geometry->WaveIndex(*p, cc->Wavesize()) * 4 + lid / 64] |= ((uint64_t) 1) << (lid % 64);
          }
        }
      }
      for (auto p = geometry->GridBegin(); p != geometry->GridEnd(); ++p) {
        for (unsigned i = 0; i < 4; ++i) {
          if (cond->ExpectThenPath(*p)) {
            results->push_back(Value(MV_UINT64, maskThen[geometry->WaveIndex(*p, cc->Wavesize()) * 4 + i]));
          } else {
            results->push_back(Value(MV_UINT64, maskElse[geometry->WaveIndex(*p, cc->Wavesize()) * 4 + i]));
          }
        }
      }
    }

    TypedReg Result() {
      TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_B64, 4);
      cond->EmitIfThenElseStart();
      te->Brig()->EmitActiveLaneMask(result, src->CondOperand());
      cond->EmitIfThenElseOtherwise();
      te->Brig()->EmitActiveLaneMask(result, src->CondOperand());
      cond->EmitIfThenElseEnd();
      return result;
    }
  };

  void CrossLaneOperationsTests::Iterate(hexl::TestSpecIterator& it)
  {
    CoreConfig* cc = CoreConfig::Get(context);
    Arena* ap = cc->Ap();
    
    TestForEach<ActiveLaneCountNoDivergence>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->ControlFlow().BinaryConditions());
    TestForEach<ActiveLaneCountIfThen>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->ControlFlow().BinaryConditions(), cc->ControlFlow().BinaryConditions());
    TestForEach<ActiveLaneCountIfThenElse>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->ControlFlow().BinaryConditions(), cc->ControlFlow().BinaryConditions());
    TestForEach<ActiveLaneIdNoDivergence>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet());
    TestForEach<ActiveLaneIdIfThen>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->ControlFlow().BinaryConditions());
    TestForEach<ActiveLaneIdIfThenElse>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->ControlFlow().BinaryConditions());
    TestForEach<ActiveLaneMaskNoDivergence>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->ControlFlow().BinaryConditions());
    TestForEach<ActiveLaneMaskIfThen>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->ControlFlow().BinaryConditions(), cc->ControlFlow().BinaryConditions());
    TestForEach<ActiveLaneMaskIfThenElse>(ap, it, "crosslane", CodeLocations(), cc->Grids().DefaultGeometrySet(), cc->ControlFlow().BinaryConditions(), cc->ControlFlow().BinaryConditions());
  }

}
