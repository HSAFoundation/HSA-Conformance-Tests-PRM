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
  Value color[4];
  BrigType coordType;

public:
  ImageStTest(Location codeLocation, 
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

  uint32_t StorePerRegValue() const { return 0x1922aaba; }

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
    imgobj = kernel->NewImage("%rwimage", HOST_INPUT_IMAGE, &imageSpec, IsImageOptional(imageGeometryProp, imageChannelOrder, imageChannelType, BRIG_TYPE_RWIMG));
    imgobj->SetInitialData(imgobj->GenMemValue(Value(MV_UINT32, InitialValue())));

    imgobj->InitImageCalculator(NULL);

    Value coords[3];
    coords[0] = Value(MV_UINT32, 0);
    coords[1] = Value(MV_UINT32, 0);
    coords[2] = Value(MV_UINT32, 0);
    Value channels[4];
    for (unsigned i = 0; i < 4; i++) { channels[i] = Value(MV_UINT32, StorePerRegValue()); }
    Value Val_store = imgobj->StoreColor(NULL, channels);
    imgobj->SetValueForCalculator(imgobj->GenMemValue(Val_store));
    imgobj->LoadColor(coords, color);
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

   bool IsValid() const  {
    return IsImageLegal(imageGeometryProp, imageChannelOrder, imageChannelType) && IsImageGeometrySupported(imageGeometryProp, imageGeometry) && (codeLocation != FUNCTION);
  }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  void ExpectedResults(Values* result) const {
    for (unsigned i = 0; i < 4; i ++)
    {
      Value res = Value(MV_UINT32, color[i].U32());
      result->push_back(res);
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

    auto regs_dest = be.AddTReg(BRIG_TYPE_U32, ResultDim());
    TypedReg coords = GetCoords();    
    TypedReg storeValues = be.AddTReg(BRIG_TYPE_U32, ResultDim());
    for(int i = 0; i < ResultDim(); i++)
      be.EmitMov(storeValues->Reg(i), be.Immed(ResultType(), StorePerRegValue()), 32);

    imgobj->EmitImageSt(storeValues, imageaddr, coords);
    be.EmitImageFence();
    imgobj->EmitImageLd(regs_dest, imageaddr, coords);
   
    return regs_dest;
  }
};

void ImageStTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageStTest>(ap, it, "image_st/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageChannelOrders(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());
}

} // hsail_conformance
