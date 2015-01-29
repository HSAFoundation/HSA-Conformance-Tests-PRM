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

#include "ImageTests.hpp"
#include "HCTests.hpp"
#include "MObject.hpp"

using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageBaseTest:  public Test {

public:
  ImageBaseTest(Location codeLocation, 
      Grid geometry): Test(codeLocation, geometry)
  {
  }

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry;
  }
};


class ImageRdTest:  public ImageBaseTest {
private:
  DirectiveVariable img;
  DirectiveVariable samp;

public:
  ImageRdTest(Location codeLocation, 
      Grid geometry): ImageBaseTest(codeLocation, geometry)
  {
  }

 void KernelArguments() {
    Test::KernelArguments();
    img = be.EmitVariableDefinition("%roimg", BRIG_SEGMENT_KERNARG, BRIG_TYPE_ROIMG);
    samp = be.EmitVariableDefinition("%sampler", BRIG_SEGMENT_KERNARG, BRIG_TYPE_SAMP);

  }

 void SetupDispatch(DispatchSetup* dsetup) {
    Test::SetupDispatch(dsetup);
    unsigned id = dsetup->MSetup().Count();
    
   // MObject* in = NewMValue(id++, "Input", MEM_GLOBAL, addressSpec->VType(), U64(42));
    //dsetup->MSetup().Add(in);
    //dsetup->MSetup().Add(NewMValue(id++, "Input (arg)", MEM_KERNARG, MV_REF, R(in->Id()))); 
  }

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
     return Value(MV_EXPR, 0);
  }

  TypedReg Result() {
   // Load input
    TypedReg image = be.AddTReg(BRIG_TYPE_ROIMG);
    be.EmitLoad(img.segment(), image, be.Address(img));
    TypedReg sampler = be.AddTReg(BRIG_TYPE_SAMP);
    be.EmitLoad(img.segment(), sampler, be.Address(samp));

    TypedReg reg_coord = be.AddTReg(BRIG_TYPE_F32);
    //10 coords
    be.EmitMov(reg_coord, be.Immed(reg_coord->Type(), 10));

    OperandOperandList reg_dest = be.AddVec(BRIG_TYPE_U32, 4);
    
   // be.EmitRdImage(reg_dest, image, sampler, reg_coord);
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, reg_dest.elements(0));
    return result;
  }
};

void ImageTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageRdTest>(ap, it, "image/image_rd/basic", CodeLocations(), cc->Grids().DimensionSet());
}

} // hsail_conformance
