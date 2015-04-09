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
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageLimitTest: public Test {
private:
  BrigImageGeometry imageGeometry; 
  BrigImageChannelOrder channelOrder; 
  BrigImageChannelType channelType;

protected:
  BrigImageGeometry ImageGeometry() const { return imageGeometry; }
  BrigImageChannelOrder ChannelOrder() const { return channelOrder; }
  BrigImageChannelType ChannelType() const { return channelType; }

  BrigType CoordType() { return BRIG_TYPE_U32; }

  TypedReg CreateCoordList(uint32_t x, uint32_t y, uint32_t z, uint32_t a) {
    TypedReg treg;
    switch (imageGeometry) {
    case BRIG_GEOMETRY_1D: 
    case BRIG_GEOMETRY_1DB:
      treg = be.AddInitialTReg(CoordType(), x);
      break;
    case BRIG_GEOMETRY_1DA:
      treg = be.AddTReg(CoordType(), 2);
      be.EmitMov(treg->Reg(0), be.Immed(CoordType(), x), 32);
      be.EmitMov(treg->Reg(1), be.Immed(CoordType(), a), 32);
      break;
    case BRIG_GEOMETRY_2D:
    case BRIG_GEOMETRY_2DDEPTH:
      treg = be.AddTReg(CoordType(), 2);
      be.EmitMov(treg->Reg(0), be.Immed(CoordType(), x), 32);
      be.EmitMov(treg->Reg(1), be.Immed(CoordType(), y), 32);
      break;
    case BRIG_GEOMETRY_3D:
      treg = be.AddTReg(CoordType(), 3);
      be.EmitMov(treg->Reg(0), be.Immed(CoordType(), x), 32);
      be.EmitMov(treg->Reg(1), be.Immed(CoordType(), y), 32);
      be.EmitMov(treg->Reg(2), be.Immed(CoordType(), z), 32);
      break;
    case BRIG_GEOMETRY_2DA:
    case BRIG_GEOMETRY_2DADEPTH:
      treg = be.AddTReg(CoordType(), 3);
      be.EmitMov(treg->Reg(0), be.Immed(CoordType(), x), 32);
      be.EmitMov(treg->Reg(1), be.Immed(CoordType(), y), 32);
      be.EmitMov(treg->Reg(2), be.Immed(CoordType(), a), 32);
      break;
    default:
      assert(0);
    }
    return treg;
  }

public:
  ImageLimitTest(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_)
    : Test(Location::KERNEL, gridGeometry_), imageGeometry(imageGeometry_), channelOrder(channelOrder_), channelType(channelType_) {}

  void Name(std::ostream& out) const override {
    out << imageGeometry2str(imageGeometry) << "/"
        << imageChannelOrder2str(channelOrder) << "_"
        << imageChannelType2str(channelType);
  }

  bool IsValid() const override {
    return IsImageLegal(imageGeometry, channelOrder, channelType);
  }

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }
};

class ImageSizeLimitTest : public ImageLimitTest {
private:
  Image image;

  static const uint32_t INITIAL_VALUE = 123;

