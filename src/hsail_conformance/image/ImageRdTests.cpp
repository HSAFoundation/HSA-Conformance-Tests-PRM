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

class ImageRdTestBase:  public Test {
private:
  Image imgobj;
  Sampler smpobj;

  ImageGeometry* imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;
  BrigSamplerCoordNormalization samplerCoord;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestBase(Location codeLocation, 
      Grid geometry, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): Test(codeLocation, geometry), 
      imageGeometry(imageGeometry_), imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_), 
      samplerCoord(samplerCoord_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '\\' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType)) << "_" <<
      SamplerCoordsString(MObjectSamplerCoords(samplerCoord)) << "_" << SamplerFilterString(MObjectSamplerFilter(samplerFilter)) << "_" << SamplerAddressingString(MObjectSamplerAddressing(samplerAddressing));
  }

  void Init() {
   Test::Init();

   EImageSpec imageSpec(BRIG_SEGMENT_KERNARG, BRIG_TYPE_ROIMG);
   imageSpec.Geometry(imageGeometryProp);
   imageSpec.ChannelOrder(imageChannelOrder);
   imageSpec.ChannelType(imageChannelType);
   imageSpec.Width(imageGeometry->ImageWidth());
   imageSpec.Height(imageGeometry->ImageHeight());
   imageSpec.Depth(imageGeometry->ImageDepth());
   imageSpec.ArraySize(imageGeometry->ImageArray());
   imgobj = kernel->NewImage("%roimage", &imageSpec);
   for (unsigned i = 0; i < imageGeometry->ImageSize(); ++i) { imgobj->AddData(Value(MV_UINT32, 0xFFFFFFFF)); }
   smpobj = kernel->NewSampler("%sampler", BRIG_SEGMENT_KERNARG, samplerCoord, samplerFilter, samplerAddressing);
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  bool IsValid() const {
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      if ((imageGeometry->ImageHeight() > 1) || (imageGeometry->ImageDepth() > 1) || (imageGeometry->ImageArray() > 1))
        return false;
      break;
    case BRIG_GEOMETRY_1DA:
      if ((imageGeometry->ImageHeight() > 1) || (imageGeometry->ImageDepth() > 1))
        return false;
      break;
     case BRIG_GEOMETRY_2D:
     case BRIG_GEOMETRY_2DDEPTH:
      if ((imageGeometry->ImageHeight() < 2) || (imageGeometry->ImageDepth() > 1) || (imageGeometry->ImageArray() > 1))
        return false;
      break;
    case BRIG_GEOMETRY_2DA:
      if ((imageGeometry->ImageHeight() < 2) || (imageGeometry->ImageDepth() > 1))
        return false;
      break;
    case BRIG_GEOMETRY_2DADEPTH:
      if (imageGeometry->ImageDepth() > 1)
        return false;
      break;
    case BRIG_GEOMETRY_3D:
      if ((imageGeometry->ImageHeight() < 2) || (imageGeometry->ImageDepth() < 2) || (imageGeometry->ImageArray() > 1))
        return false;
      break;
    default:
      if (imageGeometry->ImageArray() > 1)
        return false;
    }
    return (codeLocation != FUNCTION);
  }

  BrigTypeX ResultType() const {
    return BRIG_TYPE_U32; 
  }

  size_t OutputBufferSize() const override {
    return 1000;
  }

  Value CalculateValue()
  {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                //TODO: check this value
                return Value(MV_UINT32, 0);
                //return Value(MV_UINT32, 0xB7000100);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
  }

  TypedReg Result() {
    auto result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(BRIG_TYPE_U32, 0));
   // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 

    auto sampleraddr = be.AddTReg(smpobj->Variable().type());
    be.EmitLoad(smpobj->Segment(), sampleraddr->Type(), sampleraddr->Reg(), be.Address(smpobj->Variable())); 

    OperandOperandList regs_dest;
    auto reg_dest = be.AddTReg(BRIG_TYPE_U32, 1);
    OperandOperandList coords;
    auto coord = be.AddTReg(BRIG_TYPE_F32, 1);
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      be.EmitMov(coord, be.Immed(BRIG_TYPE_F32, 0));
      imgobj->EmitImageRd(regs_dest, BRIG_TYPE_U32,  imageaddr, sampleraddr, coord);
      break;
    case BRIG_GEOMETRY_1DA:
    case BRIG_GEOMETRY_2D:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      coords = be.AddVec(BRIG_TYPE_F32, 2);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_F32, 0), 32);
      }
      imgobj->EmitImageRd(regs_dest, BRIG_TYPE_U32,  imageaddr, sampleraddr, coords, BRIG_TYPE_F32);
      break;
    case BRIG_GEOMETRY_2DDEPTH:
      coords = be.AddVec(BRIG_TYPE_F32, 2);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_F32, 0), 32);
      }
      imgobj->EmitImageRd(reg_dest, imageaddr, sampleraddr, coords, BRIG_TYPE_F32);
      break;
    case BRIG_GEOMETRY_3D:
    case BRIG_GEOMETRY_2DA:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      coords = be.AddVec(BRIG_TYPE_F32, 3);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_F32, 0), 32);
      }
      imgobj->EmitImageRd(regs_dest, BRIG_TYPE_U32,  imageaddr, sampleraddr, coords, BRIG_TYPE_F32);
      break;
    case BRIG_GEOMETRY_2DADEPTH:
      coords = be.AddVec(BRIG_TYPE_F32, 3);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_F32, 0), 32);
      }
      imgobj->EmitImageRd(reg_dest, imageaddr, sampleraddr, coords, BRIG_TYPE_F32);
      break;
    default:
      assert(0);
    }

    if ((imageGeometryProp == BRIG_GEOMETRY_2DDEPTH) || (imageGeometryProp == BRIG_GEOMETRY_2DADEPTH)) {
      be.EmitMov(result, reg_dest);
    }
    else {
      if (imageChannelOrder == BRIG_CHANNEL_ORDER_A)
      {
        be.EmitMov(result, regs_dest.elements(3));
      }
      else
      {
        be.EmitMov(result, regs_dest.elements(0));
      }
    }
    return result;
  }
};


