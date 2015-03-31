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

  TypedReg CreateCoordList(uint32_t x, uint32_t y, uint32_t z, uint32_t a) {
    TypedReg treg;
    switch (imageGeometry) {
    case BRIG_GEOMETRY_1D: 
    case BRIG_GEOMETRY_1DB:
      treg = be.AddInitialTReg(BRIG_TYPE_U32, x);
      break;
    case BRIG_GEOMETRY_1DA:
      treg = be.AddTReg(BRIG_TYPE_U32, 2);
      be.EmitMov(treg->Reg(0), be.Immed(BRIG_TYPE_U32, x), 32);
      be.EmitMov(treg->Reg(1), be.Immed(BRIG_TYPE_U32, a), 32);
      break;
    case BRIG_GEOMETRY_2D:
    case BRIG_GEOMETRY_2DDEPTH:
      treg = be.AddTReg(BRIG_TYPE_U32, 2);
      be.EmitMov(treg->Reg(0), be.Immed(BRIG_TYPE_U32, x), 32);
      be.EmitMov(treg->Reg(1), be.Immed(BRIG_TYPE_U32, y), 32);
      break;
    case BRIG_GEOMETRY_3D:
      treg = be.AddTReg(BRIG_TYPE_U32, 3);
      be.EmitMov(treg->Reg(0), be.Immed(BRIG_TYPE_U32, x), 32);
      be.EmitMov(treg->Reg(1), be.Immed(BRIG_TYPE_U32, y), 32);
      be.EmitMov(treg->Reg(2), be.Immed(BRIG_TYPE_U32, z), 32);
      break;
    case BRIG_GEOMETRY_2DA:
    case BRIG_GEOMETRY_2DADEPTH:
      treg = be.AddTReg(BRIG_TYPE_U32, 3);
      be.EmitMov(treg->Reg(0), be.Immed(BRIG_TYPE_U32, x), 32);
      be.EmitMov(treg->Reg(1), be.Immed(BRIG_TYPE_U32, y), 32);
      be.EmitMov(treg->Reg(2), be.Immed(BRIG_TYPE_U32, a), 32);
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

  static const uint32_t INITIAL_VALUE = 0xFF;

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
    imageSpec.LimitTest(true);
    image = kernel->NewImage("image", &imageSpec);
    image->AddData(image->GenMemValue(Value(MV_UINT32, INITIAL_VALUE)));
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
    
    // read from first element
    image->EmitImageLd(imageElement, imageAddr, firstCoord);

    // read from last element
    image->EmitImageLd(imageElement, imageAddr, lastCoord);

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


class ImageHandlesNumber : public ImageLimitTest {
protected:
  std::vector<Image> images;
  Variable imagesBuffer;

  uint32_t InitialValue() const { return 123456789; }
  
  virtual uint32_t Limit() const = 0;

public:
  ImageHandlesNumber(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_)
    : ImageLimitTest(gridGeometry_, imageGeometry_, channelOrder_, channelType_) {}

  void Init() override {
    Test::Init();
    imagesBuffer = te->NewVariable("images_buffer", BRIG_SEGMENT_KERNARG, EPointerReg::GetSegmentPointerType(BRIG_SEGMENT_GLOBAL, te->CoreCfg()->IsLarge()));
    images.reserve(Limit());
  }

  void KernelArguments() override {
    ImageLimitTest::KernelArguments();
    imagesBuffer->KernelArguments();
  }

  void KernelVariables() override {
    // do nothing
  }

  void SetupDispatch(const std::string& dispatchId) override {
    ImageLimitTest::SetupDispatch(dispatchId);
    /*
    unsigned count = setup->MSetup().Count();
    // allocate memory for buffer with image handles
    uint32_t sizes[] = {(uint32_t)images.size(), 1, 1};
    auto buffer = new MBuffer(count++, "images_buffer", MEM_GLOBAL, MV_IMAGEREF, 1, sizes);
    setup->MSetup().Add(buffer);

    // allocate memory for kernel argument with reference to that buffer
    setup->MSetup().Add(NewMValue(count++, "images_buffer.kernarg", MEM_KERNARG, MV_REF, U64(buffer->Id())));

    // allocate memory for all images
    for (uint32_t i = 0; i < images.size(); ++i) {

      auto image = images[i];
      auto mimage = new MImage(count++, image->Id(), image->Segment(), image->Geometry(), 
                               image->ChannelOrder(), image->ChannelType(), image->Type(), 
                               image->Width(), image->Height(), image->Depth(), image->ArraySize());
      setup->MSetup().Add(mimage);
      mimage->ContentData() = *image->ReleaseData();
      Value value = mimage->ContentData()[0];
      mimage->VType() = value.Type();
      buffer->Data().push_back(Value(MV_IMAGEREF, mimage->Id()));
    }
      */
  }
};

class ROImageHandlesNumber : public ImageHandlesNumber {
private:
  static const uint32_t LIMIT = 128;

protected:
  uint32_t Limit() const override { return LIMIT; }

public:
  ROImageHandlesNumber(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_)
    : ImageHandlesNumber(gridGeometry_, imageGeometry_, channelOrder_, channelType_) {}

  void Init() override {
    ImageHandlesNumber::Init();

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
      image = kernel->NewImage("image" + std::to_string(i), &imageSpec);
      image->AddData(image->GenMemValue(Value(MV_UINT32, InitialValue())));
      images.push_back(image);
    }
  }

  TypedReg Result() override {
    auto trueLabel = "@true";
    auto falseLabel = "@false";
    auto endLabel = "@end";

    // load address of images buffer
    auto imagesAddr = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    imagesBuffer->EmitLoadTo(imagesAddr);
    
    auto imageAddr = be.AddTReg(images[0]->Type());
    auto coord = CreateCoordList(0, 0, 0, 0);
    auto imageElement = IsImageDepth(ImageGeometry()) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);
    auto query = be.AddTReg(BRIG_TYPE_U32);
    auto cmp = be.AddCTReg();

    for (uint32_t i = 0; i < images.size(); ++i) {
      auto image = images[i];
      be.EmitLoad(image->Segment(), imageAddr->Type(), imageAddr->Reg(), be.Address(imagesAddr)); 

      // check image query for each image 
      image->EmitImageQuery(query, imageAddr, BRIG_IMAGE_QUERY_WIDTH);
      be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), 1), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);

      // load from each image
      image->EmitImageLd(imageElement, imageAddr, coord);
      be.EmitArith(BRIG_OPCODE_ADD, imagesAddr, imagesAddr, be.Immed(imagesAddr->Type(), getBrigTypeNumBytes(image->Type())));
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