  uint32_t LimitWidth() const {
    switch (ImageGeometry()) {
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
    switch (ImageGeometry()) {
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
    switch (ImageGeometry()) {
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
    switch (ImageGeometry()) {
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

public:
  ImageSizeLimitTest(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_)
    : ImageLimitTest(gridGeometry_, imageGeometry_, channelOrder_, channelType_) {}

  void Init() override {
    Test::Init();
    EImageSpec imageSpec(BRIG_SEGMENT_KERNARG, BRIG_TYPE_ROIMG, Location::KERNEL);
    imageSpec.Geometry(ImageGeometry());
    imageSpec.ChannelOrder(ChannelOrder());
    imageSpec.ChannelType(ChannelType());
    imageSpec.Width(std::max<uint32_t>(LimitWidth(), 1));
    imageSpec.Height(std::max<uint32_t>(LimitHeight(), 1));
    imageSpec.Depth(std::max<uint32_t>(LimitDepth(), 1));
    imageSpec.ArraySize(std::max<uint32_t>(LimitArraySize(), 1));
    image = kernel->NewImage("image", HOST_INPUT_IMAGE, &imageSpec);
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
    auto imageElement = IsImageDepth(ImageGeometry()) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);

    auto imageAddr = be.AddTReg(image->Type());
    be.EmitLoad(image->Segment(), imageAddr->Type(), imageAddr->Reg(), be.Address(image->Variable())); 
    
    // query image size and check it
    auto query = be.AddTReg(BRIG_TYPE_U32);
    auto cmp = be.AddCTReg();
    image->EmitImageQuery(query, imageAddr, BRIG_IMAGE_QUERY_WIDTH);
    be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), LimitWidth()), BRIG_COMPARE_NE);
    be.EmitCbr(cmp->Reg(), falseLabel);
    if (ImageGeometryDims(ImageGeometry()) >= 2) { // check height only for 2d and greater geometries
      image->EmitImageQuery(query, imageAddr, BRIG_IMAGE_QUERY_HEIGHT);
      be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), LimitHeight()), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);
    }
    if (ImageGeometryDims(ImageGeometry()) >= 3) { // check height only for 3d geometry
      image->EmitImageQuery(query, imageAddr, BRIG_IMAGE_QUERY_DEPTH);
      be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), LimitDepth()), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);
    }
    if (IsImageGeometryArray(ImageGeometry())) { // check array size only for array images
      image->EmitImageQuery(query, imageAddr, BRIG_IMAGE_QUERY_ARRAY);
      be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), LimitArraySize()), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);
    }

    // read from first element and compare it with INITIAL_VALUE
    image->EmitImageLd(imageElement, imageAddr, firstCoord);
    be.EmitCmp(cmp->Reg(), imageElement->Type(), imageElement->Reg(3), 
               be.Immed(imageElement->Type(), INITIAL_VALUE), BRIG_COMPARE_NE);
    be.EmitCbr(cmp->Reg(), falseLabel);

    // read from last element
    image->EmitImageLd(imageElement, imageAddr, lastCoord);
    be.EmitCmp(cmp->Reg(), imageElement->Type(), imageElement->Reg(3), 
               be.Immed(imageElement->Type(), INITIAL_VALUE), BRIG_COMPARE_NE);
    be.EmitCbr(cmp->Reg(), falseLabel);

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

  void ScenarioInit() override {
    ImageLimitTest::ScenarioInit();
    auto data = std::unique_ptr<Values>(new Values(1, Value(MV_UINT32, INITIAL_VALUE)));
    ImageRegion region;
    image->ScenarioImageWrite(std::move(data), region);
    region.x = (uint32_t)image->Width() - 1;
    region.y = (uint32_t)image->Height() - 1;
    if (ImageGeometry() == BRIG_GEOMETRY_1DA) { // for 1da geometry we use y coordinate as array coordinate
      region.y = (uint32_t)image->ArraySize() - 1;
    }
    region.z = (uint32_t)image->Depth() - 1;
    if (ImageGeometry() == BRIG_GEOMETRY_2DA || ImageGeometry() == BRIG_GEOMETRY_2DADEPTH) { 
      // for 2da and 2dadepth geometry we use z coordinate as array coordinate
      region.z = (uint32_t)image->ArraySize() - 1;
    }
    data = std::unique_ptr<Values>(new Values(1, Value(MV_UINT32, INITIAL_VALUE)));
    image->ScenarioImageWrite(std::move(data), region);
  }
};


class ImageHandlesNumber : public ImageLimitTest {
protected:
  std::vector<Image> images;
  Buffer imagesBuffer;

  uint32_t InitialValue() const { return 123456789; }
  
  virtual uint32_t Limit() const = 0;
  virtual void InitImages() = 0;

public:
  ImageHandlesNumber(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_)
    : ImageLimitTest(gridGeometry_, imageGeometry_, channelOrder_, channelType_) {}

