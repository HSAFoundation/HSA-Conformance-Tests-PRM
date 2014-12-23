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

#include "CrossLaneTests.hpp"
#include "HCTests.hpp"

using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

  class CrossLaneTestBase : public Test {
  protected:
    Condition cond;
    BrigOpcode opcode;

  public:
    CrossLaneTestBase(Location codeLocation, Grid geometry, Condition cond_, BrigOpcode opcode_)
      : Test(codeLocation, geometry), cond(cond_), opcode(opcode_) { }

    TypedReg AddResultReg() {
      switch (opcode) {
      case BRIG_OPCODE_ACTIVELANECOUNT: return be.AddTReg(BRIG_TYPE_U32);
      case BRIG_OPCODE_ACTIVELANEID: return be.AddTReg(BRIG_TYPE_U32);
      case BRIG_OPCODE_ACTIVELANEMASK: return be.AddTReg(BRIG_TYPE_U64, 4);
      case BRIG_OPCODE_ACTIVELANESHUFFLE: return be.AddTReg(BRIG_TYPE_U32, 4);
      default: assert(false); return 0;
      }
    }

    void EmitCrossLane(TypedReg result) {
      switch (opcode) {
      case BRIG_OPCODE_ACTIVELANECOUNT:
        be.EmitActiveLaneCount(result, be.Immed(BRIG_TYPE_B1, 1));
        break;

      case BRIG_OPCODE_ACTIVELANEID:
        be.EmitActiveLaneId(result);
        break;

      case BRIG_OPCODE_ACTIVELANEMASK:
//          be.EmitActiveLaneMask(result);
        break;

      case BRIG_OPCODE_ACTIVELANESHUFFLE:
//          be.EmitActiveLaneShuffle();
        break;
      default:
        assert(0);
      }
    }
  };


  class CrossLaneTrivialTest : public CrossLaneTestBase {
  private:
    BrigOpcode opcode;

  protected:
    Brig::BrigTypeX ResultType() const {
      switch (opcode) {
      case BRIG_OPCODE_ACTIVELANECOUNT: return BRIG_TYPE_U32;
      case BRIG_OPCODE_ACTIVELANEID: return BRIG_TYPE_U32;
      case BRIG_OPCODE_ACTIVELANEMASK: assert(false);
      case BRIG_OPCODE_ACTIVELANESHUFFLE: return BRIG_TYPE_U32;

      default:
        assert(false); return BRIG_TYPE_NONE;
      }
    }

    Value ExpectedResult(uint64_t id) const {
      uint32_t wavesize = te->CoreCfg()->Wavesize();
      Dim point = geometry->Point(id);
      uint32_t flatid = geometry->WorkitemFlatId(point);
      uint32_t currentflatid =
        geometry->WorkitemId(point, 0) +
        geometry->WorkitemId(point, 1) * geometry->CurrentWorkgroupSize(point, 0) +
        geometry->WorkitemId(point, 2) * geometry->CurrentWorkgroupSize(point, 0) * geometry->CurrentWorkgroupSize(point, 1);
      uint32_t currentwgsize = geometry->CurrentWorkgroupSize(point);
      switch (opcode) {
      case BRIG_OPCODE_ACTIVELANECOUNT:
        {
          if (flatid < (currentwgsize / wavesize) * wavesize) {
            return Value(MV_UINT32, wavesize);
          } else {
            return Value(MV_UINT32, currentwgsize - (currentwgsize / wavesize) * wavesize);
          }
        }

      case BRIG_OPCODE_ACTIVELANEID: {
        return Value(MV_UINT32, currentflatid % wavesize);
      }

      case BRIG_OPCODE_ACTIVELANEMASK:
        if (id < (currentwgsize / wavesize) * wavesize) {
          return Value(MV_UINT64, 1 << wavesize);
        } else {
          return Value(MV_UINT64, 1 << (currentwgsize - (currentwgsize / wavesize) * wavesize));
        }

      default:
        assert(false); return Value();
      }
    }


    TypedReg Result() {
      TypedReg result = AddResultReg();
      EmitCrossLane(result);
      return result;
    }

  public:
    CrossLaneTrivialTest(Location codeLocation, Grid geometry, Condition cond, BrigOpcode opcode_)
      : CrossLaneTestBase(codeLocation, geometry, cond, opcode) { }

    void Name(std::ostream& out) const {
      out << opcode2str(opcode) << "/" << CodeLocationString() << "_" << geometry;
    }
  };

  static Sequence<BrigOpcode>* CrossLaneOpcodes() {
    static BrigOpcode crossLaneOpcodes[] = {
      BRIG_OPCODE_ACTIVELANECOUNT,
      BRIG_OPCODE_ACTIVELANEID,
      /*
      BRIG_OPCODE_ACTIVELANEMASK,
      BRIG_OPCODE_ACTIVELANESHUFFLE,
      */
    };
    static ArraySequence<BrigOpcode> sequence(crossLaneOpcodes, NELEM(crossLaneOpcodes));
    return &sequence;
  }

  void CrossLaneOperationsTests::Iterate(hexl::TestSpecIterator& it)
  {
    CoreConfig* cc = CoreConfig::Get(context);
    Arena* ap = cc->Ap();
//    TestForEach<CrossLaneTrivialTest>(ap, it, "crosslane/trivial", CodeLocations(), cc->Grids().DimensionSet(), CrossLaneOpcodes());

  }

}