protected:
  uint32_t Limit() const override { return LIMIT; }

public:
  RWImageHandlesNumber(Grid gridGeometry_, BrigImageGeometry imageGeometry_, BrigImageChannelOrder channelOrder_, BrigImageChannelType channelType_, uint32_t numberRW_)
    : ImageHandlesNumber(gridGeometry_, imageGeometry_, channelOrder_, channelType_), numberRW(numberRW_) {}

  bool IsValid() const override {
    return ImageHandlesNumber::IsValid() && numberRW <= 64;
  }

  void Name(std::ostream& out) const override {
    ImageHandlesNumber::Name(out);
    out << "/" << "rw" << numberRW << "_wo" << (Limit() - numberRW);
  }

  void Init() override {
    ImageHandlesNumber::Init();

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
      image = kernel->NewImage("rw_image" + std::to_string(i), &rwImageSpec);
      image->AddData(image->GenMemValue(Value(MV_UINT32, InitialValue())));
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
      image = kernel->NewImage("wo_image" + std::to_string(i), &woImageSpec);
      image->AddData(image->GenMemValue(Value(MV_UINT32, InitialValue())));
      images.push_back(image);
    }
  }

  TypedReg Result() override {
    auto trueLabel = "@true";
    auto falseLabel = "@false";
    auto endLabel = "@end";

    // load address of images buffer
    auto imagesAddr = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    imagesBuffer->EmitLoadTo(imagesAddr);
    
    auto imageAddr = be.AddTReg(images[0]->Type());
    auto coord = CreateCoordList(0, 0, 0, 0);
    auto imageElement = IsImageDepth(ImageGeometry()) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);
    auto query = be.AddTReg(BRIG_TYPE_U32);
    auto cmp = be.AddCTReg();

    for (uint32_t i = 0; i < images.size(); ++i) {
      auto image = images[i];
      be.EmitLoad(image->Segment(), imageAddr->Type(), imageAddr->Reg(), be.Address(imagesAddr)); 

      // check image query for each image 
      image->EmitImageQuery(query, imageAddr, BRIG_IMAGE_QUERY_WIDTH);
      be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), 1), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);

      // store in each image
      image->EmitImageSt(imageElement, imageAddr, coord);

      // if image is rw also load from it
      if (i < numberRW) {
        image->EmitImageLd(imageElement, imageAddr, coord);
        be.EmitArith(BRIG_OPCODE_ADD, imagesAddr, imagesAddr, be.Immed(imagesAddr->Type(), getBrigTypeNumBytes(image->Type())));
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
  BrigSamplerAddressing addresing;
  BrigSamplerCoordNormalization coord;
  BrigSamplerFilter filter;
  Value red;

  static const uint32_t LIMIT = 16;
  static const uint32_t INITIAL_VALUE = 123456;
  static const BrigType ELEMENT_TYPE = BRIG_TYPE_F32;

public:
  SamplerHandlesNumber(Grid geometry, BrigSamplerAddressing addresing_, BrigSamplerCoordNormalization coord_, BrigSamplerFilter filter_)
    : Test(Location::KERNEL, geometry), addresing(addresing_), coord(coord_), filter(filter_) {}

  void Name(std::ostream& out) const override {
    out << samplerAddressing2str(addresing) << "_" 
        << samplerCoordNormalization2str(coord) << "_" 
        << samplerFilter2str(filter);
  }

  bool IsValid() const override {
    if (filter == BRIG_FILTER_LINEAR) // only f32 access type is supported for linear filter
    {
      //currently rd test will generate coordinate 0, which will sample out of range texel
      //for linear filtering. We should not test it, because it implementation defined.
      //TODO:
      //Change rd test with linear filter and undefined addressing
      //to not read from edge of an image
      if(addresing == BRIG_ADDRESSING_UNDEFINED)
        return false;
    }
    if (coord == BRIG_COORD_UNNORMALIZED && 
       (addresing == BRIG_ADDRESSING_REPEAT || addresing == BRIG_ADDRESSING_MIRRORED_REPEAT)) {
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
    image = kernel->NewImage("image", &imageSpec);
    image->AddData(image->GenMemValue(Value(static_cast<float>(INITIAL_VALUE))));

    samplers.reserve(LIMIT);
    ESamplerSpec samplerSpec(BRIG_SEGMENT_GLOBAL, Location::KERNEL);
    samplerSpec.CoordNormalization(coord);
    samplerSpec.Filter(filter);
    samplerSpec.Addresing(addresing);
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
    auto coord = be.AddInitialTReg(BRIG_TYPE_U32, 0);
    
    auto imageElement = be.AddTReg(BRIG_TYPE_F32, 4);

    auto samplerAddr = be.AddTReg(samplers[0]->Type());
    auto query = be.AddTReg(BRIG_TYPE_U32);
    auto cmp = be.AddCTReg();

    for (uint32_t i = 0; i < samplers.size(); ++i) {
      auto sampler = samplers[i];
      be.EmitLoad(sampler->Segment(), samplerAddr->Type(), samplerAddr->Reg(), be.Address(sampler->Variable())); 

      // check query sampler filter mode
      sampler->EmitSamplerQuery(query, samplerAddr, BRIG_SAMPLER_QUERY_FILTER);
      be.EmitCmp(cmp->Reg(), query, be.Immed(query->Type(), filter), BRIG_COMPARE_NE);
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
  
  TestForEach<SamplerHandlesNumber>(ap, it, "limits/sampler_number", cc->Grids().TrivialGeometrySet(), cc->Sampler().SamplerAddressings(), cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters());
}

} // hsail_conformance
