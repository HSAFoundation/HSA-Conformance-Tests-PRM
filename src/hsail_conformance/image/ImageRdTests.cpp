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
  Variable nx;
  Variable ny;
  Image imgobj;
  Sampler smpobj;

  ImageGeometry* imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;
  BrigImageAccess imageAccess;
  BrigSamplerCoordNormalization samplerCoord;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

public:
  ImageRdTestBase(Location codeLocation, 
      Grid geometry, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): 
      imageGeometry(imageGeometry_), imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_), 
      samplerCoord(samplerCoord_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_), 
      Test(codeLocation, geometry)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '\\' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType)) << "_" <<
      SamplerCoordsString(MObjectSamplerCoords(samplerCoord)) << "_" << SamplerFilterString(MObjectSamplerFilter(samplerFilter)) << "_" << SamplerAddressingString(MObjectSamplerAddressing(samplerAddressing));
  }

  void Init() {
   Test::Init();

   imgobj = kernel->NewImage("%roimage", BRIG_SEGMENT_KERNARG, imageGeometryProp, imageChannelOrder, imageChannelType, BRIG_ACCESS_PERMISSION_RO, imageGeometry->ImageSize(0),imageGeometry->ImageSize(1),imageGeometry->ImageSize(2),imageGeometry->ImageSize(3),imageGeometry->ImageSize(4));
   smpobj = kernel->NewSampler("%sampler", BRIG_SEGMENT_KERNARG, samplerCoord, samplerFilter, samplerAddressing);

   nx = kernel->NewVariable("nx", BRIG_SEGMENT_KERNARG, BRIG_TYPE_U32);
   nx->PushBack(Value(MV_UINT32, imageGeometry->ImageSize(0)));
   ny = kernel->NewVariable("ny", BRIG_SEGMENT_KERNARG, BRIG_TYPE_U32);
   ny->PushBack(Value(MV_UINT32,  imageGeometry->ImageSize(1)));
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  //bool IsValid() const {
  //  return (codeLocation != FUNCTION);
  //}

  BrigTypeX ResultType() const {
    return BRIG_TYPE_U32; 
  }

  BrigTypeX ChannelComponentType() const {
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT8 :
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT24 :
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    case BRIG_CHANNEL_TYPE_FLOAT:
      return BRIG_TYPE_F32;
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
      return BRIG_TYPE_U32;
    }
    
    assert(0);
    return BRIG_TYPE_U32;
  }

  
  int ChannelComponentLen() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return 1;
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return 2;
      case BRIG_CHANNEL_TYPE_UNORM_INT24 :
      case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return 4;
    }
    assert(0);
    return 1;
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  size_t OutputBufferSize() const override {
    return imageGeometry->ImageSize();
  }

  TypedReg Result() {
    auto x = be.EmitWorkitemId(0);
    auto y = be.EmitWorkitemId(1);
    auto nxReg = nx->AddDataReg();
    be.EmitLoad(nx->Segment(), nxReg->Type(), nxReg->Reg(), be.Address(nx->Variable())); 
    auto nyReg = ny->AddDataReg();
    be.EmitLoad(ny->Segment(), nyReg->Type(), nyReg->Reg(), be.Address(ny->Variable())); 
    auto result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(result, be.Immed(BRIG_TYPE_U32, 0));
    SRef s_label_exit = "@exit";
    auto reg_c = be.AddTReg(BRIG_TYPE_B1);
    auto reg_mul1 = be.AddTReg(BRIG_TYPE_U32);
    auto reg_mul2 = be.AddTReg(BRIG_TYPE_U32);
    be.EmitArith(BRIG_OPCODE_MUL, reg_mul1, x->Reg(), y->Reg());
    be.EmitArith(BRIG_OPCODE_MUL, reg_mul2, nxReg->Reg(), nyReg->Reg());
    // x*y > nx*ny
    be.EmitCmp(reg_c->Reg(), reg_mul1, reg_mul2, BRIG_COMPARE_GT);
    be.EmitCbr(reg_c, s_label_exit);
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
    case IMG_1D:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      be.EmitMov(coord, be.Immed(BRIG_TYPE_F32, 0));
      imgobj->EmitImageRd(regs_dest, BRIG_TYPE_U32,  imageaddr, sampleraddr, coord);
      break;
    case IMG_1DA:
    case IMG_2D:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      coords = be.AddVec(BRIG_TYPE_F32, 2);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_F32, 0), 32);
      }
      imgobj->EmitImageRd(regs_dest, BRIG_TYPE_U32,  imageaddr, sampleraddr, coords, BRIG_TYPE_F32);
      break;
    case IMG_2DDEPTH:
      coords = be.AddVec(BRIG_TYPE_F32, 2);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_F32, 0), 32);
      }
      imgobj->EmitImageRd(reg_dest, imageaddr, sampleraddr, coords, BRIG_TYPE_F32);
      break;
    case IMG_3D:
    case IMG_2DA:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      coords = be.AddVec(BRIG_TYPE_F32, 3);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_F32, 0), 32);
      }
      imgobj->EmitImageRd(regs_dest, BRIG_TYPE_U32,  imageaddr, sampleraddr, coords, BRIG_TYPE_F32);
      break;
    case IMG_2DADEPTH:
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

    if ((imageGeometryProp == IMG_2DDEPTH) || (imageGeometryProp == IMG_2DADEPTH)) {
      be.EmitMov(result, reg_dest);
    }
    else {
      if (imageChannelOrder == IMG_ORDER_A)
      {
        be.EmitMov(result, regs_dest.elements(3));
      }
      else
      {
        be.EmitMov(result, regs_dest.elements(0));
      }
    }

    be.Brigantine().addLabel(s_label_exit);
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
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
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
              case IMG_1D:
              case IMG_1DA:
                return Value(MV_UINT32, 0xBB810204);
              case IMG_2D:
              case IMG_2DA:
                return Value(MV_UINT32, 0xBB010204);
              case IMG_3D:
                return Value(MV_UINT32, 0xBA810204);
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
              case IMG_1D:
              case IMG_1DA:
                return Value(MV_UINT32, 0xB7800100);
              case IMG_2D:
              case IMG_2DA:
                return Value(MV_UINT32, 0xB7000100);
              case IMG_3D:
                return Value(MV_UINT32, 0xB7000100);
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
              case IMG_1D:
              case IMG_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case IMG_2D:
              case IMG_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case IMG_3D:
                return Value(MV_UINT32, 0x3E000000);
              }
              return Value(MV_UINT32, 0x3E800000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       if (samplerFilter == BRIG_FILTER_LINEAR)
        {
            if (samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER)
            {
              switch (imageGeometryProp)
              {
              case IMG_1D:
              case IMG_1DA:
                return Value(MV_UINT32, 0x3F000000);
              case IMG_2D:
              case IMG_2DA:
                return Value(MV_UINT32, 0x3E800000);
              case IMG_3D:
                return Value(MV_UINT32, 0x3E000080);
              }
              return Value(MV_UINT32, 0x3F000000);
            }
        }
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
    }
    assert(0);
    return  Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    }

    return (codeLocation != FUNCTION);
  }

};

