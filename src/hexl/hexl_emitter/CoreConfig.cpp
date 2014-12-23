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

#include "CoreConfig.hpp"
#include "Emitter.hpp"
#include "BrigEmitter.hpp"

using namespace Brig;

namespace hexl {

namespace emitter {
const char *CoreConfig::CONTEXT_KEY = "hsail_conformance.coreConfig";

CoreConfig::CoreConfig(
  Brig::BrigVersion32_t majorVersion_, Brig::BrigVersion32_t minorVersion_,
  Brig::BrigMachineModel8_t model_, Brig::BrigProfile8_t profile_)
  : ap(new Arena()),
    majorVersion(majorVersion_), minorVersion(minorVersion_),
    model(model_), profile(profile_),
    wavesize(64),
    grids(this),
    segments(this),
    types(this),
    variables(this),
    queues(this),
    memory(this),
    directives(this),
    controlFlow(this)
{
}

CoreConfig::GridsConfig::GridsConfig(CoreConfig* cc)
  : ConfigBase(cc),
    defaultGeometry(1, 1, 1, 1, 1, 1, 1),
    defaultGeometrySet(NEWA OneValueSequence<Grid>(&defaultGeometry)),
    all(NEWA hexl::VectorSequence<hexl::Grid>()),
    degenerate(NEWA hexl::VectorSequence<hexl::Grid>()),
    dimension(NEWA hexl::VectorSequence<hexl::Grid>()),
    boundary24(NEWA hexl::VectorSequence<hexl::Grid>()),
    boundary32(NEWA hexl::VectorSequence<hexl::Grid>()),
    severalwaves(NEWA hexl::VectorSequence<hexl::Grid>())
{
  dimensions.Add(0);
  dimensions.Add(1);
  dimensions.Add(2);
  all->Add(NEWA GridGeometry(1,  256,  1,   1,  64,  1,   1));
  all->Add(NEWA GridGeometry(1,  200,  1,   1,  64,  1,   1));
  all->Add(NEWA GridGeometry(2,  32,   8,   1,  8,   4,   1));
  all->Add(NEWA GridGeometry(2,  30,   7,   1,  8,   4,   1));
  all->Add(NEWA GridGeometry(3,  4,    8,  16,  4,   2,   8));
  all->Add(NEWA GridGeometry(3,  3,    5,  11,  4,   2,   8));
  degenerate->Add(NEWA GridGeometry(1,  1,  1,   1,  64,  1,   1));
  degenerate->Add(NEWA GridGeometry(2,  200,  1,   1,  64,  1,   1));
  degenerate->Add(NEWA GridGeometry(3,  30,  7,   1,  8,  4,   1));
  degenerate->Add(NEWA GridGeometry(3,  200,  1,   1,  64,  1,   1));
  dimension->Add(NEWA GridGeometry(1,  200,  1,   1,  64,  1,   1));
  dimension->Add(NEWA GridGeometry(2,  30,   7,   1,  8,   4,   1));
  dimension->Add(NEWA GridGeometry(3,  3,    5,  11,  4,   2,   8));
  boundary24->Add(NEWA GridGeometry(1,  0x1000040,          1,          1,  64,  1,   1));
  boundary24->Add(NEWA GridGeometry(2,   0x800020,          2,          1,  64,  1,   1));
  boundary24->Add(NEWA GridGeometry(2,          2,   0x800020,          1,  64,  1,   1));
  boundary24->Add(NEWA GridGeometry(3,   0x400020,          2,          2,  64,  1,   1));
  boundary24->Add(NEWA GridGeometry(3,          2,   0x400020,          2,  64,  1,   1));
  boundary24->Add(NEWA GridGeometry(3,          2,          2,   0x400020,  64,  1,   1));
  boundary32->Add(NEWA GridGeometry(2, 0x80000040,          2,          1,  64,  1,   1));
  boundary32->Add(NEWA GridGeometry(2,          2, 0x80000040,          1,  64,  1,   1));
  boundary32->Add(NEWA GridGeometry(3, 0x40000020,          2,          2,  64,  1,   1));
  boundary32->Add(NEWA GridGeometry(3,          2, 0x40000020,          2,  64,  1,   1));
  boundary32->Add(NEWA GridGeometry(3,          2,          2, 0x40000020,  64,  1,   1));
  severalwaves->Add(NEWA GridGeometry(1,  256,  1,   1,  cc->Wavesize(),  1,   1));
}

static const BrigSegment allSegments[] = {
  BRIG_SEGMENT_FLAT,
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_READONLY,
  BRIG_SEGMENT_KERNARG,
  BRIG_SEGMENT_GROUP,
  BRIG_SEGMENT_PRIVATE,
  BRIG_SEGMENT_SPILL,
  BRIG_SEGMENT_ARG,
};

static const BrigSegment variableSegments[] = {
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_READONLY,
  BRIG_SEGMENT_KERNARG,
  BRIG_SEGMENT_GROUP,
  BRIG_SEGMENT_PRIVATE,
  BRIG_SEGMENT_SPILL,
  BRIG_SEGMENT_ARG,
};

static const BrigSegment atomicSegments[] = {
  BRIG_SEGMENT_FLAT,
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_GROUP,
};

static const BrigSegment initializableSegments[] = {
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_READONLY,
};

CoreConfig::SegmentsConfig::SegmentsConfig(CoreConfig* cc)
  : ConfigBase(cc),
    all(NEWA hexl::ArraySequence<BrigSegment>(allSegments, NELEM(allSegments))),
    variable(NEWA hexl::ArraySequence<BrigSegment>(variableSegments, NELEM(variableSegments))),
    atomic(NEWA hexl::ArraySequence<BrigSegment>(atomicSegments, NELEM(atomicSegments))),
    initializable(NEWA hexl::ArraySequence<BrigSegment>(initializableSegments, NELEM(initializableSegments)))
{
  for (unsigned segment = BRIG_SEGMENT_NONE; segment != BRIG_SEGMENT_MAX; ++segment) {
    singleList[segment] = new (ap) hexl::OneValueSequence<BrigSegment>((BrigSegment) segment);
  }

}

bool CoreConfig::SegmentsConfig::CanStore(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_KERNARG:
    return false;
  case BRIG_SEGMENT_FLAT:
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
  case BRIG_SEGMENT_SPILL:
  case BRIG_SEGMENT_ARG:
    return true;
  default:
    assert(false); return true;
  }
}

bool CoreConfig::SegmentsConfig::HasAddress(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_ARG:
  case BRIG_SEGMENT_SPILL:
    return false;
  case BRIG_SEGMENT_KERNARG:
  case BRIG_SEGMENT_FLAT:
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
    return true;
  default:
    assert(false); return true;
  }
}