  void Init() override {
    Test::Init();
    InitImages();

    imagesBuffer = kernel->NewBuffer("images_buffer", HOST_INPUT_BUFFER, MV_UINT64, Limit());
    for (auto image: images) {
      imagesBuffer->AddData(Value(MV_STRING, Str(new std::string(image->IdHandle()))));
    }
  }
};

class ROImageHandlesNumber : public ImageHandlesNumber {
private:
  static const uint32_t LIMIT = 128;

protected:
  uint32_t Limit() const override { return LIMIT; }

  void InitImages() override {
    EImageSpec imageSpec(BRIG_SEGMENT_GLOBAL, BRIG_TYPE_ROIMG);
    imageSpec.Geometry(ImageGeometry());
    imageSpec.ChannelOrder(ChannelOrder());
    imageSpec.ChannelType(ChannelType());
    imageSpec.Width(1);
    imageSpec.Height(1);
    imageSpec.Depth(1);
    imageSpec.ArraySize(1);
    Image image;
    for (uint32_t i = 0; i < LIMIT; ++i) {
      image = kernel->NewImage("image" + std::to_string(i), HOST_IMAGE, &imageSpec);
      image->SetInitialData(image->GenMemValue(Value(MV_UINT32, InitialValue())));
      images.push_back(image);
    }
  }

public:
  ROImageHandlesNumber(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_)
    : ImageHandlesNumber(gridGeometry_, imageGeometry_, channelOrder_, channelType_) {}

  TypedReg Result() override {
    auto trueLabel = "@true";
    auto falseLabel = "@false";
    auto endLabel = "@end";

    auto imageAddr = be.AddTReg(images[0]->Type());
    auto indexReg = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    auto coord = CreateCoordList(0, 0, 0, 0);
    auto imageElement = IsImageDepth(ImageGeometry()) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);
    auto query = be.AddTReg(BRIG_TYPE_U32);
    auto cmp = be.AddCTReg();

    for (uint32_t i = 0; i < images.size(); ++i) {
      auto image = images[i];
      be.EmitMov(indexReg, be.Immed(indexReg->Type(), i));
      imagesBuffer->EmitLoadData(imageAddr, indexReg);

      // check image query for each image 
      image->EmitImageQuery(query, imageAddr, BRIG_IMAGE_QUERY_WIDTH);

      be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), 1), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);

      // load from each image
      image->EmitImageLd(imageElement, imageAddr, coord);
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

class RWImageHandlesNumber : public ImageHandlesNumber {
private:
  uint32_t numberRW;

  static const uint32_t LIMIT = 64;
  static const uint32_t STORE_VALUE = 987654321;

protected:
  uint32_t Limit() const override { return LIMIT; }

  void InitImages() override {
    // rw images
    EImageSpec rwImageSpec(BRIG_SEGMENT_GLOBAL, BRIG_TYPE_RWIMG);
    rwImageSpec.Geometry(ImageGeometry());
    rwImageSpec.ChannelOrder(ChannelOrder());
    rwImageSpec.ChannelType(ChannelType());
    rwImageSpec.Width(1);
    rwImageSpec.Height(1);
    rwImageSpec.Depth(1);
    rwImageSpec.ArraySize(1);
    Image image;
    for (uint32_t i = 0; i < numberRW; ++i) {
      image = kernel->NewImage("rw_image" + std::to_string(i), HOST_IMAGE, &rwImageSpec);
      image->SetInitialData(image->GenMemValue(Value(MV_UINT32, InitialValue())));
      images.push_back(image);
    }

    // wo images
    EImageSpec woImageSpec(BRIG_SEGMENT_GLOBAL, BRIG_TYPE_WOIMG);
    woImageSpec.Geometry(ImageGeometry());
    woImageSpec.ChannelOrder(ChannelOrder());
    woImageSpec.ChannelType(ChannelType());
    woImageSpec.Width(1);
    woImageSpec.Height(1);
    woImageSpec.Depth(1);
    woImageSpec.ArraySize(1);
    for (uint32_t i = 0; i < Limit() - numberRW; ++i) {
      image = kernel->NewImage("wo_image" + std::to_string(i), HOST_IMAGE, &woImageSpec);
      image->SetInitialData(image->GenMemValue(Value(MV_UINT32, InitialValue())));
      images.push_back(image);
    }
  }

public:
  RWImageHandlesNumber(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_, uint32_t numberRW_)
    : ImageHandlesNumber(gridGeometry_, imageGeometry_, channelOrder_, channelType_), numberRW(numberRW_) {}