class ImageRdTestA:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;
public:
  ImageRdTestA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_A, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                //TODO: check this value
                return Value(MV_UINT32, 0);
                //return Value(MV_UINT32, 0xB7000100);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestR:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestR(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_R, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};


class ImageRdTestRX:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestRX(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RX, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRG:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestRG(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RG, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRGX:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestRGX(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGX, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRA:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestRA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RA, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRGB:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestRGB(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGB, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
      break;
    default:
      return false;
    }
    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRGBX:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestRGBX(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBX, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRGBA:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestRGBA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBA, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestBGRA:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestBGRA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_BGRA, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      break;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestARGB:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestARGB(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ARGB, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      break;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};


class ImageRdTestABGR:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestABGR(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ABGR, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      break;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};


class ImageRdTestSRGB:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestSRGB(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGB, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }


};


class ImageRdTestSRGBX:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestSRGBX(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBX, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestSRGBA:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestSRGBA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBA, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestSBGRA:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestSBGRA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SBGRA, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      return false;
    default:
      break;
    }

    return ImageRdTestBase::IsValid();
  }

};


class ImageRdTestINTENSITY:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestINTENSITY(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_INTENSITY, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    case BRIG_CHANNEL_TYPE_FLOAT:
      break;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestLUMINANCE:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestLUMINANCE(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_LUMINANCE, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0xFF);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0xFFFF);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0xFFFFFFFF);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0xBA810204);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0);
              default:
                break;
              }
              return Value(MV_UINT32, 0xBB810204);
            }
        }
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000000);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    case BRIG_CHANNEL_TYPE_FLOAT:
      break;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestDEPTH:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestDEPTH(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_INT24:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_FLOAT:
      break;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestDEPTHSTENCIL:  public ImageRdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestDEPTHSTENCIL(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH_STENCIL, imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_INT24:
       if (samplerFilter == BRIG_FILTER_LINEAR) {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case BRIG_GEOMETRY_1D:
              case BRIG_GEOMETRY_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case BRIG_GEOMETRY_2D:
              case BRIG_GEOMETRY_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case BRIG_GEOMETRY_3D:
                return Value(MV_UINT32, 0x3E000080);
              default:
                break;
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_FLOAT:
      break;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

void ImageRdTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageRdTestA>(ap, it, "image_rd_a/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
     cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestR>(ap, it, "image_rd_r/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
     cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestRX>(ap, it, "image_rd_rx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRG>(ap, it, "image_rd_rg/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestRGX>(ap, it, "image_rd_rgx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRA>(ap, it, "image_rd_ra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRGB>(ap, it, "image_rd_rgb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestRGBX>(ap, it, "image_rd_rgbx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRGBA>(ap, it, "image_rd_rgba/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestBGRA>(ap, it, "image_rd_bgra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestARGB>(ap, it, "image_rd_argb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestABGR>(ap, it, "image_rd_abgr/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestSRGB>(ap, it, "image_rd_srgb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestSRGBX>(ap, it, "image_rd_srgbx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestSRGBA>(ap, it, "image_rd_srgba/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestSBGRA>(ap, it, "image_rd_sbgra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestINTENSITY>(ap, it, "image_rd_intensity/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestLUMINANCE>(ap, it, "image_rd_luminance/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestDEPTH>(ap, it, "image_rd_depth/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  //TestForEach<ImageRdTestDEPTHSTENCIL>(ap, it, "image_rd_depth_stencil/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
  //  cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());
}

} // hsail_conformance
