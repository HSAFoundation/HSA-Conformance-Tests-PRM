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

#include "HexlTestGen.hpp"
#include "Utils.hpp"
#include "HSAILTestGenManager.h"
#include "Scenario.hpp"
#include "Emitter.hpp"
#include "BrigEmitter.hpp"
#include "HSAILTestGenBrigContext.h"
#include "HSAILTestGenDataProvider.h"

#define TESTGEN_PRECISION_HACK
    /// \todo 1ULP for all f16 ops
    /// \todo Default (very rough) for all native floating ops
    /// \todo Default (very rough) for fma_f64 and mad_f64 ops

using namespace TESTGEN;
using namespace HSAIL_ASM;
using namespace hexl::emitter;

namespace hexl {


namespace TestGen {

const std::string TestGenConfig::ID = "TestGenConfig";

class HexlTestGenManager : public TestGenManager {
private:
  std::string path;
  std::string prefix;
  std::string fullpath;
  unsigned opcode;
  TestSpecIterator& it;
  unsigned index;
  std::unique_ptr<TestEmitter> te;
  Module module;
  Dispatch dispatch;

public:
  HexlTestGenManager(const std::string& path_, const std::string& prefix_, unsigned opcode_, TestSpecIterator& it_)
    : TestGenManager("LUA", true, false, true, true),
      path(path_), prefix(prefix_), opcode(opcode_), it(it_), index(0)
  {
    fullpath = path;
    if (!fullpath.empty()) { fullpath += "/"; }
    fullpath += prefix;
  }
  
  bool isOpcodeEnabled(unsigned opcode) { return this->opcode == opcode; }

  bool startTest(Inst inst) { return true; }

  std::string getTestName()
  {
    return prefix;
  }

  void testComplete(TestDesc& testDesc)
  {
    it(fullpath, CreateTestSpec(testDesc));
  }

private:
  string getSrcArrayName(unsigned idx, string prefix = "")
  { return prefix + "src" + index2str(idx); }

#ifdef TESTGEN_PRECISION_HACK
  bool isNativeFloatingOp(Inst inst)
  {
      switch(inst.opcode())
      {
      case BRIG_OPCODE_NCOS: case BRIG_OPCODE_NEXP2: case BRIG_OPCODE_NFMA: case BRIG_OPCODE_NLOG2:
      case BRIG_OPCODE_NRCP: case BRIG_OPCODE_NRSQRT: case BRIG_OPCODE_NSIN: case BRIG_OPCODE_NSQRT:
          return true;
      default:
        return false;
      }
  }

  bool isFmaOrMadF64(Inst inst)
  {
      switch(inst.opcode()) {
      case BRIG_OPCODE_FMA: case BRIG_OPCODE_MAD:
          switch(getType(inst)) {
          case BRIG_TYPE_F64: case BRIG_TYPE_F64X2: return true;
          default: break;
          }
      default:
        return false;
      }
  }
#endif

  TestSpec* CreateTestSpec(TestDesc& testDesc)
  {
    BrigCodeOffset32_t ioffset = testDesc.getInst().brigOffset();
    // TODO: update inst because CreateBrigFromContainer can modify the container invalidatng sections.
    testDesc.setInst(Inst(testDesc.getContainer(), ioffset));
    TestGroupArray* testGroup = testDesc.getData();
    TestDataMap* map = testDesc.getMap();
    te.reset(new TestEmitter());
    unsigned id = 0;
    module = te->NewModule("sample");

    Grid geometry = new(te->Ap()) GridGeometry(
      1, 
      testGroup->getGroupsNum(), 1, 1, 
      (std::min)(testGroup->getGroupsNum(), (unsigned) 64), 1, 1);
    dispatch = te->NewDispatch("dispatch", "executable", "", geometry);
    for (unsigned i = map->getFirstSrcArgIdx(); i <= map->getLastSrcArgIdx(); ++i) {
      defSrcArray(testGroup, id, i);
      id++;
    }
    if (map->getDstArgsNum() == 1) {
      defResultArray(testGroup, id, "dst", true
#ifdef TESTGEN_PRECISION_HACK
        , isNativeFloatingOp(testDesc.getInst()) || isFmaOrMadF64(testDesc.getInst())
#endif
        );
      id++;
    }
    if (map->getMemArgsNum() == 1) {
      defResultArray(testGroup, id, "mem", false
#ifdef TESTGEN_PRECISION_HACK
        , false
#endif
        );
      id++;
    }

    dispatch->ScenarioInit();
    te->TestScenario()->Commands()->ProgramCreate();
    module->ScenarioProgram();
    te->TestScenario()->Commands()->ProgramFinalize();
    te->TestScenario()->Commands()->ExecutableCreate(dispatch->ExecutableId());
    te->TestScenario()->Commands()->ExecutableLoadCode();
    te->TestScenario()->Commands()->ExecutableFreeze();
    module->SetupDispatch("dispatch");
    dispatch->SetupDispatch("dispatch");
    dispatch->ScenarioDispatch();
    dispatch->ScenarioValidation();
    dispatch->ScenarioEnd();

    std::ostringstream ss;
    ss << dumpInst(testDesc.getInst()) << "_" << std::setw(5) << std::setfill('0') << index++;
    std::string testName = ss.str();

    Context* initialContext = te->ReleaseContext();
    // FIXME: workaroud for double deletion of brig container
    BrigContainer* copy = new BrigContainer(testDesc.getContainer()->getBrigModule());
    initialContext->Move("sample.brig", copy);
    initialContext->Move("scenario", te->TestScenario()->ReleaseScenario());
    Test* test = new ScenarioTest(testName, initialContext);
    return new TestHolder(test);
  }

