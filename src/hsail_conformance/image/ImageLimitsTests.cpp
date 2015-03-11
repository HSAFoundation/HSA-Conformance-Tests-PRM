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

#include "ImageLimitsTests.hpp"
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

class ImageSizeLimitTest : public Test {
private:
  BrigImageGeometry imageGeometry; 
  BrigImageChannelOrder channelOrder; 
  BrigImageChannelType channelType;
  Image image;

  static const uint32_t INITIAL_VALUE = 0xFFFFFFFF;

  uint32_t LimitWidth() const {
    switch (imageGeometry) {
    case BRIG_GEOMETRY_1D:        return 16384;
    case BRIG_GEOMETRY_1DA:       return 1;
    case BRIG_GEOMETRY_1DB:       return 65536;
    case BRIG_GEOMETRY_2D:        return 16384;
    case BRIG_GEOMETRY_2DA:       return 1;
    case BRIG_GEOMETRY_2DDEPTH:   return 16384;
    case BRIG_GEOMETRY_2DADEPTH:  return 1;
    case BRIG_GEOMETRY_3D:        return 2048;
    default: assert(false); return 0;
    }
  }

  uint32_t LimitHeight() const {
    switch (imageGeometry) {
    case BRIG_GEOMETRY_1D:        return 0;
    case BRIG_GEOMETRY_1DA:       return 0;
    case BRIG_GEOMETRY_1DB:       return 0;
    case BRIG_GEOMETRY_2D:        return 16384;
    case BRIG_GEOMETRY_2DA:       return 1;
    case BRIG_GEOMETRY_2DDEPTH:   return 16384;
    case BRIG_GEOMETRY_2DADEPTH:  return 1;
    case BRIG_GEOMETRY_3D:        return 2048;
    default: assert(false); return 0;
    }
  }

  uint32_t LimitDepth() const {
    switch (imageGeometry) {
    case BRIG_GEOMETRY_3D: 
      return 2048;
    case BRIG_GEOMETRY_1D:  case BRIG_GEOMETRY_2D:    
    case BRIG_GEOMETRY_1DA: case BRIG_GEOMETRY_2DA:
    case BRIG_GEOMETRY_1DB: case BRIG_GEOMETRY_2DDEPTH:
    case BRIG_GEOMETRY_2DADEPTH:
      return 0;
    default: assert(false); return 0;
    }
  }
  
  uint32_t LimitArraySize() const {
    switch (imageGeometry) {
    case BRIG_GEOMETRY_1DA: case BRIG_GEOMETRY_2DA:
    case BRIG_GEOMETRY_2DADEPTH:
      return 2048;
    case BRIG_GEOMETRY_3D: case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_2D: case BRIG_GEOMETRY_1DB:
    case BRIG_GEOMETRY_2DDEPTH:
      return 0;
    default: assert(false); return 0;
    }
  }

  OperandOperandList CreateCoordList(uint32_t x, uint32_t y, uint32_t z, uint32_t a) {
    ItemList coordList;
    switch (imageGeometry) {
    case BRIG_GEOMETRY_1D: 
    case BRIG_GEOMETRY_1DB:
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, x)->Reg());
      break;
    case BRIG_GEOMETRY_1DA:
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, x)->Reg());
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, a)->Reg());
      break;
    case BRIG_GEOMETRY_2D:
    case BRIG_GEOMETRY_2DDEPTH:
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, x)->Reg());
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, y)->Reg());
      break;
    case BRIG_GEOMETRY_3D:
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, x)->Reg());
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, y)->Reg());
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, z)->Reg());
      break;
    case BRIG_GEOMETRY_2DA:
    case BRIG_GEOMETRY_2DADEPTH:
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, x)->Reg());
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, y)->Reg());
      coordList.push_back(be.AddInitialTReg(BRIG_TYPE_U32, a)->Reg());
      break;
    default:
      assert(0);
    }
    return be.Brigantine().createOperandList(coordList);
  }

