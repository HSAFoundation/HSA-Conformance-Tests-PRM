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
  BrigSamplerCoordNormalization samplerCoord;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;
  BrigSamplerQuery samplerQuery;

public:
  ImageQuerySamplerTest(Location codeLocation, Grid geometry, 
    BrigSamplerCoordNormalization samplerCoord_, BrigSamplerFilter samplerFilter_, BrigSamplerAddressing samplerAddressing_, BrigSamplerQuery samplerQuery_): Test(codeLocation, geometry),
    samplerCoord(samplerCoord_), samplerFilter(samplerFilter_), samplerAddressing(samplerAddressing_), samplerQuery(samplerQuery_)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << "_" <<
      SamplerCoordsString(MObjectSamplerCoords(samplerCoord)) << "_" << SamplerFilterString(MObjectSamplerFilter(samplerFilter)) << "_" << SamplerAddressingString(MObjectSamplerAddressing(samplerAddressing)) << "_" << SamplerQueryString(MObjectSamplerQuery(samplerQuery));
  }

  void Init() {
   Test::Init();
   smpobj = kernel->NewSampler("%sampler", BRIG_SEGMENT_KERNARG, samplerCoord, samplerFilter, samplerAddressing);
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

    return (codeLocation != FUNCTION);
  }

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    switch (samplerQuery)
    {
    case BRIG_SAMPLER_QUERY_ADDRESSING:
      return Value(MV_UINT32, samplerAddressing);
    case BRIG_SAMPLER_QUERY_COORD:
      return Value(MV_UINT32, samplerCoord);
    case BRIG_SAMPLER_QUERY_FILTER:
      return Value(MV_UINT32, samplerFilter);
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

class ImageQueryTestBase:  public Test {
private:
  Image imgobj;
  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;
  BrigImageQuery imageQuery;

public:
  ImageQueryTestBase(Location codeLocation, Grid geometry, 
    BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): Test(codeLocation, geometry),
    imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_), imageQuery(imageQuery_)
  {
    imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2));
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '\\' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelOrderString(MObjectImageChannelOrder(imageChannelOrder)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType)) << "_" <<
      ImageQueryString(MObjectImageQuery(imageQuery));
  }

  void Init() {
   Test::Init();
   imgobj = kernel->NewImage("%roimage", BRIG_SEGMENT_KERNARG, imageGeometryProp, imageChannelOrder, imageChannelType, BRIG_TYPE_ROIMG, imageGeometry.ImageWidth(),imageGeometry.ImageHeight(),imageGeometry.ImageDepth(),imageGeometry.ImageArray());
   for (unsigned i = 0; i < imageGeometry.ImageSize(); ++i) { imgobj->AddData(Value(MV_UINT8, 0xFF)); }
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

    //check query type and geometry equal
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DA:
       if ((imageQuery == BRIG_IMAGE_QUERY_HEIGHT) || (imageQuery == BRIG_IMAGE_QUERY_DEPTH) || (imageQuery == BRIG_IMAGE_QUERY_ARRAY))
        return false;
       else break;
    case BRIG_GEOMETRY_1DB:
     return false; // ??

    case BRIG_GEOMETRY_2D:
    case BRIG_GEOMETRY_2DA:
      if  ((imageQuery == BRIG_IMAGE_QUERY_DEPTH) || (imageQuery == BRIG_IMAGE_QUERY_ARRAY))
        return false;
      else break;
    case BRIG_GEOMETRY_3D:
      if  (imageQuery == BRIG_IMAGE_QUERY_ARRAY)
        return false;
      else break;
    default:
      break;
    }

    return (codeLocation != FUNCTION);
  }

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

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

class ImageQueryTestA:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_A, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestR:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestR(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_R, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};