  void defSrcArray(TestGroupArray* testGroup, unsigned id, unsigned operandIdx) {
    TestData& data = testGroup->getData(0);
    BrigType type = (BrigType) data.src[operandIdx].getValType();
    unsigned vecSize = data.src[operandIdx].getDim();
    Buffer buffer = dispatch->NewBuffer(getSrcArrayName(operandIdx), HOST_INPUT_BUFFER, BufferValueType(type), BufferArraySize(type, vecSize * testGroup->getFlatSize()));
    for (unsigned flatIdx = 0; flatIdx < testGroup->getFlatSize(); ++flatIdx) {
      TestData& data = testGroup->getData(flatIdx);
      for (unsigned k = 0; k < vecSize; ++k) {
        Val val = data.src[operandIdx][k];
        Val2Value(buffer, val);
      }
    }
  }

  void defResultArray(TestGroupArray* testGroup, unsigned id, std::string name, bool isDst
#ifdef TESTGEN_PRECISION_HACK
    , bool compareWithDefaultPrecision
#endif
  ) {
    TestData& data = testGroup->getData(0);
    unsigned vecSize = isDst? data.dst.getDim() : data.mem.getDim();
    BrigType type = (BrigType) (isDst? data.dst.getValType() : data.mem.getValType());
    Buffer buffer = dispatch->NewBuffer(name, HOST_RESULT_BUFFER, BufferValueType(type), BufferArraySize(type, vecSize * testGroup->getFlatSize()));
#ifdef TESTGEN_PRECISION_HACK
    if (buffer->VType() == MV_FLOAT16 || buffer->VType() == MV_PLAIN_FLOAT16) { buffer->SetComparisonMethod("ulps=1"); }
    if (compareWithDefaultPrecision) { buffer->SetComparisonMethod("legacy_default"); }
#endif
    for (unsigned flatIdx = 0; flatIdx < testGroup->getFlatSize(); ++flatIdx) {
      TestData& data = testGroup->getData(flatIdx);
      for (unsigned k = 0; k < vecSize; ++k) {
        Val val = isDst? data.dst[k] : data.mem[k];
        Val2Value(buffer, val);
      }
    }
  }

  ValueType BufferValueType(BrigType type) {
    return Brig2ValueType(type);
  }

  unsigned BufferArraySize(BrigType type, unsigned size) {
    return size * Brig2ValueCount(type);
  }

  void Val2Value(Buffer buffer, Val val) {
    unsigned type = val.getType();
    switch (type) {
    case BRIG_TYPE_B1: buffer->AddData(Value(MV_UINT32, val.b1())); return;
    case BRIG_TYPE_B8: buffer->AddData(Value(MV_UINT32, val.b8())); return;
    case BRIG_TYPE_U8: buffer->AddData(Value(MV_UINT32, val.u8())); return;
    case BRIG_TYPE_S8: buffer->AddData(Value(MV_INT32, (int32_t)val.s8())); return;
    case BRIG_TYPE_B16: buffer->AddData(Value(MV_UINT32, val.b16())); return;
    case BRIG_TYPE_U16: buffer->AddData(Value(MV_UINT32, val.u16())); return;
    case BRIG_TYPE_S16: buffer->AddData(Value(MV_INT32, (int32_t)val.s16())); return;
    case BRIG_TYPE_B32: buffer->AddData(Value(MV_UINT32, val.b32())); return;
    case BRIG_TYPE_U32: buffer->AddData(Value(MV_UINT32, val.u32())); return;
    case BRIG_TYPE_S32: buffer->AddData(Value(MV_INT32, val.s32())); return;
    case BRIG_TYPE_B64: buffer->AddData(Value(MV_UINT64, val.b64())); return;
    case BRIG_TYPE_U64: buffer->AddData(Value(MV_UINT64, val.u64())); return;
    case BRIG_TYPE_S64: buffer->AddData(Value(MV_INT64, val.s64())); return;
    case BRIG_TYPE_F16: 
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
      buffer->AddData(Value(MV_PLAIN_FLOAT16, val.getAsB16())); return;
#else
      buffer->AddData(Value(MV_FLOAT16, val.getAsB16())); return;
#endif
    case BRIG_TYPE_F64: buffer->AddData(Value(val.f64())); return;
    case BRIG_TYPE_F32: buffer->AddData(Value(val.f32())); return;
    case BRIG_TYPE_B128: {
      buffer->AddData(Value(MV_UINT64, U64(val.b128().get<uint64_t>(0))));
      buffer->AddData(Value(MV_UINT64, U64(val.b128().get<uint64_t>(1))));
      return;
    }
    case BRIG_TYPE_F16X2:
    case BRIG_TYPE_F16X4:
    case BRIG_TYPE_F16X8:
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    {
      unsigned dim = getPackedTypeDim(val.getType());
      for (unsigned i = 0; i < dim; ++i) {
        buffer->AddData(Value(MV_FLOAT16, val.getPackedElement(i).getAsB16()));
      }
      return;
    }
#endif
    case BRIG_TYPE_F32X2:
    case BRIG_TYPE_F32X4:
    case BRIG_TYPE_F64X2: {
      unsigned dim = getPackedTypeDim(val.getType());
      for (unsigned i = 0; i < dim; ++i) Val2Value(buffer, val.getPackedElement(i));
      return;
    }
    default: {
      assert(isIntPackedType(type));
      unsigned size = getBrigTypeNumBits(type);
      switch (size) {
      case 32: buffer->AddData(Value(MV_UINT32, val.getAsB32())); return;
      case 64: buffer->AddData(Value(MV_UINT64, val.getAsB64())); return;
      case 128: {
        buffer->AddData(Value(MV_UINT64, U64(val.getAsB64(0))));
        buffer->AddData(Value(MV_UINT64, U64(val.getAsB64(1))));
        return;
      }
      default:
        std::cout << val.getType() << std::endl; assert(!"Unsupported type in Val2Value"); 
        return;
      }
    }
    }
  }