bool CoreConfig::SegmentsConfig::HasFlatAddress(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_ARG:
  case BRIG_SEGMENT_SPILL:
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_KERNARG:
    return false;
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
    return true;
  case BRIG_SEGMENT_FLAT:
    assert(false); return true;
  default:
    assert(false); return true;
  }
}

bool CoreConfig::SegmentsConfig::CanPassAddressToKernel(BrigSegment8_t segment)
{
  switch (segment) {
  case BRIG_SEGMENT_KERNARG:
  case BRIG_SEGMENT_ARG:
  case BRIG_SEGMENT_SPILL:
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
    return false;
  case BRIG_SEGMENT_FLAT:
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_READONLY:
    return true;
  default:
    assert(false); return true;
  }
}

hexl::Sequence<Brig::BrigSegment>*
CoreConfig::SegmentsConfig::Single(Brig::BrigSegment segment)
{
  return singleList[segment];
}

static const BrigTypeX compoundTypes[] = {
  BRIG_TYPE_U8,
  BRIG_TYPE_U16,
  BRIG_TYPE_U32,
  BRIG_TYPE_U64,
  BRIG_TYPE_S8,
  BRIG_TYPE_S16,
  BRIG_TYPE_S32,
  BRIG_TYPE_S64,
//  BRIG_TYPE_F16,
  BRIG_TYPE_F32,
  BRIG_TYPE_F64,
};

static const BrigTypeX compoundIntegralTypes[] = {
  BRIG_TYPE_U8,
  BRIG_TYPE_U16,
  BRIG_TYPE_U32,
  BRIG_TYPE_U64,
  BRIG_TYPE_S8,
  BRIG_TYPE_S16,
  BRIG_TYPE_S32,
  BRIG_TYPE_S64
};

static const BrigTypeX compoundFloatingTypes[] = {
//  BRIG_TYPE_F16,
  BRIG_TYPE_F32,
  BRIG_TYPE_F64
};

CoreConfig::TypesConfig::TypesConfig(CoreConfig* cc)
  : ConfigBase(cc),
    compound(NEWA ArraySequence<BrigTypeX>(compoundTypes, NELEM(compoundTypes))),
    compoundIntegral(NEWA ArraySequence<BrigTypeX>(compoundIntegralTypes, NELEM(compoundIntegralTypes))),
    compoundFloating(NEWA ArraySequence<BrigTypeX>(compoundFloatingTypes, NELEM(compoundFloatingTypes)))
{
}

