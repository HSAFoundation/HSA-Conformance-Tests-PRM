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

#include "HexlTestGen.hpp"
#include "Utils.hpp"
#include "HSAILTestGenManager.h"
#include "BasicHexlTests.hpp"
#include "HSAILTestGenBrigContext.h"
#include "HSAILTestGenDataProvider.h"

using namespace Brig;
using namespace TESTGEN;
using namespace HSAIL_ASM;

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

  TestSpec* CreateTestSpec(TestDesc& testDesc)
  {
    brig_container_t brig = CreateBrigFromContainer(testDesc.getContainer());
    TestGroupArray* testGroup = testDesc.getData();
    TestDataMap* map = testDesc.getMap();
    DispatchSetup* dsetup = new DispatchSetup();
    dsetup->SetDimensions(1);
    dsetup->SetGridSize(testGroup->getGroupsNum());
    dsetup->SetWorkgroupSize(std::min(testGroup->getGroupsNum(), (unsigned) 64));
    unsigned id = 0;
    for (unsigned i = map->getFirstSrcArgIdx(); i <= map->getLastSrcArgIdx(); ++i) {
      id = defSrcArray(dsetup, testGroup, id, i);
    }
    if (map->getDstArgsNum() == 1) {
      id = defResultArray(dsetup, testGroup, id, "dst", true);
    }
    if (map->getMemArgsNum() == 1) {
      id = defResultArray(dsetup, testGroup, id, "mem", false);
    }
    std::ostringstream ss;
    ss << dumpInst(testDesc.getInst()) << "_" << std::setw(5) << std::setfill('0') << index++;
    std::string testName = ss.str();
    Test* test = new ValidateBrigContainerTest(testName, brig, dsetup);
    return new TestHolder(test);
  }

  unsigned defSrcArray(DispatchSetup* dsetup, TestGroupArray* testGroup, unsigned id, unsigned operandIdx) {
    TestData& data = testGroup->getData(0);
    unsigned vecSize = data.src[operandIdx].getDim();
    BrigTypeX type = (BrigTypeX) data.src[operandIdx].getValType();
    uint32_t sizes[3] = { BufferArraySize(type, vecSize * testGroup->getFlatSize()), 1, 1 };
    MBuffer* mb = new MBuffer(id++, getSrcArrayName(operandIdx), MEM_GLOBAL,
                              BufferValueType(type), 1, sizes);
    for (unsigned flatIdx = 0; flatIdx < testGroup->getFlatSize(); ++flatIdx) {
      TestData& data = testGroup->getData(flatIdx);
      for (unsigned k = 0; k < vecSize; ++k) {
        Val val = data.src[operandIdx][k];
        Val2Value(mb->Data(), val);
      }
    }
    dsetup->MSetup().Add(mb);
    dsetup->MSetup().Add(NewMValue(id++, std::string("Arg ") + getSrcArrayName(operandIdx),
                               MEM_KERNARG, MV_REF, R(mb->Id())));
    return id;
  }

  unsigned defResultArray(DispatchSetup* dsetup, TestGroupArray* testGroup, unsigned id, std::string name, bool isDst) {
    TestData& data = testGroup->getData(0);
    unsigned vecSize = isDst? data.dst.getDim() : data.mem.getDim();
    BrigTypeX type = (BrigTypeX) (isDst? data.dst.getValType() : data.mem.getValType());
    uint32_t sizes[3] = { BufferArraySize(type, vecSize * testGroup->getFlatSize()), 1, 1 };
    MBuffer* mb = new MBuffer(id++, name, MEM_GLOBAL,
                              BufferValueType(type),
                              1, sizes);
    dsetup->MSetup().Add(mb);
    dsetup->MSetup().Add(NewMValue(id++, std::string("Arg ") + name,
                                MEM_KERNARG, MV_REF, R(mb->Id())));
    MRBuffer* mr = new MRBuffer(id++, name + " (check)", mb->VType(), mb->Id());
    for (unsigned flatIdx = 0; flatIdx < testGroup->getFlatSize(); ++flatIdx) {
      TestData& data = testGroup->getData(flatIdx);
      for (unsigned k = 0; k < vecSize; ++k) {
        Val val = isDst? data.dst[k] : data.mem[k];
        Val2Value(mr->Data(), val);
      }
    }
    dsetup->MSetup().Add(mr);
    return id;
  }

  ValueType BufferValueType(BrigTypeX type) {
    return Brig2ValueType(type);
  }

  unsigned BufferArraySize(BrigTypeX type, unsigned size) {
    return size * Brig2ValueCount(type);
  }

  void Val2Value(Values& values, Val val) {
    unsigned type = val.getType();
    switch (type) {
    case BRIG_TYPE_B1: values.push_back(Value(MV_UINT32, val.b1())); return;
    case BRIG_TYPE_B8: values.push_back(Value(MV_UINT32, val.b8())); return;
    case BRIG_TYPE_U8: values.push_back(Value(MV_UINT32, val.u8())); return;
    case BRIG_TYPE_S8: values.push_back(Value(MV_INT32, (int32_t)val.s8())); return;
    case BRIG_TYPE_B16: values.push_back(Value(MV_UINT32, val.b16())); return;
    case BRIG_TYPE_U16: values.push_back(Value(MV_UINT32, val.u16())); return;
    case BRIG_TYPE_S16: values.push_back(Value(MV_INT32, (int32_t)val.s16())); return;
    case BRIG_TYPE_B32: values.push_back(Value(MV_UINT32, val.b32())); return;
    case BRIG_TYPE_U32: values.push_back(Value(MV_UINT32, val.u32())); return;
    case BRIG_TYPE_S32: values.push_back(Value(MV_INT32, val.s32())); return;
    case BRIG_TYPE_B64: values.push_back(Value(MV_UINT64, val.b64())); return;
    case BRIG_TYPE_U64: values.push_back(Value(MV_UINT64, val.u64())); return;
    case BRIG_TYPE_S64: values.push_back(Value(MV_INT64, val.s64())); return;
    case BRIG_TYPE_F16: 
#ifdef MBUFFER_KEEP_F16_AS_U32
      values.push_back(Value(MV_FLOAT16_MBUFFER, val.getAsB16())); return;
#else
      values.push_back(Value(MV_FLOAT16, val.getAsB16())); return;
#endif
    case BRIG_TYPE_F64: values.push_back(Value(val.f64())); return;
    case BRIG_TYPE_F32: values.push_back(Value(val.f32())); return;
    case BRIG_TYPE_B128: {
      values.push_back(Value(MV_UINT64, U64(val.b128().get<uint64_t>(0))));
      values.push_back(Value(MV_UINT64, U64(val.b128().get<uint64_t>(1))));
      return;
    }
    case BRIG_TYPE_F16X2:
    case BRIG_TYPE_F16X4:
    case BRIG_TYPE_F16X8:
#ifdef MBUFFER_KEEP_F16_AS_U32
    {
      unsigned dim = getPackedTypeDim(val.getType());
      for (unsigned i = 0; i < dim; ++i) {
        values.push_back(Value(MV_FLOAT16, val.getPackedElement(i).getAsB16()));
      }
      return;
    }
#endif
    case BRIG_TYPE_F32X2:
    case BRIG_TYPE_F32X4:
    case BRIG_TYPE_F64X2: {
      unsigned dim = getPackedTypeDim(val.getType());
      for (unsigned i = 0; i < dim; ++i) Val2Value(values, val.getPackedElement(i));
      return;
    }
    default: {
      assert(isIntPackedType(type));
      unsigned size = getBrigTypeNumBits(type);
      switch (size) {
      case 32: values.push_back(Value(MV_UINT32, val.getAsB32())); return;
      case 64: values.push_back(Value(MV_UINT64, val.getAsB64())); return;
      case 128: {
        values.push_back(Value(MV_UINT64, U64(val.getAsB64(0))));
        values.push_back(Value(MV_UINT64, U64(val.getAsB64(1))));
        return;
      }
      default:
        std::cout << val.getType() << std::endl; assert(!"Unsupported type in Val2Value"); 
        return;
      }
    }
    }
  }

  hexl::ValueType Brig2ValueType(BrigTypeX type)
  {
    switch (type) {
    case BRIG_TYPE_B1: return MV_UINT32;
    case BRIG_TYPE_F16:
#ifdef MBUFFER_KEEP_F16_AS_U32
      return MV_FLOAT16_MBUFFER;
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

  unsigned Brig2ValueCount(BrigTypeX type)
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
  TestDataProvider::init(true, true, 0, 64, 0, context->Opts()->IsSet("XtestF16"));
  TestGenConfig* testGenConfig = context->Get<TestGenConfig*>(TestGenConfig::ID);
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

} // namespace TestGen

} // namespace hexl