  hexl::ValueType Brig2ValueType(BrigType type)
  {
    switch (type) {
    case BRIG_TYPE_B1: return MV_UINT32;
    case BRIG_TYPE_F16:
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
      return MV_PLAIN_FLOAT16;
#else
      return MV_FLOAT16;
#endif
    case BRIG_TYPE_F32: return MV_FLOAT;
    case BRIG_TYPE_F64: return MV_DOUBLE;
    case BRIG_TYPE_F16X2:
    case BRIG_TYPE_F16X4:
    case BRIG_TYPE_F16X8: return MV_FLOAT16;
    case BRIG_TYPE_F32X2:
    case BRIG_TYPE_F32X4: return MV_FLOAT;
    case BRIG_TYPE_F64X2: return MV_DOUBLE;
    default: {
      assert(!isFloatType(type));
      unsigned size = getBrigTypeNumBits(type);
      bool isSigned = isSignedType(type);
      switch (size) {
      case 8: return isSigned ? MV_INT32 : MV_UINT32;
      case 16: return isSigned ? MV_INT32 : MV_UINT32;
      case 32: return isSigned ? MV_INT32 : MV_UINT32;
      case 64: return isSigned ? MV_INT64 : MV_UINT64;
      case 128: return MV_UINT64;
      default: assert(!"Unsupported size in Brig2ValueType"); return MV_LAST;
      }
      }
    }
  }

  unsigned Brig2ValueCount(BrigType type)
  {
    switch (type) {
    case BRIG_TYPE_F16X2: return 2;
    case BRIG_TYPE_F16X4: return 4;
    case BRIG_TYPE_F16X8: return 8;
    case BRIG_TYPE_F32X2: return 2;
    case BRIG_TYPE_F32X4: return 4;
    case BRIG_TYPE_F64X2: return 2;
    default: 
      return (getBrigTypeNumBits(type) == 128)? 2 : 1;
    }
  }

}; // class HexlTestGenManager

void TestGenTestSet::Iterate(TestSpecIterator& it)
{
  TestDataProvider::init(true, true, 0, 64, 0, true, context->Opts()->IsSet("XtestFtzF16"));
  TestGenConfig* testGenConfig = context->Get<TestGenConfig>(TestGenConfig::ID);
  assert(testGenConfig);
  BrigSettings::init(testGenConfig->Model(), testGenConfig->Profile(), true, false, false, context->IsDumpEnabled("hsail"));
  TESTGEN::TestGen::init(true);
  PropDesc::init(testGenConfig->Model(), testGenConfig->Profile());
  HexlTestGenManager m(path, prefix, opcode, it);
  m.generate();
  TESTGEN::TestGen::clean();
  PropDesc::clean();
  TestDataProvider::clean();
}

TestSet* TestGenTestSet::Filter(TestNameFilter* filter)
{
  std::string rest;
  if (filter->NamePattern().empty()) { return this; }
  std::string fullpath = path + "/" + prefix;
  size_t minlen = (std::min)(filter->NamePattern().length(), fullpath.length());
  if (minlen > 0 && fullpath.compare(0, minlen, filter->NamePattern().substr(0, minlen)) != 0) { return new EmptyTestSet(); }
  return new FilteredTestSet(this, filter);
}

} // namespace TestGen

} // namespace hexl
