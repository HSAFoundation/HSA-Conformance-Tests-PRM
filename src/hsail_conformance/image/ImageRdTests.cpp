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
#include "HCTests.hpp"
#include "MObject.hpp"

#define FIXME_UNUSED(var) (void)var

using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageRdTest:  public Test {
private:
  DirectiveVariable img;
  DirectiveVariable samp;

public:
  ImageRdTest(Location codeLocation, 
      Grid geometry): Test(codeLocation, geometry)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry;
  }

 void KernelArguments() {
    Test::KernelArguments();
    img = be.EmitVariableDefinition("%roimage", BRIG_SEGMENT_KERNARG, BRIG_TYPE_ROIMG);
    samp = be.EmitVariableDefinition("%sampler", BRIG_SEGMENT_KERNARG, BRIG_TYPE_SAMP);

  }

 void SetupDispatch(DispatchSetup* dsetup) {
    Test::SetupDispatch(dsetup);
    unsigned id = dsetup->MSetup().Count();
    
    MImage* in1 = NewMValue(id++, "Roimage", BRIG_GEOMETRY_1D, BRIG_CHANNEL_ORDER_A, BRIG_CHANNEL_TYPE_SNORM_INT8, 1 /*HSA_ACCESS_PERMISSION_RO*/, 256,1,1,256,1,U8(42));
    dsetup->MSetup().Add(in1);
    dsetup->MSetup().Add(NewMValue(id++, "Roimage (arg)", MEM_KERNARG, MV_REF, R(in1->Id())));

    MSampler* in2 = NewMValue(id++, "Sampler", BRIG_COORD_NORMALIZED, BRIG_FILTER_NEAREST, BRIG_ADDRESSING_CLAMP_TO_EDGE);
    dsetup->MSetup().Add(in2);
    dsetup->MSetup().Add(NewMValue(id++, "Sampler (arg)", MEM_KERNARG, MV_REF, R(in2->Id())));
  }

  BrigTypeX ResultType() const { return BRIG_TYPE_U8; }

  Value ExpectedResult() const {
     return Value(MV_EXPR, U8(42));
  }

  TypedReg Result() {
   // Load input
    TypedReg image = be.AddTReg(BRIG_TYPE_ROIMG);
    FIXME_UNUSED(image); // be.EmitLoad(img.segment(), image, be.Address(img));
    TypedReg sampler = be.AddTReg(BRIG_TYPE_SAMP);
    FIXME_UNUSED(sampler); // be.EmitLoad(img.segment(), sampler, be.Address(samp));

    TypedReg reg_coord = be.AddTReg(BRIG_TYPE_F32);
    //10 coords
    be.EmitMov(reg_coord, be.Immed(reg_coord->Type(), 10));

    OperandOperandList reg_dest = be.AddVec(BRIG_TYPE_U32, 4);
    
   // be.EmitRdImage(reg_dest, image, sampler, reg_coord);
    TypedReg result = be.AddTReg(BRIG_TYPE_U8);
    be.EmitMov(result, reg_dest.elements(0));
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
