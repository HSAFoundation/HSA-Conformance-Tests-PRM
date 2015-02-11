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

#include "ImageQueryTests.hpp"
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

class ImageQuerySamplerTest:  public Test {
private:
  Sampler smpobj;

public:
  ImageQuerySamplerTest(Location codeLocation, 
      Grid geometry): Test(codeLocation, geometry)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry;
  }

  void Init() {
   Test::Init();
   smpobj = kernel->NewSampler("%sampler", BRIG_SEGMENT_KERNARG, BRIG_COORD_UNNORMALIZED, BRIG_FILTER_LINEAR, BRIG_ADDRESSING_UNDEFINED);
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
    return Value(MV_UINT32, 1);
  }

  size_t OutputBufferSize() const override {
    return 1000;
  }

  TypedReg Result() {
    auto result = be.AddTReg(ResultType());
   // Load input
    auto sampleraddr = be.AddTReg(smpobj->Variable().type());
    be.EmitLoad(smpobj->Segment(), sampleraddr->Type(), sampleraddr->Reg(), be.Address(smpobj->Variable())); 
    auto reg_dest = be.AddTReg(BRIG_TYPE_U32);
    smpobj->EmitSamplerQuery(reg_dest, sampleraddr, BRIG_SAMPLER_QUERY_FILTER);
    be.EmitMov(result, reg_dest);
    return result;
  }
};

class ImageQueryImageTest:  public Test {
private:
  Image imgobj;

public:
  ImageQueryImageTest(Location codeLocation, 
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
    return Value(MV_UINT32, 1000);
  }

  size_t OutputBufferSize() const override {
    return 1000;
  }

  TypedReg Result() {
    auto result = be.AddTReg(ResultType());
   // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 
    auto reg_dest = be.AddTReg(BRIG_TYPE_U32);
    imgobj->EmitImageQuery(reg_dest, imageaddr, BRIG_IMAGE_QUERY_WIDTH);
    be.EmitMov(result, reg_dest);
    return result;
  }
};

void ImageQueryTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageQueryImageTest>(ap, it, "image_query_1d_image/basic", CodeLocations(), cc->Grids().DimensionSet());
  TestForEach<ImageQuerySamplerTest>(ap, it, "image_query_1d_sampler/basic", CodeLocations(), cc->Grids().DimensionSet());
}

} // hsail_conformance
