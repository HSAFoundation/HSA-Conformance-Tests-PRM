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
using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class ImageStTest:  public Test {
private:
  Variable nx;
  Variable ny;
  Image imgobj;

public:
  ImageStTest(Location codeLocation, 
      Grid geometry): Test(codeLocation, geometry)
  {
  }
  
  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry;
  }

  void Init() {
   Test::Init();

   imgobj = kernel->NewImage("%rwimage", BRIG_SEGMENT_KERNARG, BRIG_GEOMETRY_1D, BRIG_CHANNEL_ORDER_A, BRIG_CHANNEL_TYPE_UNSIGNED_INT8, BRIG_ACCESS_PERMISSION_RW, 1000,1,1,1,1);
   for (unsigned i = 0; i < 1000; ++i) { imgobj->AddData(Value(MV_UINT32, 0xFFFFFFFF)); }

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
    return Value(MV_UINT32, 0xAA);
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

    OperandOperandList reg_dest = be.AddVec(BRIG_TYPE_U32, 4);

    auto coord = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(coord, be.Immed(coord->Type(), 0));

    be.EmitMov(reg_dest.elements(3), be.Immed(coord->Type(), 0xAA), 32);

    imgobj->EmitImageSt(reg_dest, imageaddr, coord);
    imgobj->EmitImageLd(reg_dest, BRIG_TYPE_U32, imageaddr, coord);
    
    be.EmitMov(result, reg_dest.elements(3));

    be.Brigantine().addLabel(s_label_exit);
    return result;
  }
};

void ImageStTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<ImageStTest>(ap, it, "image_st_1d/basic", CodeLocations(), cc->Grids().DimensionSet());
}

} // hsail_conformance
