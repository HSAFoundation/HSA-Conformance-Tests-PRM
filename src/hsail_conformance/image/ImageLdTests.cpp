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
using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageLdTestBase:  public Test {
private:
  Variable nx;
  Variable ny;
  Image imgobj;

  ImageGeometry* imageGeometry;
  BrigImageGeometry ImageGeometryProps;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestBase(Location codeLocation, 
      Grid geometry, ImageGeometry* imageGeometry_, BrigImageGeometry ImageGeometryProps_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_): Test(codeLocation, geometry), 
      imageGeometry(imageGeometry_), ImageGeometryProps(ImageGeometryProps_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '\\' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(ImageGeometryProps)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType));
  }
  
  void Init() {
   Test::Init();

   imgobj = kernel->NewImage("%roimage", BRIG_SEGMENT_KERNARG, ImageGeometryProps, imageChannelOrder, imageChannelType, BRIG_ACCESS_PERMISSION_RO, imageGeometry->ImageSize(0),imageGeometry->ImageSize(1),imageGeometry->ImageSize(2),imageGeometry->ImageSize(3),imageGeometry->ImageSize(4));
   
   nx = kernel->NewVariable("nx", BRIG_SEGMENT_KERNARG, BRIG_TYPE_U32);
   nx->PushBack(Value(MV_UINT32, 1000));
   ny = kernel->NewVariable("ny", BRIG_SEGMENT_KERNARG, BRIG_TYPE_U32);
   ny->PushBack(Value(MV_UINT32, 1));
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
    return Value(MV_UINT32, 255);
  }

  size_t OutputBufferSize() const override {
    return 1000;
  }

  TypedReg Result() {
    auto x = be.EmitWorkitemId(0);
    auto y = be.EmitWorkitemId(1);
    auto nxReg = nx->AddDataReg();
    be.EmitLoad(nx->Segment(), nxReg->Type(), nxReg->Reg(), be.Address(nx->Variable())); 
    auto nyReg = ny->AddDataReg();
    be.EmitLoad(ny->Segment(), nyReg->Type(), nyReg->Reg(), be.Address(ny->Variable())); 
    auto result = be.AddTReg(ResultType());
    be.EmitMov(result, be.Immed(ResultType(), 0));
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

    OperandOperandList regs_dest;
    auto reg_dest = be.AddTReg(BRIG_TYPE_U32, 1);
    OperandOperandList coords;
    auto coord = be.AddTReg(BRIG_TYPE_U32, 1);
    switch (ImageGeometryProps)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      be.EmitMov(coord, be.Immed(BRIG_TYPE_U32, 0));
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, coord);
      break;
    case BRIG_GEOMETRY_1DA:
    case BRIG_GEOMETRY_2D:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      coords = be.AddVec(BRIG_TYPE_U32, 2);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_U32, 0), 32);
      }
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, coords, BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_2DDEPTH:
      coords = be.AddVec(BRIG_TYPE_U32, 2);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_U32, 0), 32);
      }
      imgobj->EmitImageLd(reg_dest, imageaddr, coords, BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_3D:
    case BRIG_GEOMETRY_2DA:
      regs_dest = be.AddVec(BRIG_TYPE_U32, 4);
      coords = be.AddVec(BRIG_TYPE_U32, 3);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_U32, 0), 32);
      }
      imgobj->EmitImageLd(regs_dest, BRIG_TYPE_U32,  imageaddr, coords, BRIG_TYPE_U32);
      break;
    case BRIG_GEOMETRY_2DADEPTH:
      coords = be.AddVec(BRIG_TYPE_U32, 3);
      for (unsigned i = 0; i < coords.elementCount(); i++)
      {
        be.EmitMov(coords.elements(i), be.Immed(BRIG_TYPE_U32, 0), 32);
      }
      imgobj->EmitImageLd(reg_dest, imageaddr, coords, BRIG_TYPE_U32);
      break;
    default:
      assert(0);
    }

    if ((ImageGeometryProps == BRIG_GEOMETRY_2DDEPTH) || (ImageGeometryProps == BRIG_GEOMETRY_2DADEPTH)) {
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
    be.Brigantine().addLabel(s_label_exit);
    return result;
  }
};

class ImageLdTestA:  public ImageLdTestBase {
private:
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelType imageChannelType;

public:
  ImageLdTestA(Location codeLocation_, 
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_A, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_R, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RX, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RG, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGX, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RA, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGB, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBX, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBA, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_BGRA, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ARGB, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ABGR, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGB, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBX, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBA, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SBGRA, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_INTENSITY, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_LUMINANCE, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH, imageChannelType_), 
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
      Grid geometry_, ImageGeometry* imageGeometry_, BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_): 
      ImageLdTestBase(codeLocation_, geometry_, imageGeometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH_STENCIL, imageChannelType_), 
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
  TestForEach<ImageLdTestA>(ap, it, "image_ld_a/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestR>(ap, it, "image_ld_r/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestRX>(ap, it, "image_ld_rx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestRG>(ap, it, "image_ld_rg/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestRGX>(ap, it, "image_ld_rgx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestRA>(ap, it, "image_ld_ra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestRGB>(ap, it, "image_ld_rgb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestRGBX>(ap, it, "image_ld_rgbx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestRGBA>(ap, it, "image_ld_rgba/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestBGRA>(ap, it, "image_ld_bgra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestARGB>(ap, it, "image_ld_argb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestABGR>(ap, it, "image_ld_abgr/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestSRGB>(ap, it, "image_ld_srgb/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestSRGBX>(ap, it, "image_ld_srgbx/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestSRGBA>(ap, it, "image_ld_srgba/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestSBGRA>(ap, it, "image_ld_sbgra/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestINTENSITY>(ap, it, "image_ld_intensity/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  TestForEach<ImageLdTestLUMINANCE>(ap, it, "image_ld_luminance/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestDEPTH>(ap, it, "image_ld_depth/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes());

  //TestForEach<ImageLdTestDEPTHSTENCIL>(ap, it, "image_ld_depth_stencil/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Images().DefaultImageGeometrySet(), cc->Images().ImageDepthGeometryProp(), cc->Images().ImageChannelTypes());
}

} // hsail_conformance
