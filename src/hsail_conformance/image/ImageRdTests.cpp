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
#include <math.h>

using namespace hexl::emitter;
using namespace hexl::scenario;
using namespace HSAIL_ASM;
using namespace hexl;

namespace hsail_conformance {

class ImageRdTest:  public Test {
private:
  Image imgobj;
  Sampler smpobj;

  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;
  BrigSamplerCoordNormalization samplerCoord;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;
  Value color[4];

public:
  ImageRdTest(Location codeLocation, 
      Grid geometry, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_, unsigned Array_ = 1): Test(codeLocation, geometry), 
      imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_), 
      samplerCoord(samplerCoord_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
     imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2), Array_);
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '/' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelOrderString(MObjectImageChannelOrder(imageChannelOrder)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType)) << "_" <<
      SamplerCoordsString(MObjectSamplerCoords(samplerCoord)) << "_" << SamplerFilterString(MObjectSamplerFilter(samplerFilter)) << "_" << SamplerAddressingString(MObjectSamplerAddressing(samplerAddressing));
  }

  ImageCalc calc;

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
    imgobj = kernel->NewImage("%roimage", &imageSpec);
    imgobj->AddData(imgobj->GenMemValue(Value(MV_UINT32, 0x32323232)));
 
    ESamplerSpec samplerSpec(BRIG_SEGMENT_KERNARG);
    samplerSpec.CoordNormalization(samplerCoord);
    samplerSpec.Filter(samplerFilter);
    samplerSpec.Addresing(samplerAddressing);
    smpobj = kernel->NewSampler("%sampler", &samplerSpec);

    imgobj->InitImageCalculator(smpobj);

    Value coords[3];
    coords[0] = Value(0.0f);
    coords[1] = Value(0.0f);
    coords[2] = Value(0.0f);
    imgobj->ReadColor(coords, color);
    
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  uint64_t ResultDim() const override {
    return 4;
  }

  void ExpectedResults(Values* result) const {
    for (unsigned i = 0; i < 4; i ++)
    {
      Value res = Value(MV_UINT32, color[i].U32());
      result->push_back(res);
    }
  }

  bool IsValid() const override {
    if (samplerFilter == BRIG_FILTER_LINEAR) //only f32 access type is supported for linear filter
    {
      switch (imageChannelType)
      {
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNKNOWN:
      case BRIG_CHANNEL_TYPE_FIRST_USER_DEFINED:
        return false;
        break;
      default:
        break;
      }
	    //currently rd test will generate coordinate 0, which will sample out of range texel
	    //for linear filtering. We should not test it, because it implementation defined.
	    //TODO:
	    //Change rd test with linear filter and undefined addressing
	    //to not read from edge of an image
	    if(samplerAddressing == BRIG_ADDRESSING_UNDEFINED)
		    return false;
    }
    
    if(!IsSamplerLegal(samplerCoord, samplerFilter, samplerAddressing))
      return false;
    return IsImageLegal(imageGeometryProp, imageChannelOrder, imageChannelType) && IsImageGeometrySupported(imageGeometryProp, imageGeometry) && (codeLocation != FUNCTION);
  }
 
  BrigType ResultType() const {
    return BRIG_TYPE_U32; 
  }

  BrigType CoordType() const {
    return BRIG_TYPE_F32; 
  }

  TypedReg Get1dCoord()
  {
    auto result = be.AddTReg(CoordType());
    auto x = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result, x->Reg());
    return result;
  }

  TypedReg Get2dCoord()
  {
    auto result = be.AddTReg(CoordType(), 2);
    auto x = be.EmitWorkitemAbsId(1, false);
    auto y = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result->Reg(0), x->Reg(), 32);
    be.EmitMov(result->Reg(1), y->Reg(), 32);
    return result;
  }

  TypedReg Get3dCoord()
  {
    auto result = be.AddTReg(CoordType(), 3);
    auto x = be.EmitWorkitemAbsId(2, false);
    auto y = be.EmitWorkitemAbsId(1, false);
    auto z = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result->Reg(0), x->Reg(), 32);
    be.EmitMov(result->Reg(1), y->Reg(), 32);
    be.EmitMov(result->Reg(2), z->Reg(), 32);
    return result;
  }

  TypedReg Result() {
   // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 

    auto sampleraddr = be.AddTReg(smpobj->Variable().type());
    be.EmitLoad(smpobj->Segment(), sampleraddr->Type(), sampleraddr->Reg(), be.Address(smpobj->Variable())); 

    auto regs_dest = IsImageDepth(imageGeometryProp) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      imgobj->EmitImageRd(regs_dest,  imageaddr, sampleraddr, Get1dCoord());
      break;
    case BRIG_GEOMETRY_1DA:
    case BRIG_GEOMETRY_2D:
      imgobj->EmitImageRd(regs_dest, imageaddr, sampleraddr, Get2dCoord());
      break;
    case BRIG_GEOMETRY_2DDEPTH:
      imgobj->EmitImageRd(regs_dest, imageaddr, sampleraddr, Get2dCoord());
      break;
    case BRIG_GEOMETRY_3D:
    case BRIG_GEOMETRY_2DA:
      imgobj->EmitImageRd(regs_dest,  imageaddr, sampleraddr, Get3dCoord());
      break;
    case BRIG_GEOMETRY_2DADEPTH:
      imgobj->EmitImageRd(regs_dest, imageaddr, sampleraddr, Get3dCoord());
      break;
    default:
      assert(0);
    }

    return regs_dest;
  }
};

void ImageRdTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageRdTest>(ap, it, "image_rd/basic", CodeLocations(), cc->Grids().ImagesSet(),
     cc->Images().ImageRdGeometryProp(), cc->Images().ImageSupportedChannelOrders(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings(), cc->Images().ImageArraySets());
}

} // hsail_conformance
