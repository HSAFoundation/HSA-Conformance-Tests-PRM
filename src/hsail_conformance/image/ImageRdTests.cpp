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

#define MAX_ALLOWED_ERROR_FOR_LINEAR_FILTERING "relf=0.001"

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
  SamplerParams samplerParams;
  Value color[4];
  BrigType coordType;

public:
  ImageRdTest(Location codeLocation, 
      Grid geometry, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, 
      SamplerParams* samplerParams_,
      BrigType coordType_, unsigned Array_ = 1): Test(codeLocation, geometry), 
      imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_),
      samplerParams(*samplerParams_), coordType(coordType_)
  {
     imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2), Array_);
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '/' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" <<
      ImageChannelOrderString(MObjectImageChannelOrder(imageChannelOrder)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType)) << "_" <<
      samplerParams << "_" << type2str(coordType);
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
    imgobj = kernel->NewImage("%roimage", HOST_INPUT_IMAGE, &imageSpec);
    imgobj->SetInitialData(imgobj->GenMemValue(Value(MV_UINT32, 0x45245833)));
 
    ESamplerSpec samplerSpec(BRIG_SEGMENT_KERNARG);
    samplerSpec.Coord(samplerParams.Coord());
    samplerSpec.Filter(samplerParams.Filter());
    samplerSpec.Addressing(samplerParams.Addressing());
    smpobj = kernel->NewSampler("%sampler", &samplerSpec);

    imgobj->InitImageCalculator(smpobj);

    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
      if(samplerParams.Filter() == BRIG_FILTER_LINEAR) {
        output->SetComparisonMethod(MAX_ALLOWED_ERROR_FOR_LINEAR_FILTERING ",minf=-1.0,maxf=1.0");
      }else{
        output->SetComparisonMethod("ulps=2,minf=-1.0,maxf=1.0"); //1.5ulp [-1.0; 1.0]
      }
      break;
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
      if(samplerParams.Filter() == BRIG_FILTER_LINEAR) {
        output->SetComparisonMethod(MAX_ALLOWED_ERROR_FOR_LINEAR_FILTERING ",minf=0.0,maxf=1.0");
      }else{
        switch (imageChannelOrder)
        {
        case BRIG_CHANNEL_ORDER_SRGB:
        case BRIG_CHANNEL_ORDER_SRGBX:
        case BRIG_CHANNEL_ORDER_SRGBA:
        case BRIG_CHANNEL_ORDER_SBGRA:
          output->SetComparisonMethod("ulps=3,minf=0.0,maxf=1.0"); // s-* channel orders are doing additional math (PRM 7.1.4.1.2)
          break;
        default:
          output->SetComparisonMethod("ulps=2,minf=0.0,maxf=1.0"); //1.5ulp [0.0; 1.0]
          break;
        }
      }
      break;
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
      //integer types are compared for equality
      break;
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      if(samplerParams.Filter() == BRIG_FILTER_LINEAR) {
        output->SetComparisonMethod(MAX_ALLOWED_ERROR_FOR_LINEAR_FILTERING);
      }else{
        output->SetComparisonMethod("ulps=0"); //f16 denorms should not be flushed (as it will produce normalized f32)
      }
      break;
    case BRIG_CHANNEL_TYPE_FLOAT:
      if(samplerParams.Filter() == BRIG_FILTER_LINEAR) {
        output->SetComparisonMethod(MAX_ALLOWED_ERROR_FOR_LINEAR_FILTERING ",flushDenorms");
      }else{
        output->SetComparisonMethod("ulps=0,flushDenorms"); //flushDenorms
      }
      break;
    default:
      break;
    }
    
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  uint64_t ResultDim() const override {
    return IsImageDepth(imageGeometryProp) ? 1 : 4;
  }

  void ExpectedResults(Values* result) const {
    uint16_t channels = IsImageDepth(imageGeometryProp) ? 1 : 4;
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++){
          Value coords[3];
          Value texel[4];
          int arrayCoord = -1; //array coord is always unnormalised
          switch (imageGeometryProp)
          {
          case BRIG_GEOMETRY_1DA:
            arrayCoord = 1;
            break;
          case BRIG_GEOMETRY_2DA:
          case BRIG_GEOMETRY_2DADEPTH:
            arrayCoord = 2;
            break;
          default:
            break;
          }
          switch (coordType)
          {
          case BRIG_TYPE_S32:
            coords[0] = Value(MV_INT32, S32(x));
            coords[1] = Value(MV_INT32, S32(y));
            coords[2] = Value(MV_INT32, S32(z));
            break;

          case BRIG_TYPE_F32:{
            double fcoords[3];
            fcoords[0] = x;
            fcoords[1] = y;
            fcoords[2] = z;

            for(int k = 0; k < 3; k++)
            {
              //avoiding accessing out of range texels
              if(samplerParams.Addressing() == BRIG_ADDRESSING_UNDEFINED && samplerParams.Filter() == BRIG_FILTER_LINEAR && k != arrayCoord)
                fcoords[k] = std::max(fcoords[k], 1.0);

              //normalize coordinates
              if(samplerParams.Coord() == BRIG_COORD_NORMALIZED && k != arrayCoord)
                fcoords[k] /= imageGeometry.ImageSize(k);
              
              coords[k] = Value((float)fcoords[k]);
            }
            }
            break;
          default:
            assert(!"Illegal coordinate type");
            break;
          }
          
          imgobj->ReadColor(coords, texel);
          for (unsigned i = 0; i < channels; i++)
            result->push_back(texel[i]);
        }
  }

  bool IsValid() const override {
    //only f32 access type is supported for linear filter
    if (samplerParams.Filter() == BRIG_FILTER_LINEAR && ImageAccessType(imageChannelType) != BRIG_TYPE_F32)
      return false;

    //only f32 coordinates are supported for linear filter
    if (samplerParams.Filter() == BRIG_FILTER_LINEAR && coordType != BRIG_TYPE_F32)
      return false;

    //only f32 coordinates is supported for normalized sampler
    if (samplerParams.Coord() == BRIG_COORD_NORMALIZED && coordType != BRIG_TYPE_F32)
      return false;
    
    //With undefinied addressing we should not touch any out of range texels.
    //As linear filtering requires 2, 2x2 or 2x2x2 texels we should avoid slim images.
    if (samplerParams.Filter() == BRIG_FILTER_LINEAR && samplerParams.Addressing() == BRIG_ADDRESSING_UNDEFINED)
    {
      switch (imageGeometryProp)
      {
      case BRIG_GEOMETRY_1D:
      case BRIG_GEOMETRY_1DA:
        if(imageGeometry.ImageSize(0) < 2)
          return false;
        break;

      case BRIG_GEOMETRY_2D:
      case BRIG_GEOMETRY_2DA:
      case BRIG_GEOMETRY_2DDEPTH:
      case BRIG_GEOMETRY_2DADEPTH:
        if(imageGeometry.ImageSize(0) < 2 || imageGeometry.ImageSize(1) < 2)
          return false;
        break;
        
      case BRIG_GEOMETRY_3D:
        if(imageGeometry.ImageSize(0) < 2 || imageGeometry.ImageSize(1) < 2 || imageGeometry.ImageSize(3) < 2)
          return false;
        break;
      default:
        break;
      }
    }
    
    if (!samplerParams.IsValid()) { return false; }
    return IsImageLegal(imageGeometryProp, imageChannelOrder, imageChannelType) && IsImageGeometrySupported(imageGeometryProp, imageGeometry) && (codeLocation != FUNCTION);
  }
 
  BrigType ResultType() const {
    return ImageAccessType(imageChannelType); 
  }

  TypedReg GetCoords()
  {
    int dim = 0;
    int arrayCoord = -1;
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      dim = 1;
      break;
    case BRIG_GEOMETRY_1DA:
    case BRIG_GEOMETRY_2D:
    case BRIG_GEOMETRY_2DDEPTH:
      dim = 2;
      break;
    case BRIG_GEOMETRY_3D:
    case BRIG_GEOMETRY_2DA:
    case BRIG_GEOMETRY_2DADEPTH:
      dim = 3;
      break;
    default:
      assert(0);
    }
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1DA:
      arrayCoord = 1;
      break;
    case BRIG_GEOMETRY_2DA:
    case BRIG_GEOMETRY_2DADEPTH:
      arrayCoord = 2;
      break;
    default:
      break;
    }

    auto result = be.AddTReg(coordType, dim);

    for(int k=0; k<dim; k++){
      auto gid = be.EmitWorkitemAbsId(k, false);
      switch (coordType)
      {
      case BRIG_TYPE_S32:
        be.EmitMov(result->Reg(k), gid->Reg(), 32);
        break;

      case BRIG_TYPE_F32:{
        //avoiding accessing out of range texels
        if(samplerParams.Addressing() == BRIG_ADDRESSING_UNDEFINED && samplerParams.Filter() == BRIG_FILTER_LINEAR)
          if(k != arrayCoord) be.EmitArith(BRIG_OPCODE_MAX, gid, gid, be.Immed(BRIG_TYPE_U32, 1));
        auto fgid = be.AddTReg(coordType);
        
        be.EmitCvt(fgid, gid, BRIG_ROUND_FLOAT_DEFAULT);

        //normalize coordinates
        if(samplerParams.Coord() == BRIG_COORD_NORMALIZED && k != arrayCoord){
          TypedReg divisor = be.AddTReg(BRIG_TYPE_F32);
          TypedReg dimSize = be.AddTReg(BRIG_TYPE_U32);
          be.EmitMov(dimSize, be.Immed(BRIG_TYPE_U32, imageGeometry.ImageSize(k)));
          be.EmitCvt(divisor, dimSize);
          be.EmitArith(BRIG_OPCODE_DIV, fgid, fgid, divisor->Reg());
        }

        be.EmitMov(result->Reg(k), fgid->Reg(), 32);
        }
        break;
      default:
        assert(!"Illegal coord type");
        break;
      }
    }

    return result;
  }

  TypedReg Result() {
   // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 

    auto sampleraddr = be.AddTReg(smpobj->Variable().type());
    be.EmitLoad(smpobj->Segment(), sampleraddr->Type(), sampleraddr->Reg(), be.Address(smpobj->Variable())); 

    auto regs_dest = IsImageDepth(imageGeometryProp) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);
    auto coords = GetCoords();

    imgobj->EmitImageRd(regs_dest,  imageaddr, sampleraddr, coords);

    return regs_dest;
  }
};

void ImageRdTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageRdTest>(ap, it, "image_rd/basic", CodeLocations(), cc->Grids().ImagesSet(),
     cc->Images().ImageRdGeometryProp(), cc->Images().ImageSupportedChannelOrders(), cc->Images().ImageChannelTypes(),
     cc->Samplers().All(), cc->Images().ImageRdCoordinateTypes(), cc->Images().ImageArraySets());
}

} // hsail_conformance
