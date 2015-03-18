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
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageStTest:  public Test {
private:
  Image imgobj;

  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;

public:
  ImageStTest(Location codeLocation, 
      Grid geometry, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): Test(codeLocation, geometry), 
      imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_)
  {
    imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2), Array_);
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '/' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelOrderString(MObjectImageChannelOrder(imageChannelOrder)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType));
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
   imgobj->InitMemValue(Value(MV_UINT32, 0x00000032));

   imgobj->InitImageCalculator(NULL, Value(MV_UINT32, 0x80808080));
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

   bool IsValid() const  {
    return IsImageSupported(imageGeometryProp, imageChannelOrder, imageChannelType) && IsImageGeometrySupported(imageGeometryProp, imageGeometry) && (codeLocation != FUNCTION);
  }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    Value color[4];
    Value coords[3];
    coords[0] = Value(0.0f);
    coords[1] = Value(0.0f);
    coords[2] = Value(0.0f);

    imgobj->ReadColor(coords, color);

    return (imageChannelOrder == BRIG_CHANNEL_ORDER_A) ? Value(MV_UINT32, color[3].U8()) : Value(MV_UINT32, color[0].U8());
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

void ImageStTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageStTest>(ap, it, "image_st/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageSupportedChannelOrders(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());
}

} // hsail_conformance