static const uint64_t smallDimensions[] = { 0, 1, 2, 3, 4, 8, };
static const uint64_t initializerDimensions[] = {0, 1, 2, 64};

CoreConfig::VariablesConfig::VariablesConfig(CoreConfig* cc)
  : ConfigBase(cc),
  bySegmentType(SequenceMap<EVariableSpec>(ap, SequenceProduct(ap, cc->Segments().Variable(), cc->Types().Compound()))),
  dim0(NEWA OneValueSequence<uint64_t>(0)),
  dims(NEWA ArraySequence<uint64_t>(smallDimensions, NELEM(smallDimensions))),
  initializerDims(NEWA ArraySequence<uint64_t>(initializerDimensions, NELEM(initializerDimensions))),
  autoLocation(NEWA OneValueSequence<Location>(AUTO))
{
  for (unsigned a = BRIG_ALIGNMENT_1; a != BRIG_ALIGNMENT_LAST; a++) {
    allAlignment.Add((BrigAlignment) a);
  }
  for (unsigned segment = BRIG_SEGMENT_NONE; segment != BRIG_SEGMENT_MAX; segment++) {
    auto product = SequenceProduct(ap,
      cc->Segments().Single((BrigSegment) segment),
      cc->Types().Compound(),
      cc->Variables().AutoLocation(),
      cc->Variables().AllAlignment());
    byTypeAlign[segment] = SequenceMap<EVariableSpec>(ap, product);
    byTypeDimensionAlign[segment] = SequenceMap<EVariableSpec>(ap,
      SequenceProduct(ap,
        cc->Segments().Single((BrigSegment) segment),
        cc->Types().Compound(),
        cc->Variables().AutoLocation(),
        cc->Variables().AllAlignment(),
        cc->Variables().Dims()
      ));
  }
}

static const BrigSegment queueSegments[] = { BRIG_SEGMENT_GLOBAL, BRIG_SEGMENT_FLAT };
static const BrigOpcode ldOpcodesValues[] = { BRIG_OPCODE_LDQUEUEREADINDEX, BRIG_OPCODE_LDQUEUEWRITEINDEX };
static const BrigOpcode addCasOpcodesValues[] = { BRIG_OPCODE_ADDQUEUEWRITEINDEX, BRIG_OPCODE_CASQUEUEWRITEINDEX };
static const BrigOpcode stOpcodesValues[] = { BRIG_OPCODE_STQUEUEREADINDEX, BRIG_OPCODE_STQUEUEWRITEINDEX };
static const BrigMemoryOrder ldMemoryOrdersValues[] = { BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_SC_ACQUIRE };
static const BrigMemoryOrder addCasMemoryOrdersValues[] = { BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE };
static const BrigMemoryOrder stMemoryOrdersValues[] = { BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_SC_RELEASE };

CoreConfig::QueuesConfig::QueuesConfig(CoreConfig* cc)
  : ConfigBase(cc),
    types(NEWA EnumSequence<UserModeQueueType>(SOURCE_START, SOURCE_END)),
    segments(NEWA ArraySequence<BrigSegment>(queueSegments, NELEM(queueSegments))),
    ldOpcodes(NEWA ArraySequence<BrigOpcode>(ldOpcodesValues, NELEM(ldOpcodesValues))),
    addCasOpcodes(NEWA ArraySequence<BrigOpcode>(addCasOpcodesValues, NELEM(addCasOpcodesValues))),
    stOpcodes(NEWA ArraySequence<BrigOpcode>(stOpcodesValues, NELEM(stOpcodesValues))),
    ldMemoryOrders(NEWA ArraySequence<BrigMemoryOrder>(ldMemoryOrdersValues, NELEM(ldMemoryOrdersValues))),
    addCasMemoryOrders(NEWA ArraySequence<BrigMemoryOrder>(addCasMemoryOrdersValues, NELEM(addCasMemoryOrdersValues))),
    stMemoryOrders(NEWA ArraySequence<BrigMemoryOrder>(stMemoryOrdersValues, NELEM(stMemoryOrdersValues)))
{
}

