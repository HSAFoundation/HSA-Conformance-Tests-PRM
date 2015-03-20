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

  uint32_t InitialValue() const { return 0x30313233; }

  uint32_t StorePerRegValue() const { return 0x00000080; }

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
    imgobj->AddData(imgobj->GenMemValue(Value(MV_UINT32, InitialValue())));

    imgobj->InitImageCalculator(NULL);

    Value coords[3];
    coords[0] = Value(0.0f);
    coords[1] = Value(0.0f);
    coords[2] = Value(0.0f);
    Value channels[4];
    for (unsigned i = 0; i < 4; i++) { channels[i] = Value(MV_UINT32, StorePerRegValue()); }
    Value Val_store = imgobj->StoreColor(NULL, channels);
    imgobj->SetValueForCalculator(imgobj->GenMemValue(Val_store));
    imgobj->ReadColor(coords, color);
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

   bool IsValid() const  {
    return IsImageSupported(imageGeometryProp, imageChannelOrder, imageChannelType) && IsImageGeometrySupported(imageGeometryProp, imageGeometry) && (codeLocation != FUNCTION);
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
    return 4;
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


  OperandOperandList StoreValues(int cnt) {
    auto result = be.AddVec(BRIG_TYPE_U32, cnt);
    for (int i = 0; i < cnt; i++)
      be.EmitMov(result.elements(i), be.Immed(ResultType(), StorePerRegValue()), 32);
    return result;
  }

  void KernelCode() override {
    auto result = be.AddTReg(ResultType());
    be.EmitMov(result, be.Immed(ResultType(), 0));
    // Load input
    auto imageaddr = be.AddTReg(imgobj->Variable().type());
    be.EmitLoad(imgobj->Segment(), imageaddr->Type(), imageaddr->Reg(), be.Address(imgobj->Variable())); 

    OperandOperandList regs_dest;
    auto reg_dest = be.AddTReg(BRIG_TYPE_U32, 1);
    be.EmitMov(result, be.Immed(ResultType(), StorePerRegValue()));
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
    auto outputAddr = output->Address();
    auto storeAddr = be.AddAReg(outputAddr->Segment());
    auto offsetBase = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    auto offset = be.AddTReg(offsetBase->Type());

    for (unsigned i = 0; i < regs_dest.elementCount(); i++) {
      be.EmitMov(offset, be.Immed(offsetBase->Type(), i));
      be.EmitArith(BRIG_OPCODE_MUL, offset, offset->Reg(), be.Immed(offsetBase->Type(), 4));
      be.EmitArith(BRIG_OPCODE_ADD, storeAddr, outputAddr, offset->Reg());
      be.EmitStore(BRIG_TYPE_U32, regs_dest.elements(i), storeAddr);
    }
  }
};

void ImageStTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageStTest>(ap, it, "image_st/basic", CodeLocations(), cc->Grids().ImagesSet(),  cc->Images().ImageGeometryProps(), cc->Images().ImageSupportedChannelOrders(), cc->Images().ImageChannelTypes(), cc->Images().ImageArraySets());
}

} // hsail_conformance
