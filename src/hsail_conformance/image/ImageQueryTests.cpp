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

#include "ImageQueryTests.hpp"
#include "RuntimeContext.hpp"
#include "HCTests.hpp"
#include "MObject.hpp"

using namespace hexl::emitter;
using namespace hexl::scenario;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageQuerySamplerTest:  public Test {
private:
  Sampler smpobj;
  SamplerParams samplerParams;
  BrigSamplerQuery samplerQuery;

public:
  ImageQuerySamplerTest(Location codeLocation, Grid geometry, 
    SamplerParams* samplerParams_, BrigSamplerQuery samplerQuery_): Test(codeLocation, geometry),
    samplerParams(*samplerParams_), samplerQuery(samplerQuery_)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << "_" <<
      samplerParams << "_" << SamplerQueryString(MObjectSamplerQuery(samplerQuery));
  }

  void Init() {
   Test::Init();
   ESamplerSpec samplerSpec(BRIG_SEGMENT_KERNARG);
   samplerSpec.Params(samplerParams);
   smpobj = kernel->NewSampler("%sampler", &samplerSpec);
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  bool IsValid() const
  {
    //if ( samplerAddressing == BRIG_ADDRESSING_UNDEFINED )  //TODO: Would we need skeep this ? 
    //{
    //  return false;
    //}

    return samplerParams.IsValid() && (codeLocation != FUNCTION);
  }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    switch (samplerQuery)
    {
    case BRIG_SAMPLER_QUERY_ADDRESSING:
      return Value(MV_UINT32, samplerParams.Addressing());
    case BRIG_SAMPLER_QUERY_COORD:
      return Value(MV_UINT32, samplerParams.Coord());
    case BRIG_SAMPLER_QUERY_FILTER:
      return Value(MV_UINT32, samplerParams.Filter());
    default:
      assert(0);
      break;
    }
    return Value(MV_UINT32, 0);
  }

  TypedReg Result() {
    auto result = be.AddTReg(ResultType());
   // Load input
    auto sampleraddr = be.AddTReg(smpobj->Variable().type());
    be.EmitLoad(smpobj->Segment(), sampleraddr->Type(), sampleraddr->Reg(), be.Address(smpobj->Variable())); 
    auto reg_dest = be.AddTReg(BRIG_TYPE_U32);
    smpobj->EmitSamplerQuery(reg_dest, sampleraddr, samplerQuery);
    be.EmitMov(result, reg_dest);
    return result;
  }
};

class ImageQueryTest:  public Test {
private:
  Image imgobj;
  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;
  BrigImageQuery imageQuery;

public:
  ImageQueryTest(Location codeLocation, Grid geometry, 
    BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_, unsigned Array_ = 1): Test(codeLocation, geometry),
    imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_), imageQuery(imageQuery_)
  {
    imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2), Array_);
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '\\' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelOrderString(MObjectImageChannelOrder(imageChannelOrder)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType)) << "_" <<
      ImageQueryString(MObjectImageQuery(imageQuery));
  }

  bool IsValid() const override {
    return IsImageLegal(imageGeometryProp, imageChannelOrder, imageChannelType) && IsImageQueryGeometrySupport(imageGeometryProp, imageQuery) && IsImageGeometrySupported(imageGeometryProp, imageGeometry) && (codeLocation != FUNCTION);
  }

  void Init() {
   Test::Init();

   EImageSpec imageSpec(BRIG_SEGMENT_KERNARG, BRIG_TYPE_ROIMG);
   imageSpec.Geometry(imageGeometryProp);
   imageSpec.ChannelOrder(imageChannelOrder);
   imageSpec.ChannelType(imageChannelType);
   imageSpec.Width(imageGeometry.ImageWidth());
   imageSpec.Height(imageGeometry.ImageHeight());
   imageSpec.Depth(imageGeometry.ImageDepth());
   imageSpec.ArraySize(imageGeometry.ImageArray());
   imgobj = kernel->NewImage("%roimage", HOST_INPUT_IMAGE, &imageSpec, IsImageOptional(imageGeometryProp, imageChannelOrder, imageChannelType, BRIG_TYPE_ROIMG));
   imgobj->SetInitialData(imgobj->GenMemValue(Value(MV_UINT8, 0xFF)));
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    switch(imageQuery)
    {
    case BRIG_IMAGE_QUERY_WIDTH:
      return Value(MV_UINT32, imageGeometry.ImageWidth());
    case BRIG_IMAGE_QUERY_HEIGHT:
      return Value(MV_UINT32, imageGeometry.ImageHeight());
    case BRIG_IMAGE_QUERY_DEPTH:
      return Value(MV_UINT32, imageGeometry.ImageDepth());
    case BRIG_IMAGE_QUERY_ARRAY:
      return Value(MV_UINT32, imageGeometry.ImageArray());
    case BRIG_IMAGE_QUERY_CHANNELORDER:
      return Value(MV_UINT32, imageChannelOrder);
    case BRIG_IMAGE_QUERY_CHANNELTYPE:
      return Value(MV_UINT32, imageChannelType);
    default:
      assert(0);
    }
    return Value(MV_UINT32, 0);
  }

  size_t OutputBufferSize() const override {
    return imageGeometry.ImageSize()*4;
  }

  TypedReg Result() {
    auto result = be.AddTReg(ResultType());
   // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 
    auto reg_dest = be.AddTReg(BRIG_TYPE_U32);
    imgobj->EmitImageQuery(reg_dest, imageaddr, imageQuery);
    be.EmitMov(result, reg_dest);
    return result;
  }
};

void ImageQueryTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();

  TestForEach<ImageQueryTest>(ap, it, "image_query/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelOrders(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes(), cc->Images().ImageArraySets());
 
  TestForEach<ImageQuerySamplerTest>(ap, it, "image_query_sampler/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Samplers().All(), cc->Samplers().SamplerQueryTypes());
}

} // hsail_conformance