static const BrigAtomicOperation allAtomicsValues[] = {
// TODO: initial & expected values for the rest atomics in barrier tests
  BRIG_ATOMIC_ADD,
  BRIG_ATOMIC_AND,
//      BRIG_ATOMIC_CAS,
//      BRIG_ATOMIC_EXCH,
//      BRIG_ATOMIC_LD,
  BRIG_ATOMIC_MAX,
  BRIG_ATOMIC_MIN,
  BRIG_ATOMIC_OR,
//      BRIG_ATOMIC_ST,
  BRIG_ATOMIC_SUB,
  BRIG_ATOMIC_WRAPDEC,
  BRIG_ATOMIC_WRAPINC,
  BRIG_ATOMIC_XOR
};

static const BrigAtomicOperation signalSendAtomicsValues[] = {
  BRIG_ATOMIC_ST,
  BRIG_ATOMIC_ADD,
  BRIG_ATOMIC_AND,
  BRIG_ATOMIC_CAS,
  BRIG_ATOMIC_EXCH,
  BRIG_ATOMIC_OR,
  BRIG_ATOMIC_SUB,
  BRIG_ATOMIC_XOR
};

static const BrigAtomicOperation signalWaitAtomicsValues[] = {
  BRIG_ATOMIC_LD,
  BRIG_ATOMIC_WAIT_EQ,
  BRIG_ATOMIC_WAIT_NE,
  BRIG_ATOMIC_WAIT_LT,
  BRIG_ATOMIC_WAIT_GTE,
  BRIG_ATOMIC_WAITTIMEOUT_EQ,
  BRIG_ATOMIC_WAITTIMEOUT_NE,
  BRIG_ATOMIC_WAITTIMEOUT_LT,
  BRIG_ATOMIC_WAITTIMEOUT_GTE
};

static const BrigSegment memfenceSegmentsValues[] = {
  BRIG_SEGMENT_GLOBAL,
  BRIG_SEGMENT_GROUP,
};

CoreConfig::MemoryConfig::MemoryConfig(CoreConfig* cc)
  : ConfigBase(cc),
    allMemoryOrders(NEWA EnumSequence<BrigMemoryOrder>(BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_LAST)),
    signalSendMemoryOrders(NEWA EnumSequence<BrigMemoryOrder>(BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_LAST)),
    signalWaitMemoryOrders(NEWA EnumSequence<BrigMemoryOrder>(BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_ORDER_SC_RELEASE)),
    allMemoryScopes(NEWA EnumSequence<BrigMemoryScope>(BRIG_MEMORY_SCOPE_WORKITEM, BRIG_MEMORY_SCOPE_LAST)),
    allAtomics(NEWA ArraySequence<BrigAtomicOperation>(allAtomicsValues, NELEM(allAtomicsValues))),
    signalSendAtomics(NEWA ArraySequence<BrigAtomicOperation>(signalSendAtomicsValues, NELEM(signalSendAtomicsValues))),
    signalWaitAtomics(NEWA ArraySequence<BrigAtomicOperation>(signalWaitAtomicsValues, NELEM(signalWaitAtomicsValues))),
    memfenceSegments(new (ap) hexl::ArraySequence<BrigSegment>(memfenceSegmentsValues, NELEM(memfenceSegmentsValues)))
{    
}

static const BrigControlDirective gridGroupRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
  BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS,
};

static const BrigControlDirective gridSizeRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
};

static const BrigControlDirective workitemIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
};

static const BrigControlDirective workitemAbsIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
};

static const BrigControlDirective workitemFlatIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
  BRIG_CONTROL_MAXFLATWORKGROUPSIZE,
};

static const BrigControlDirective workitemFlatAbsIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDDIM,
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
};

static const BrigControlDirective degenerateRelatedValues[] = {
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
};

static const BrigControlDirective boundary24WorkitemAbsIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
};

static const BrigControlDirective boundary24WorkitemFlatAbsIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
};

static const BrigControlDirective boundary24WorkitemFlatIdRelatedValues[] = {
  BRIG_CONTROL_REQUIREDGRIDSIZE,
  BRIG_CONTROL_MAXFLATGRIDSIZE,
  BRIG_CONTROL_REQUIREDWORKGROUPSIZE,
  BRIG_CONTROL_MAXFLATWORKGROUPSIZE,
};