  bool IsValid() const override {
    return ImageHandlesNumber::IsValid() && numberRW <= LIMIT;
  }

  void Name(std::ostream& out) const override {
    ImageHandlesNumber::Name(out);
    out << "/" << "rw" << numberRW << "_wo" << (Limit() - numberRW);
  }

  TypedReg Result() override {
    auto trueLabel = "@true";
    auto falseLabel = "@false";
    auto endLabel = "@end";

    auto rwImageAddr = be.AddTReg(BRIG_TYPE_RWIMG);
    auto woImageAddr = be.AddTReg(BRIG_TYPE_WOIMG);
    auto indexReg = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    auto coord = CreateCoordList(0, 0, 0, 0);
    auto imageElement = IsImageDepth(ImageGeometry()) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);
    auto query = be.AddTReg(BRIG_TYPE_U32);
    auto cmp = be.AddCTReg();

    for (uint32_t i = 0; i < images.size(); ++i) {
      auto image = images[i];
      be.EmitMov(indexReg, be.Immed(indexReg->Type(), i));

      if (i < numberRW) {   // rw images
        imagesBuffer->EmitLoadData(rwImageAddr, indexReg);
        // check image query
        image->EmitImageQuery(query, rwImageAddr, BRIG_IMAGE_QUERY_WIDTH);
        be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), 1), BRIG_COMPARE_NE);
        be.EmitCbr(cmp->Reg(), falseLabel);
        // store in image
        for (uint32_t j = 0; j < imageElement->Count(); ++j) {
          be.EmitMov(imageElement->Reg(j), be.Immed(imageElement->Type(), STORE_VALUE), imageElement->TypeSizeBits());
        }
        image->EmitImageSt(imageElement, rwImageAddr, coord);
        be.EmitImageFence();
        image->EmitImageLd(imageElement, rwImageAddr, coord);
      } else { // wo images
        imagesBuffer->EmitLoadData(woImageAddr, indexReg);
        // check image query
        image->EmitImageQuery(query, woImageAddr, BRIG_IMAGE_QUERY_WIDTH);
        be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), 1), BRIG_COMPARE_NE);
        be.EmitCbr(cmp->Reg(), falseLabel);
        // store in image
        image->EmitImageSt(imageElement, woImageAddr, coord);
      }
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

class SamplerHandlesNumber : public Test {
private:
  std::vector<Sampler> samplers;
  Image image;
  SamplerParams samplerParams;
  Value red;

  static const uint32_t LIMIT = 16;
  static const uint32_t INITIAL_VALUE = 123456;
  static const BrigType ELEMENT_TYPE = BRIG_TYPE_F32;

public:
  SamplerHandlesNumber(Grid geometry, SamplerParams* samplerParams_)
    : Test(Location::KERNEL, geometry), samplerParams(*samplerParams_) { }

  void Name(std::ostream& out) const override {
    out << samplerParams;
  }

  bool IsValid() const override {
    if (!samplerParams.IsValid()) { return false; }
    if (samplerParams.Filter() == BRIG_FILTER_LINEAR) // only f32 access type is supported for linear filter
    {
      //currently rd test will generate coordinate 0, which will sample out of range texel
      //for linear filtering. We should not test it, because it implementation defined.
      //TODO:
      //Change rd test with linear filter and undefined addressing
      //to not read from edge of an image
      if(samplerParams.Addressing() == BRIG_ADDRESSING_UNDEFINED)
        return false;
    }
    return Test::IsValid();
  }