public:
  ImageSizeLimitTest(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_)
    : Test(Location::KERNEL, gridGeometry_), imageGeometry(imageGeometry_), channelOrder(channelOrder_), channelType(channelType_) {}

  void Name(std::ostream& out) const override {
    out << imageGeometry2str(imageGeometry) << "/"
        << imageChannelOrder2str(channelOrder) << "_"
        << imageChannelType2str(channelType);
  }

  bool IsValid() const override {
    return IsImageSupported(imageGeometry, channelOrder, channelType);
  }

  void Init() override {
    Test::Init();
    EImageSpec imageSpec(BRIG_SEGMENT_KERNARG, BRIG_TYPE_ROIMG, Location::KERNEL);
    imageSpec.Geometry(imageGeometry);
    imageSpec.ChannelOrder(channelOrder);
    imageSpec.ChannelType(channelType);
    imageSpec.Width(std::max<uint32_t>(LimitWidth(), 1));
    imageSpec.Height(std::max<uint32_t>(LimitHeight(), 1));
    imageSpec.Depth(std::max<uint32_t>(LimitDepth(), 1));
    imageSpec.ArraySize(std::max<uint32_t>(LimitArraySize(), 1));
    image = kernel->NewImage("image", &imageSpec);
    image->AddData(Value(MV_UINT8, INITIAL_VALUE)); 
    image->LimitEnable(true);
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  TypedReg Result() override {
    auto trueLabel = "@true";
    auto falseLabel = "@false";
    auto endLabel = "@end";
    
    auto firstCoord = CreateCoordList(0, 0, 0, 0);
    auto lastCoord = CreateCoordList(
      LimitWidth() == 0 ? 0 : LimitWidth() - 1, 
      LimitHeight() == 0 ? 0 : LimitHeight() - 1,
      LimitDepth() == 0 ? 0 : LimitDepth() - 1,
      LimitArraySize() == 0 ? 0 : LimitArraySize() - 1);
    auto imageElement = IsImageDepth(imageGeometry) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);
    auto imageElementList = be.Brigantine().createOperandList(imageElement->Regs());

    auto imageAddr = be.AddTReg(image->Type());
    be.EmitLoad(image->Segment(), imageAddr->Type(), imageAddr->Reg(), be.Address(image->Variable())); 
    
    // read from first element
    image->EmitImageLd(imageElementList, BRIG_TYPE_U32, imageAddr, firstCoord, BRIG_TYPE_U32);
    auto cmp = be.AddCTReg();
    for (size_t i = 0; i < imageElement->Count(); ++i) {
      be.EmitCmp(cmp->Reg(), imageElement->Type(), imageElement->Reg(i), be.Immed(imageElement->Type(), INITIAL_VALUE), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);
    }
    
    // read from last element
    image->EmitImageLd(imageElementList, BRIG_TYPE_U32, imageAddr, lastCoord, BRIG_TYPE_U32);
    for (size_t i = 0; i < imageElement->Count(); ++i) {
      be.EmitCmp(cmp->Reg(), imageElement->Type(), imageElement->Reg(i), be.Immed(imageElement->Type(), INITIAL_VALUE), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);
    }

    // true
    be.EmitLabel(trueLabel);
    auto result = be.AddInitialTReg(ResultType(), 1);
    be.EmitBr(endLabel);
    // false
    be.EmitLabel(falseLabel);
    be.EmitMov(result, be.Immed(result->Type(), 0));
    //end
    be.EmitLabel(endLabel);
    return result;
  }
};


void ImageLimitsTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();

  //auto geometry = new(ap) OneValueSequence<BrigImageGeometry>(BRIG_GEOMETRY_3D);
  //auto channelOrder = new(ap) OneValueSequence<BrigImageChannelOrder>(BRIG_CHANNEL_ORDER_A);
  //auto channelType = new(ap) OneValueSequence<BrigImageChannelType>(BRIG_CHANNEL_TYPE_UNSIGNED_INT8);
  //TestForEach<ImageSizeLimitTest>(ap, it, "limits/size", cc->Grids().TrivialGeometrySet(), geometry, channelOrder, channelType);

  TestForEach<ImageSizeLimitTest>(ap, it, "limits/size", cc->Grids().TrivialGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageSupportedChannelOrders(), cc->Images().ImageChannelTypes());
}

} // hsail_conformance
