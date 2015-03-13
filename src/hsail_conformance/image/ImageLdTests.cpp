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

#include "ImageLdTests.hpp"
#include "RuntimeContext.hpp"
#include "HCTests.hpp"
#include "MObject.hpp"

using namespace hexl::emitter;
using namespace hexl::scenario;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageLdTestBase:  public Test {
private:
  Image imgobj;

  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestBase(Location codeLocation, 
      Grid geometry, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): Test(codeLocation, geometry), 
      imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_)
  {
    imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2), Array_);
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '\\' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType));
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
   imgobj = kernel->NewImage("%roimage", &imageSpec);
   imgobj->AddData(Value(MV_UINT8, 0xFF));

  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  bool IsValid() const  {
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      if ((imageGeometry.ImageHeight() > 1) || (imageGeometry.ImageDepth() > 1) || (imageGeometry.ImageArray() > 1))
        return false;
      break;
    case BRIG_GEOMETRY_1DA:
      if ((imageGeometry.ImageHeight() > 1) || (imageGeometry.ImageDepth() > 1) || (imageGeometry.ImageArray() < 2))
        return false;
      break;
     case BRIG_GEOMETRY_2D:
     case BRIG_GEOMETRY_2DDEPTH:
      if ((imageGeometry.ImageHeight() < 2) || (imageGeometry.ImageDepth() > 1) || (imageGeometry.ImageArray() > 1))
        return false;
      break;
    case BRIG_GEOMETRY_2DA:
      if ((imageGeometry.ImageHeight() < 2) || (imageGeometry.ImageDepth() > 1) || (imageGeometry.ImageArray() < 2))
        return false;
      break;
    case BRIG_GEOMETRY_2DADEPTH:
      if (imageGeometry.ImageDepth() > 1)
        return false;
      break;
    case BRIG_GEOMETRY_3D:
      if ((imageGeometry.ImageHeight() < 2) || (imageGeometry.ImageDepth() < 2) || (imageGeometry.ImageArray() > 1))
        return false;
      break;
    default:
      if (imageGeometry.ImageArray() > 1)
        return false;
    }
    return (codeLocation != FUNCTION);
  }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    return Value(MV_UINT32, 255);
  }

  size_t OutputBufferSize() const override {
    return imageGeometry.ImageSize()*4;
  }

  
  TypedReg Get1dCoord()
  {
    auto result = be.AddTReg(BRIG_TYPE_U32);
    auto x = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result, x->Reg());
    return result;
  }

  OperandOperandList Get2dCoord()
  {
    auto result = be.AddVec(BRIG_TYPE_U32, 2);
    auto x = be.EmitWorkitemAbsId(1, false);
    auto y = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result.elements(0), x->Reg(), 32);
    be.EmitMov(result.elements(1), y->Reg(), 32);
    return result;
  }

  OperandOperandList Get3dCoord()
  {
    auto result = be.AddVec(BRIG_TYPE_U32, 3);
    auto x = be.EmitWorkitemAbsId(2, false);
    auto y = be.EmitWorkitemAbsId(1, false);
    auto z = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result.elements(0), x->Reg(), 32);
    be.EmitMov(result.elements(1), y->Reg(), 32);
    be.EmitMov(result.elements(2), z->Reg(), 32);
    return result;
  }

  TypedReg Result() {
    auto result = be.AddTReg(ResultType());
    be.EmitMov(result, be.Immed(ResultType(), 0));
   // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 

    OperandOperandList regs_dest;
    auto reg_dest = be.AddTReg(BRIG_TYPE_U32, 1);
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, Get1dCoord());
      break;
    case BRIG_GEOMETRY_1DA:
    case BRIG_GEOMETRY_2D:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, Get2dCoord(), BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_2DDEPTH:
      imgobj->EmitImageLd(reg_dest, imageaddr, Get2dCoord(), BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_3D:
    case BRIG_GEOMETRY_2DA:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, Get3dCoord(), BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_2DADEPTH:
      imgobj->EmitImageLd(reg_dest, imageaddr, Get3dCoord(), BRIG_TYPE_U32);
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
   // be.Brigantine().addLabel(s_label_exit);
    return result;
  }
};


class ImageLdTestA:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestA(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_A, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestR:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageLdTestR(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_R, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};


class ImageLdTestRX:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  


public:
  ImageLdTestRX(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RX, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestRG:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestRG(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RG, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestRGX:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestRGX(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGX, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestRA:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestRA(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RA, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestRGB:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestRGB(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGB, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
        return Value(MV_UINT32, 0x3F800000);    //TODO ?? (ret 0)
      case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
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
    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestRGBX:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestRGBX(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBX, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
       return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
       return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestRGBA:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageLdTestRGBA(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBA, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestBGRA:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestBGRA(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_BGRA, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestARGB:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestARGB(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ARGB, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
       return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
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

    return ImageLdTestBase::IsValid();
  }

};


class ImageLdTestABGR:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestABGR(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ABGR, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};


class ImageLdTestSRGB:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageLdTestSRGB(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGB, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }


};


class ImageLdTestSRGBX:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageLdTestSRGBX(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBX, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);

      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
       return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestSRGBA:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageLdTestSRGBA(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBA, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestSBGRA:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageLdTestSBGRA(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SBGRA, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};


class ImageLdTestINTENSITY:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageLdTestINTENSITY(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_INTENSITY, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestLUMINANCE:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageLdTestLUMINANCE(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_LUMINANCE, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
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
        return Value(MV_UINT32, 0xBC010204);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0xB8000100);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0x3F800000);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0xFFC00000);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0xFFFFFFFF);
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestDEPTH:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestDEPTH(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_INT24:
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

    return ImageLdTestBase::IsValid();
  }

};

class ImageLdTestDEPTHSTENCIL:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestDEPTHSTENCIL(Location codeLocation_, 
      Grid geometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH_STENCIL, imageChannelType_, Array_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_INT24:
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

    return ImageLdTestBase::IsValid();
  }

};

void ImageLdTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageLdTestA>(ap, it, "image_ld_a/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestR>(ap, it, "image_ld_r/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestRX>(ap, it, "image_ld_rx/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestRG>(ap, it, "image_ld_rg/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestRGX>(ap, it, "image_ld_rgx/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestRA>(ap, it, "image_ld_ra/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestRGB>(ap, it, "image_ld_rgb/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestRGBX>(ap, it, "image_ld_rgbx/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestRGBA>(ap, it, "image_ld_rgba/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestBGRA>(ap, it, "image_ld_bgra/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestARGB>(ap, it, "image_ld_argb/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestABGR>(ap, it, "image_ld_abgr/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestSRGB>(ap, it, "image_ld_srgb/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestSRGBX>(ap, it, "image_ld_srgbx/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestSRGBA>(ap, it, "image_ld_srgba/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestSBGRA>(ap, it, "image_ld_sbgra/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestINTENSITY>(ap, it, "image_ld_intensity/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  TestForEach<ImageLdTestLUMINANCE>(ap, it, "image_ld_luminance/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestDEPTH>(ap, it, "image_ld_depth/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());

  //TestForEach<ImageLdTestDEPTHSTENCIL>(ap, it, "image_ld_depth_stencil/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());
}

} // hsail_conformance