  void Init() override {
    Test::Init();

    // image to test sampler
    EImageSpec imageSpec(BRIG_SEGMENT_KERNARG, BRIG_TYPE_ROIMG);
    imageSpec.Geometry(BRIG_GEOMETRY_1D);
    imageSpec.ChannelOrder(BRIG_CHANNEL_ORDER_R);
    imageSpec.ChannelType(BRIG_CHANNEL_TYPE_FLOAT);
    imageSpec.Width(1);
    imageSpec.Height(1);
    imageSpec.Depth(1);
    imageSpec.ArraySize(1);
    image = kernel->NewImage("image", HOST_INPUT_IMAGE, &imageSpec);
    image->SetInitialData(image->GenMemValue(Value(static_cast<float>(INITIAL_VALUE))));

    samplers.reserve(LIMIT);
    ESamplerSpec samplerSpec(BRIG_SEGMENT_GLOBAL, Location::KERNEL);
    samplerSpec.Params(samplerParams);
    for (uint32_t i = 0; i < LIMIT; ++i) {
      samplers.push_back(kernel->NewSampler("sampler" + std::to_string(i), &samplerSpec));
    }

    image->InitImageCalculator(samplers[0]);
    Value readColor[4];
    Value readCoords[3];
    readCoords[0] = Value(0.0F);
    readCoords[1] = Value(0.0F);
    readCoords[2] = Value(0.0F);
    image->ReadColor(readCoords, readColor);
    red = readColor[0];
  }

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  TypedReg Result() override {
    auto trueLabel = "@true";
    auto falseLabel = "@false";
    auto endLabel = "@end";

    // load image handle
    auto imageAddr = be.AddTReg(image->Type());
    be.EmitLoad(image->Segment(), imageAddr->Type(), imageAddr->Reg(), be.Address(image->Variable())); 

    // coordinates where to read from image
    auto coord = be.AddTReg(BRIG_TYPE_F32);
    be.EmitMov(coord, be.Immed(BRIG_TYPE_U32, 0));
    
    auto imageElement = be.AddTReg(BRIG_TYPE_F32, 4);

    auto samplerAddr = be.AddTReg(samplers[0]->Variable().type());
    auto query = be.AddTReg(BRIG_TYPE_U32);
    auto cmp = be.AddCTReg();

    for (uint32_t i = 0; i < samplers.size(); ++i) {
      auto sampler = samplers[i];
      be.EmitLoad(sampler->Segment(), samplerAddr->Type(), samplerAddr->Reg(), be.Address(sampler->Variable())); 

      // check query sampler filter mode
      sampler->EmitSamplerQuery(query, samplerAddr, BRIG_SAMPLER_QUERY_FILTER);
      be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), samplerParams.Filter()), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);

      // read from image with sampler and compare R channel with INITIAL_VALUE
      image->EmitImageRd(imageElement, imageAddr, samplerAddr, coord);
      be.EmitCmp(cmp->Reg(), imageElement->Type(), imageElement->Reg(0), be.Immed(red.F()), BRIG_COMPARE_NE); 
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

  auto channelOrder = new(ap) OneValueSequence<BrigImageChannelOrder>(BRIG_CHANNEL_ORDER_A);
  auto channelType = new(ap) OneValueSequence<BrigImageChannelType>(BRIG_CHANNEL_TYPE_UNSIGNED_INT8);
  TestForEach<ImageSizeLimitTest>(ap, it, "limits/size", cc->Grids().TrivialGeometrySet(), cc->Images().ImageGeometryProps(), channelOrder, channelType);

  TestForEach<ROImageHandlesNumber>(ap, it, "limits/ro_number", cc->Grids().TrivialGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageSupportedChannelOrders(), cc->Images().ImageChannelTypes());
  TestForEach<RWImageHandlesNumber>(ap, it, "limits/rw_number", cc->Grids().TrivialGeometrySet(), cc->Images().ImageGeometryProps(), cc->Images().ImageSupportedChannelOrders(), cc->Images().ImageChannelTypes(), cc->Images().NumberOfRwImageHandles());
  
  TestForEach<SamplerHandlesNumber>(ap, it, "limits/sampler_number", cc->Grids().TrivialGeometrySet(), cc->Samplers().All());
}

} // hsail_conformance