CoreConfig::ControlDirectivesConfig::ControlDirectivesConfig(CoreConfig* cc)
  : ConfigBase(cc),
    none(NEWA EControlDirectives(NEWA EmptySequence<Brig::BrigControlDirective>())),
    dimensionRelated(NEWA EControlDirectives(NEWA OneValueSequence<BrigControlDirective>(BRIG_CONTROL_REQUIREDDIM))),
    gridGroupRelated(Array(ap, gridGroupRelatedValues, NELEM(gridGroupRelatedValues))),
    gridSizeRelated(Array(ap, gridSizeRelatedValues, NELEM(gridSizeRelatedValues))),
    workitemIdRelated(Array(ap, workitemIdRelatedValues, NELEM(workitemIdRelatedValues))),
    workitemAbsIdRelated(Array(ap, workitemAbsIdRelatedValues, NELEM(workitemAbsIdRelatedValues))),
    workitemFlatIdRelated(Array(ap, workitemFlatIdRelatedValues, NELEM(workitemFlatIdRelatedValues))),
    workitemFlatAbsIdRelated(Array(ap, workitemFlatAbsIdRelatedValues, NELEM(workitemFlatAbsIdRelatedValues))),
    degenerateRelated(Array(ap, degenerateRelatedValues, NELEM(degenerateRelatedValues))),
    boundary24WorkitemAbsIdRelated(Array(ap, boundary24WorkitemAbsIdRelatedValues, NELEM(boundary24WorkitemAbsIdRelatedValues))),
    boundary24WorkitemFlatAbsIdRelated(Array(ap, boundary24WorkitemFlatAbsIdRelatedValues, NELEM(boundary24WorkitemFlatAbsIdRelatedValues))),
    boundary24WorkitemFlatIdRelated(Array(ap, boundary24WorkitemFlatIdRelatedValues, NELEM(boundary24WorkitemFlatIdRelatedValues))),
    noneSets(DSubsets(ap, none)),
    dimensionRelatedSets(DSubsets(ap, dimensionRelated)),
    gridGroupRelatedSets(DSubsets(ap, gridGroupRelated)),
    gridSizeRelatedSets(DSubsets(ap, gridSizeRelated)),
    workitemIdRelatedSets(DSubsets(ap, workitemIdRelated)),
    workitemAbsIdRelatedSets(DSubsets(ap, workitemAbsIdRelated)),
    workitemFlatIdRelatedSets(DSubsets(ap, workitemFlatIdRelated)),
    workitemFlatAbsIdRelatedSets(DSubsets(ap, workitemFlatAbsIdRelated)),
    degenerateRelatedSets(DSubsets(ap, degenerateRelated)),
    boundary24WorkitemAbsIdRelatedSets(DSubsets(ap, boundary24WorkitemAbsIdRelated)),
    boundary24WorkitemFlatAbsIdRelatedSets(DSubsets(ap, boundary24WorkitemFlatAbsIdRelated)),
    boundary24WorkitemFlatIdRelatedSets(DSubsets(ap, boundary24WorkitemFlatIdRelated))
{
}

hexl::Sequence<ControlDirectives>* CoreConfig::ControlDirectivesConfig::DSubsets(Arena *ap, const ControlDirectives& set)
{
  return SequenceMap<EControlDirectives>(ap, Subsets(ap, set->Spec()));
}

ControlDirectives CoreConfig::ControlDirectivesConfig::Array(Arena* ap, const BrigControlDirective *values, size_t count)
{
  return NEWA EControlDirectives(NEWA ArraySequence<BrigControlDirective>(values, (unsigned) count));
}

CoreConfig::ControlFlowConfig::ControlFlowConfig(CoreConfig* cc)
  : ConfigBase(cc),
    allWidths(NEWA EnumSequence<BrigWidth>(BRIG_WIDTH_NONE, BRIG_WIDTH_LAST)),
    workgroupWidths(NEWA VectorSequence<BrigWidth>()),
    conditionInputs(NEWA EnumSequence<ConditionInput>(COND_INPUT_START, COND_INPUT_END)),
    binaryConditions(SequenceMap<ECondition>(ap, SequenceProduct(ap, NEWA OneValueSequence<ConditionType>(COND_BINARY), ConditionInputs(), WorkgroupWidths()))),
    sbrTypes(NEWA EnumSequence<BrigTypeX>(BRIG_TYPE_U32, BRIG_TYPE_S8)),
    switchConditions(SequenceMap<ECondition>(ap, SequenceProduct(ap, NEWA OneValueSequence<ConditionType>(COND_SWITCH), ConditionInputs(), SbrTypes(), WorkgroupWidths())))
{
  for (unsigned w = BRIG_WIDTH_1; w <= BRIG_WIDTH_256; ++w) {
    workgroupWidths->Add((BrigWidth) w);
  }
  workgroupWidths->Add(BRIG_WIDTH_WAVESIZE);
  workgroupWidths->Add(BRIG_WIDTH_ALL);
}
}
}