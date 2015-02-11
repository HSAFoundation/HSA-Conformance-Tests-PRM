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

#include "ImageRdTests.hpp"
#include "RuntimeContext.hpp"
#include "HCTests.hpp"
#include "MObject.hpp"

using namespace hexl::emitter;
using namespace hexl::scenario;
using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageRdTest:  public Test {
private:
  Variable nx;
  Variable ny;
  Image imgobj;
  Sampler smpobj;

public:
  ImageRdTest(Location codeLocation, 
      Grid geometry): Test(codeLocation, geometry)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry;
  }

  void Init() {
   Test::Init();
   unsigned access = 0;

   access = 1; //HSA_ACCESS_PERMISSION_RO

  // access = 2; //HSA_ACCESS_PERMISSION_WO

  // access = 3; //HSA_ACCESS_PERMISSION_RW

   imgobj = kernel->NewImage("%roimage", BRIG_SEGMENT_KERNARG, BRIG_GEOMETRY_1D, BRIG_CHANNEL_ORDER_A, BRIG_CHANNEL_TYPE_UNSIGNED_INT8, access, 1000,1,1,1,1);
   smpobj = kernel->NewSampler("%sampler", BRIG_SEGMENT_KERNARG, BRIG_COORD_UNNORMALIZED, BRIG_FILTER_LINEAR, BRIG_ADDRESSING_UNDEFINED);

   nx = kernel->NewVariable("nx", BRIG_SEGMENT_KERNARG, BRIG_TYPE_U32);
   nx->PushBack(Value(MV_UINT32, 1000));
   ny = kernel->NewVariable("ny", BRIG_SEGMENT_KERNARG, BRIG_TYPE_U32);
   ny->PushBack(Value(MV_UINT32, 1));
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  bool IsValid() const
  {
    return (codeLocation != FUNCTION);
  }

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    return Value(MV_UINT32, 255);
  }

  size_t OutputBufferSize() const override {
    return 1000;
  }

  TypedReg Result() {
    auto x = be.EmitWorkitemId(0);
    auto y = be.EmitWorkitemId(1);
    auto nxReg = nx->AddDataReg();
    be.EmitLoad(nx->Segment(), nxReg->Type(), nxReg->Reg(), be.Address(nx->Variable())); 
    auto nyReg = ny->AddDataReg();
    be.EmitLoad(ny->Segment(), nyReg->Type(), nyReg->Reg(), be.Address(ny->Variable())); 
    auto result = be.AddTReg(ResultType());
    be.EmitMov(result, be.Immed(ResultType(), 0));
    SRef s_label_exit = "@exit";
    auto reg_c = be.AddTReg(BRIG_TYPE_B1);
    // x > nx
    be.EmitCmp(reg_c->Reg(), x, nxReg, BRIG_COMPARE_GT);
    be.EmitCbr(reg_c, s_label_exit);
    // y > ny
    be.EmitCmp(reg_c->Reg(), y, nyReg, BRIG_COMPARE_GT);
    be.EmitCbr(reg_c, s_label_exit);
   // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 

    auto sampleraddr = be.AddTReg(smpobj->Variable().type());
    be.EmitLoad(smpobj->Segment(), sampleraddr->Type(), sampleraddr->Reg(), be.Address(smpobj->Variable())); 

    OperandOperandList reg_dest = be.AddVec(BRIG_TYPE_U32, 4);

    auto coord = be.AddTReg(BRIG_TYPE_F32);
    be.EmitMov(coord, be.Immed(coord->Type(), 0));

    imgobj->EmitImageRd(reg_dest, BRIG_TYPE_U32,  imageaddr, sampleraddr, coord);
    
    be.EmitMov(result, reg_dest.elements(3));

    be.Brigantine().addLabel(s_label_exit);
    return result;
  }
};

void ImageRdTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageRdTest>(ap, it, "image_rd_1d/basic", CodeLocations(), cc->Grids().DimensionSet());
}

} // hsail_conformance
