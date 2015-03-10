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

#include "ImageInitializerTests.hpp"
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

class SamplerInitializerTest: public Test {
private:
  BrigSamplerCoordNormalization coord; 
  BrigSamplerFilter filter;
  BrigSamplerAddressing addresing;
  BrigSegment segment;
  Location initializerLocation;
  uint64_t dim;
  bool isConst;
  Sampler sampler;

  uint64_t SamplerDim() const { 
    return std::max<uint64_t>(dim, static_cast<uint64_t>(1));
  }

public:
  explicit SamplerInitializerTest(
    BrigSamplerCoordNormalization coord_, 
    BrigSamplerFilter filter_,
    BrigSamplerAddressing addresing_,
    BrigSegment segment_,
    Location initializerLocation_,
    uint64_t dim_,
    bool isConst_)
    : Test(initializerLocation_ == Location::FUNCTION ? Location::FUNCTION : Location::KERNEL), 
      coord(coord_), filter(filter_), addresing(addresing_), segment(segment_), 
      initializerLocation(initializerLocation_), dim(dim_), isConst(isConst_) {}

  void Init() override {
    Test::Init();
    ESamplerSpec samplerSpec(segment, initializerLocation, dim, isConst);
    samplerSpec.CoordNormalization(coord);
    samplerSpec.Filter(filter);
    samplerSpec.Addresing(addresing);
    sampler = te->NewSampler("sampler", &samplerSpec);
  }

  void Name(std::ostream& out) const override {
    out << LocationString(initializerLocation) << "/"
        << segment2str(segment) << "/";
    if (isConst) {
      out << "const_";
    }
    out << samplerCoordNormalization2str(coord) << "_" 
        << samplerFilter2str(filter) << "_"
        << samplerAddressing2str(addresing);
    if (dim != 0) {
      out << "[" << dim << "]";
    }
  }

  bool IsValid() const override {
    if (coord == BRIG_COORD_UNNORMALIZED && 
       (addresing == BRIG_ADDRESSING_REPEAT || addresing == BRIG_ADDRESSING_MIRRORED_REPEAT)) {
      return false;
    }
    if (initializerLocation != Location::KERNEL && 
        initializerLocation != Location::MODULE && 
        initializerLocation != Location::FUNCTION) {
      return false;
    }
    return true;
  }

  void ModuleDirectives() override {
    be.EmitExtensionDirective("IMAGE");
  }

  void ModuleVariables() override {
    sampler->ModuleVariables();
  }

  void KernelVariables() override {
    sampler->KernelVariables();
  }

  void FunctionVariables() override {
    sampler->FunctionVariables();
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(MV_UINT32, 1); }

  TypedReg Result() {
    auto trueLabel = "@true";
    auto falseLabel = "@false";
    auto endLabel = "@end";
    auto loopLabel = "@loop";

    auto cmp = be.AddCTReg();
    auto dest = be.AddTReg(BRIG_TYPE_U32);

    auto counter = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(counter, be.Immed(counter->Type(), 0));
    be.EmitLabel(loopLabel);

    // load appropriate sampler from array
    auto samplerAddr = be.AddTReg(BRIG_TYPE_SAMP);
    auto offset = be.AddAReg(segment);
    auto cvt = be.AddTReg(offset->Type());
    be.EmitCvtOrMov(cvt, counter);
    be.EmitArith(BRIG_OPCODE_MUL, offset, cvt, be.Immed(offset->Type(), 8)); // sampler handel always has a size of 8 bytes
    be.EmitLoad(sampler->Segment(), samplerAddr->Type(), samplerAddr->Reg(), be.Address(sampler->Variable(), offset->Reg(), 0)); 

    // query sampler addresing
    // if addresing is set to "undefined" then implementation can return any addresing mode
    // skip addresing mode checking if it was set to "undefined"
    if (addresing != BRIG_ADDRESSING_UNDEFINED) {
      sampler->EmitSamplerQuery(dest, samplerAddr, BRIG_SAMPLER_QUERY_ADDRESSING);
      be.EmitCmp(cmp->Reg(), dest, be.Immed(dest->Type(), addresing), BRIG_COMPARE_NE);
      be.EmitCbr(cmp->Reg(), falseLabel);
    }

    // query sampler coordinate
    sampler->EmitSamplerQuery(dest, samplerAddr, BRIG_SAMPLER_QUERY_COORD);
    be.EmitCmp(cmp->Reg(), dest, be.Immed(dest->Type(), coord), BRIG_COMPARE_NE);
    be.EmitCbr(cmp->Reg(), falseLabel);

    // query sampler filter
    sampler->EmitSamplerQuery(dest, samplerAddr, BRIG_SAMPLER_QUERY_FILTER);
    be.EmitCmp(cmp->Reg(), dest, be.Immed(dest->Type(), filter), BRIG_COMPARE_NE);
    be.EmitCbr(cmp->Reg(), falseLabel);

    be.EmitArith(BRIG_OPCODE_ADD, counter, counter, be.Immed(counter->Type(), 1));
    be.EmitCmp(cmp->Reg(), counter, be.Immed(counter->Type(), SamplerDim()), BRIG_COMPARE_LT);
    be.EmitCbr(cmp->Reg(), loopLabel);

    // true
    auto result = be.AddTReg(ResultType());
    be.EmitLabel(trueLabel);
    be.EmitMov(result, be.Immed(result->Type(), 1));
    be.EmitBr(endLabel);
    // false
    be.EmitLabel(falseLabel);
    be.EmitMov(result, be.Immed(result->Type(), 0));
    //end
    be.EmitLabel(endLabel);
    return result;
  }
};

void ImageInitializerTestSet::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();

  auto seq = new(ap) OneValueSequence<uint32_t>(1);
  TestForEach<SamplerInitializerTest>(ap, it, "initializer/sampler", cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings(), cc->Segments().InitializableSegments(), cc->Variables().InitializerLocations(), seq, Bools::All());
  //TestForEach<SamplerInitializerTest>(ap, it, "initializer/sampler", cc->Sampler().SamplerCoords(), cc->Sampler().SamplerFilters(), cc->Sampler().SamplerAddressings(), cc->Segments().InitializableSegments(), cc->Variables().InitializerLocations(), cc->Variables().InitializerDims(), Bools::All());
}

} // hsail_conformance