class ImageRdTestR:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestR(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    }

    return (codeLocation != FUNCTION);
  }

};


class ImageRdTestRX:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestRX(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRG:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestRG(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRGX:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestRGX(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRA:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestRA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRGB:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestRGB(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
      return true;
    default:
      return false;
    }
    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRGBX:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestRGBX(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestRGBA:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestRGBA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestBGRA:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestBGRA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_INT8:
    case IMG_SNORM_INT8:
    case IMG_SIGNED_INT8:
    case IMG_UNSIGNED_INT8:
      return true;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestARGB:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestARGB(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_INT8:
    case IMG_SNORM_INT8:
    case IMG_SIGNED_INT8:
    case IMG_UNSIGNED_INT8:
      return true;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};


class ImageRdTestABGR:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestABGR(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_INT8:
    case IMG_SNORM_INT8:
    case IMG_SIGNED_INT8:
    case IMG_UNSIGNED_INT8:
      return true;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};


class ImageRdTestSRGB:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestSRGB(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};


class ImageRdTestSRGBX:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestSRGBX(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestSRGBA:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestSRGBA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestSBGRA:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestSBGRA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_SHORT_555:
    case IMG_UNORM_SHORT_565:
    case IMG_UNORM_SHORT_101010:
    case IMG_UNORM_INT24:
      return false;
    default:
      return true;
    }

    return ImageRdTestBase::IsValid();
  }

};


class ImageRdTestINTENSITY:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestINTENSITY(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_INT8:
    case IMG_UNORM_INT16:
    case IMG_SNORM_INT8:
    case IMG_SNORM_INT16:
    case IMG_HALF_FLOAT:
    case IMG_FLOAT:
      return true;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestLUMINANCE:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestLUMINANCE(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_INT8:
    case IMG_UNORM_INT16:
    case IMG_SNORM_INT8:
    case IMG_SNORM_INT16:
    case IMG_HALF_FLOAT:
    case IMG_FLOAT:
      return true;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestDEPTH:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestDEPTH(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_INT16:
    case IMG_UNORM_INT24:
    case IMG_FLOAT:
      return true;
    default:
      return false;
    }

    return ImageRdTestBase::IsValid();
  }

};

class ImageRdTestDEPTHSTENCIL:  public ImageRdTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageRdTestDEPTHSTENCIL(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, 
      BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_): imageChannelType(imageChannelType_),
      ImageRdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BrigImageChannelOrder(IMG_ORDER_A), imageChannelType_, samplerCoord_, samplerFilter_, samplerAddressing_)
  {
  }

  Value ExpectedResult() const {
    switch (ChannelComponentLen())
    {
    case 1:
      return Value(MV_UINT32, 0xFF);
    case 2:
      return Value(MV_UINT32, 0xFFFF);
    case 4:
      return Value(MV_UINT32, 0xFFFFFFFF);
    }
    assert(0);
    return Value(MV_UINT32, 0xFF);
  }

  bool IsValid() const
  {
    switch (imageChannelType)
    {
    case IMG_UNORM_INT16:
    case IMG_UNORM_INT24:
    case IMG_FLOAT:
      return true;
    default:
      return false;
    }

    return (codeLocation != FUNCTION);
  }

};

void ImageRdTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageRdTestA>(ap, it, "image_rd_a/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());
  /*
  TestForEach<ImageRdTestR>(ap, it, "image_rd_r/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRX>(ap, it, "image_rd_rx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRG>(ap, it, "image_rd_rg/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRGX>(ap, it, "image_rd_rgx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRA>(ap, it, "image_rd_ra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRGB>(ap, it, "image_rd_rgb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRGBX>(ap, it, "image_rd_rgbx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestRGBA>(ap, it, "image_rd_rgba/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestBGRA>(ap, it, "image_rd_bgra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestARGB>(ap, it, "image_rd_argb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestABGR>(ap, it, "image_rd_abgr/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestSRGB>(ap, it, "image_rd_srgb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestSRGBX>(ap, it, "image_rd_srgbx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestSRGBA>(ap, it, "image_rd_srgba/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestSBGRA>(ap, it, "image_rd_sbgra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestINTENSITY>(ap, it, "image_rd_intensity/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestLUMINANCE>(ap, it, "image_rd_luminance/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageRdGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestDEPTH>(ap, it, "image_rd_depth/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());

  TestForEach<ImageRdTestDEPTHSTENCIL>(ap, it, "image_rd_depth_stencil/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(),
    cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings());
*/
}

} // hsail_conformance
