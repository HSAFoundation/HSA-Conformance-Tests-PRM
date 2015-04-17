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
  BrigType coordType;

public:
  ImageLdTest(Location codeLocation, 
      Grid geometry, BrigImageGeometry imageGeometryProp_, BrigImageChannelOrder imageChannelOrder_, BrigImageChannelType imageChannelType_, unsigned Array_ = 1): Test(codeLocation, geometry), 
      imageGeometryProp(imageGeometryProp_), imageChannelOrder(imageChannelOrder_), imageChannelType(imageChannelType_)
  {
    imageGeometry = ImageGeometry(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2), Array_);
    coordType = BRIG_TYPE_U32;
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
    imgobj = kernel->NewImage("%roimage", HOST_INPUT_IMAGE, &imageSpec, IsImageOptional(imageGeometryProp, imageChannelOrder, imageChannelType, BRIG_TYPE_ROIMG));
    imgobj->SetInitialData(imgobj->GenMemValue(Value(MV_UINT32, InitialValue())));
   
    imgobj->InitImageCalculator(NULL);

    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
      output->SetComparisonMethod("ulps=2,minf=-1.0,maxf=1.0"); //1.5ulp [-1.0; 1.0]
      break;
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
      switch (imageChannelOrder)
      {
      case BRIG_CHANNEL_ORDER_SRGB:
      case BRIG_CHANNEL_ORDER_SRGBX:
      case BRIG_CHANNEL_ORDER_SRGBA:
      case BRIG_CHANNEL_ORDER_SBGRA:
        output->SetComparisonMethod("ulps=3,minf=0.0,maxf=1.0"); // s-* channel orders are doing additional math (PRM 7.1.4.1.2)
        break;
      default:
        output->SetComparisonMethod("ulps=2,minf=0.0,maxf=1.0"); //1.5ulp [0.0; 1.0]
        break;
      }
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
      output->SetComparisonMethod("ulps=0"); //f16 denorms should not be flushed (as it will produce normalized f32)
      break;
    case BRIG_CHANNEL_TYPE_FLOAT:
      output->SetComparisonMethod("ulps=0,flushDenorms"); //flushDenorms
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
    uint16_t channels =  IsImageDepth(imageGeometryProp) ? 1 : 4;
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++){
          Value coords[3];
          Value texel[4];
          coords[0] = Value(MV_UINT32, U32(x));
          coords[1] = Value(MV_UINT32, U32(y));
          coords[2] = Value(MV_UINT32, U32(z));
          imgobj->LoadColor(coords, texel);
          for (uint16_t i = 0; i < channels; i++)
            result->push_back(texel[i]);
        }
  }

  uint64_t ResultDim() const override {
    return IsImageDepth(imageGeometryProp) ? 1 : 4;
  }

  TypedReg GetCoords()
  {
    int dim = 0;
    switch (imageGeometryProp)
    {
    case BRIG_GEOMETRY_1D:
    case BRIG_GEOMETRY_1DB:
      dim = 1;
      break;
    case BRIG_GEOMETRY_1DA:
    case BRIG_GEOMETRY_2D:
    case BRIG_GEOMETRY_2DDEPTH:
      dim = 2;
      break;
    case BRIG_GEOMETRY_3D:
    case BRIG_GEOMETRY_2DA:
    case BRIG_GEOMETRY_2DADEPTH:
      dim = 3;
      break;
    default:
      assert(0);
    }

    auto result = be.AddTReg(coordType, dim);

    for(int i=0; i<dim; i++){
      auto gid = be.EmitWorkitemAbsId(i, false);
      be.EmitMov(result->Reg(i), gid->Reg(), 32);
    }

    return result;
  }

  TypedReg Result() {
    // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable()));

    auto regs_dest = be.AddTReg(ResultType(), static_cast<unsigned>(ResultDim()));
    imgobj->EmitImageLd(regs_dest, imageaddr, GetCoords());
    return regs_dest;
  }
};

void ImageLdTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageLdTest>(ap, it, "image_ld/basic", CodeLocations(), cc->Grids().ImagesSet(), cc->Images().ImageGeometryProps(), cc->Images().ImageChannelOrders(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());
}

} // hsail_conformance
