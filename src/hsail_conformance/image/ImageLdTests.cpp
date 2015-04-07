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

class ImageLdTest:  public Test {
private:
  Image imgobj;

  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;
  Value color[4];

public:
  ImageLdTest(Location codeLocation, 
      Grid geometry, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): Test(codeLocation, geometry), 
      imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_)
  {
    imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2), Array_);
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '/' << imageGeometry << "_" << ImageGeometryString(MObjectImageGeometry(imageGeometryProp)) << "_" << ImageChannelOrderString(MObjectImageChannelOrder(imageChannelOrder)) << "_" << ImageChannelTypeString(MObjectImageChannelType(imageChannelType));
  }
  
  uint32_t InitialValue() const { return 0x30313233; }

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
    imgobj = kernel->NewImage("%roimage", HOST_INPUT_IMAGE, &imageSpec);
    imgobj->SetInitialData(imgobj->GenMemValue(Value(MV_UINT32, InitialValue())));
   
    imgobj->InitImageCalculator(NULL);

    Value coords[3];
    coords[0] = Value(MV_UINT32, 0);
    coords[1] = Value(MV_UINT32, 0);
    coords[2] = Value(MV_UINT32, 0);
    imgobj->LoadColor(coords, color);

    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
      output->SetComparisonMethod("2ulp,minf=-1.0,maxf=1.0"); //1.5ulp [-1.0; 1.0]
      break;
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
      output->SetComparisonMethod("2ulp,minf=0.0,maxf=1.0"); //1.5ulp [0.0; 1.0]
      break;
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
      //integer types are compared for equality
      break;
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
      output->SetComparisonMethod("0ulp"); //f16 denorms should not be flushed (as it will produce normalized f32)
      break;
    case BRIG_CHANNEL_TYPE_FLOAT:
      output->SetComparisonMethod("0ulp,flushDenorms"); //flushDenorms
      break;
    default:
      break;
    }
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  bool IsValid() const  {
    return IsImageLegal(imageGeometryProp, imageChannelOrder, imageChannelType) && IsImageGeometrySupported(imageGeometryProp, imageGeometry) && (codeLocation != FUNCTION);
  }

  BrigType ResultType() const { return ImageAccessType(imageChannelType); }

  void ExpectedResults(Values* result) const
  {
    uint16_t channels = IsImageDepth(imageGeometryProp) ? 1 : 4;
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++)
          for (unsigned i = 0; i < channels; i++)
            result->push_back(color[i]);
  }

  uint64_t ResultDim() const override {
    return IsImageDepth(imageGeometryProp) ? 1 : 4;
  }

  TypedReg Get1dCoord()
  {
    auto result = be.AddTReg(BRIG_TYPE_U32);
    auto x = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result, x->Reg());
    return result;
  }

  TypedReg Get2dCoord()
  {
    auto result = be.AddTReg(BRIG_TYPE_U32, 2);
    auto x = be.EmitWorkitemAbsId(1, false);
    auto y = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result->Reg(0), x->Reg(), 32);
    be.EmitMov(result->Reg(1), y->Reg(), 32);
    return result;
  }

  TypedReg Get3dCoord()
  {
    auto result = be.AddTReg(BRIG_TYPE_U32, 3);
    auto x = be.EmitWorkitemAbsId(2, false);
    auto y = be.EmitWorkitemAbsId(1, false);
    auto z = be.EmitWorkitemAbsId(0, false);
    be.EmitMov(result->Reg(0), x->Reg(), 32);
    be.EmitMov(result->Reg(1), y->Reg(), 32);
    be.EmitMov(result->Reg(2), z->Reg(), 32);
    return result;
  }

  TypedReg Result() {
   // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 

    auto regs_dest = IsImageDepth(imageGeometryProp) ? be.AddTReg(BRIG_TYPE_U32) : be.AddTReg(BRIG_TYPE_U32, 4);
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      imgobj->EmitImageLd(regs_dest, imageaddr, Get1dCoord());
      break;
    case BRIG_GEOMETRY_1DA:
    case BRIG_GEOMETRY_2D:
      imgobj->EmitImageLd(regs_dest, imageaddr, Get2dCoord());
      break;
    case BRIG_GEOMETRY_2DDEPTH:
      imgobj->EmitImageLd(regs_dest, imageaddr, Get2dCoord());
      break;
    case BRIG_GEOMETRY_3D:
    case BRIG_GEOMETRY_2DA:
      imgobj->EmitImageLd(regs_dest, imageaddr, Get3dCoord());
      break;
    case BRIG_GEOMETRY_2DADEPTH:
      imgobj->EmitImageLd(regs_dest, imageaddr, Get3dCoord());
      break;
    default:
      assert(0);
    }
    return regs_dest;
  }
};

void ImageLdTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageLdTest>(ap, it, "image_ld/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageSupportedChannelOrders(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());
}

} // hsail_conformance
