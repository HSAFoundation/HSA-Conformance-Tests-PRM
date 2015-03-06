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

#include "ImageStTests.hpp"
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

class ImageStTestBase:  public Test {
private:
  Image imgobj;

  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestBase(Location codeLocation, 
      Grid geometry, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_): Test(codeLocation, geometry), 
      imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_)
  {
    imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2));
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '\\' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType));
  }
  

  void Init() {
   Test::Init();
   EImageSpec imageSpec(BRIG_SEGMENT_KERNARG, BRIG_TYPE_RWIMG);
   imageSpec.Geometry(imageGeometryProp);
   imageSpec.ChannelOrder(imageChannelOrder);
   imageSpec.ChannelType(imageChannelType);
   imageSpec.Width(imageGeometry.ImageWidth());
   imageSpec.Height(imageGeometry.ImageHeight());
   imageSpec.Depth(imageGeometry.ImageDepth());
   imageSpec.ArraySize(imageGeometry.ImageArray());
   imgobj = kernel->NewImage("%rwimage", &imageSpec);
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

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    return Value(MV_UINT32, 0x80);
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

  unsigned char GetReplacedVal() {
    return 0x80;
  }

  OperandOperandList StoreValues(int cnt) {
    auto result = be.AddVec(BRIG_TYPE_U32, cnt);
    for (int i = 0; i < cnt; i++)
      be.EmitMov(result.elements(i), be.Immed(ResultType(), GetReplacedVal()), 32);
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
    be.EmitMov(result, be.Immed(ResultType(), GetReplacedVal()));
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      imgobj->EmitImageSt(StoreValues(4), BRIG_TYPE_U32, imageaddr, Get1dCoord());
     // be.EmitBarrier();
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, Get1dCoord());
      break;
    case BRIG_GEOMETRY_1DA:
    case BRIG_GEOMETRY_2D:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      imgobj->EmitImageSt(StoreValues(4), BRIG_TYPE_U32, imageaddr, Get2dCoord(), BRIG_TYPE_U32);
    //  be.EmitBarrier();
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, Get2dCoord(), BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_2DDEPTH:
      imgobj->EmitImageSt(reg_dest, imageaddr, Get2dCoord(), BRIG_TYPE_U32);
   //   be.EmitBarrier();
      imgobj->EmitImageLd(reg_dest, imageaddr, Get2dCoord(), BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_3D:
    case BRIG_GEOMETRY_2DA:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      imgobj->EmitImageSt(StoreValues(4), BRIG_TYPE_U32, imageaddr, Get3dCoord(), BRIG_TYPE_U32);
    //  be.EmitBarrier();
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, Get3dCoord(), BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_2DADEPTH:
      imgobj->EmitImageSt(reg_dest, imageaddr, Get3dCoord(), BRIG_TYPE_U32);
    //  be.EmitBarrier();
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
    return result;
  }
};

class ImageStTestA:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_A, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestR:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageStTestR(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_R, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};


class ImageStTestRX:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  


public:
  ImageStTestRX(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RX, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestRG:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestRG(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RG, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestRGX:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestRGX(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGX, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestRA:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestRA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RA, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestRGB:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestRGB(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGB, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
        return Value(MV_UINT32, 0x0);    //TODO ?? (ret 0)
      case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
        return Value(MV_UINT32, 0x0);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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
    return ImageStTestBase::IsValid();
  }

};

class ImageStTestRGBX:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestRGBX(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBX, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestRGBA:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageStTestRGBA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBA, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestBGRA:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestBGRA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_BGRA, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x0);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestARGB:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestARGB(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ARGB, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
       return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0x0);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};


class ImageStTestABGR:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestABGR(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ABGR, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};


class ImageStTestSRGB:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageStTestSRGB(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGB, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }


};


class ImageStTestSRGBX:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageStTestSRGBX(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBX, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestSRGBA:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageStTestSRGBA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBA, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestSBGRA:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageStTestSBGRA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SBGRA, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};


class ImageStTestINTENSITY:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageStTestINTENSITY(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_INTENSITY, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestLUMINANCE:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;
  
public:
  ImageStTestLUMINANCE(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_LUMINANCE, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:
        return Value(MV_UINT32, 0x7F);
      case BRIG_CHANNEL_TYPE_SIGNED_INT16:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SIGNED_INT32:
      case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_SNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_SNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
        return Value(MV_UINT32, 0);
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:
        return Value(MV_UINT32, 0x0);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x80);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestDEPTH:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestDEPTH(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_INT24:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x00);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

class ImageStTestDEPTHSTENCIL:  public ImageStTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageStTestDEPTHSTENCIL(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageStTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH_STENCIL, imageChannelType_), 
      imageGeometryProp(imageGeometryProp_), imageChannelType(imageChannelType_)
  {
  }

  Value ExpectedResult() const {
    switch (imageChannelType)
    {
      case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_INT24:
        return Value(MV_UINT32, 0x80);
      case BRIG_CHANNEL_TYPE_FLOAT:
        return Value(MV_UINT32, 0x0);
      default:
        break;
    }
    assert(0);
    return  Value(MV_UINT32, 0x80);
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

    return ImageStTestBase::IsValid();
  }

};

void ImageStTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageStTestA>(ap, it, "image_st_a/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestR>(ap, it, "image_st_r/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestRX>(ap, it, "image_st_rx/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestRG>(ap, it, "image_st_rg/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestRGX>(ap, it, "image_st_rgx/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestRA>(ap, it, "image_st_ra/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestRGB>(ap, it, "image_st_rgb/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestRGBX>(ap, it, "image_st_rgbx/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestRGBA>(ap, it, "image_st_rgba/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestBGRA>(ap, it, "image_st_bgra/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestARGB>(ap, it, "image_st_argb/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestABGR>(ap, it, "image_st_abgr/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestSRGB>(ap, it, "image_st_srgb/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestSRGBX>(ap, it, "image_st_srgbx/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestSRGBA>(ap, it, "image_st_srgba/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestSBGRA>(ap, it, "image_st_sbgra/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestINTENSITY>(ap, it, "image_st_intensity/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageStTestLUMINANCE>(ap, it, "image_st_luminance/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestDEPTH>(ap, it, "image_st_depth/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageStTestDEPTHSTENCIL>(ap, it, "image_st_depth_stencil/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes());
}

} // hsail_conformance