class ImageQueryTestRX:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestRX(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RX, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestRG:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestRG(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RG, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestRGX:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestRGX(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGX, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestRA:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestRA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RA, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestRGB:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestRGB(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGB, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestRGBX:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestRGBX(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBX, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestRGBA:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestRGBA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_RGBA, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestBGRA:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestBGRA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_BGRA, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestARGB:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestARGB(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ARGB, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestABGR:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestABGR(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_ABGR, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestSRGB:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestSRGB(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGB, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};


class ImageQueryTestSRGBX:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestSRGBX(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBX, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestSRGBA:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestSRGBA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SRGBA, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestSBGRA:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestSBGRA(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_SBGRA, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestINTENSITY:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestINTENSITY(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_INTENSITY, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestLUMINANCE:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestLUMINANCE(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_LUMINANCE, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestDEPTH:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestDEPTH(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};

class ImageQueryTestDEPTHSTENCIL:  public ImageQueryTestBase {
private:
  BrigImageChannelType imageChannelType;

public:
  ImageQueryTestDEPTHSTENCIL(Location codeLocation_, 
      Grid geometry_,  BrigImageGeometry imageGeometryProp_, BrigImageChannelType imageChannelType_, BrigImageQuery imageQuery_): 
      ImageQueryTestBase(codeLocation_, geometry_, imageGeometryProp_, BRIG_CHANNEL_ORDER_DEPTH_STENCIL, imageChannelType_, imageQuery_), 
      imageChannelType(imageChannelType_)
  {
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

    return ImageQueryTestBase::IsValid();
  }

};


void ImageQueryTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageQueryTestA>(ap, it, "image_query_a/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestR>(ap, it, "image_query_r/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  //TestForEach<ImageQueryTestRX>(ap, it, "image_query_rx/basic", CodeLocations(), cc->Grids().ImagesSet(),
  //  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestRA>(ap, it, "image_query_ra/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestRG>(ap, it, "image_query_rg/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestRGB>(ap, it, "image_query_rgb/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  //TestForEach<ImageQueryTestRGBX>(ap, it, "image_query_rgbx/basic", CodeLocations(), cc->Grids().ImagesSet(),
  //  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestRGBA>(ap, it, "image_query_rgba/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestBGRA>(ap, it, "image_query_bgra/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestARGB>(ap, it, "image_query_argb/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestABGR>(ap, it, "image_query_abgr/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

 // TestForEach<ImageQueryTestSRGB>(ap, it, "image_query_srgb/basic", CodeLocations(), cc->Grids().ImagesSet(),
 //   cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

 // TestForEach<ImageQueryTestSRGBX>(ap, it, "image_query_srgbx/basic", CodeLocations(), cc->Grids().ImagesSet(),
 //   cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

 // TestForEach<ImageQueryTestSRGBA>(ap, it, "image_query_srgba/basic", CodeLocations(), cc->Grids().ImagesSet(),
 //   cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

 // TestForEach<ImageQueryTestSBGRA>(ap, it, "image_query_sbgra/basic", CodeLocations(), cc->Grids().ImagesSet(),
 //   cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQueryTestINTENSITY>(ap, it, "image_query_intensity/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());
   
  TestForEach<ImageQueryTestLUMINANCE>(ap, it, "image_query_luminance/basic", CodeLocations(), cc->Grids().ImagesSet(),
    cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

 // TestForEach<ImageQueryTestDEPTH>(ap, it, "image_query_depth/basic", CodeLocations(), cc->Grids().ImagesSet(),
 //   cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

 // TestForEach<ImageQueryTestDEPTHSTENCIL>(ap, it, "image_query_depth_stencil/basic", CodeLocations(), cc->Grids().ImagesSet(),
 //   cc->Images().ImageGeometryProps(), cc->Images().ImageChannelTypes(), cc->Images().ImageQueryTypes());

  TestForEach<ImageQuerySamplerTest>(ap, it, "image_query_sampler/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings(), cc->Sampler().SamplerQueryTypes());
}

} // hsail_conformance
