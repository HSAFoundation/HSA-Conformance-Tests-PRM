/*
   Copyright 2014-2015 Heterogeneous System Architecture (HSA) Foundation

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

#ifndef CORE_CONFIG_HPP
#define CORE_CONFIG_HPP

#include "Brig.h"
#include "HexlContext.hpp"
#include "Arena.hpp"
#include "MObject.hpp"
#include "Sequence.hpp"
#include "EmitterCommon.hpp"
#include "Image.hpp"
#include "Utils.hpp"
#include <sstream>
#include <cassert>

#define BRIG_SEGMENT_MAX BRIG_SEGMENT_AMD_GCN

namespace hexl {

  namespace emitter {

    class CoreConfig {
    private:
      std::unique_ptr<Arena> ap;
      BrigVersion32_t majorVersion;
      BrigVersion32_t minorVersion;
      BrigMachineModel8_t model;
      BrigProfile8_t profile;
      uint32_t wavesize;
      uint8_t wavesPerGroup;
      bool isDetectSupported;
      bool isBreakSupported;
      EndiannessConfig endianness;

    public:
      static const char *CONTEXT_KEY;

      static CoreConfig* Get(hexl::Context *context) { return context->Get<CoreConfig>(CONTEXT_KEY); }

      CoreConfig(BrigVersion32_t majorVersion_ = BRIG_VERSION_HSAIL_MAJOR,
                 BrigVersion32_t minorVersion_ = BRIG_VERSION_HSAIL_MINOR,
                 BrigMachineModel8_t model_ = (sizeof(void *) == 8 ? BRIG_MACHINE_LARGE : BRIG_MACHINE_SMALL),
                 BrigProfile8_t profile_ = BRIG_PROFILE_FULL);

      void Init(hexl::Context *context);

      Arena* Ap() { return ap.get(); }
      BrigVersion32_t MajorVersion() const { return majorVersion; }
      BrigVersion32_t MinorVersion() const { return minorVersion; }
      BrigMachineModel8_t Model() const { return model; }
      BrigProfile8_t Profile() const { return profile; }
      uint32_t Wavesize() const { return wavesize; }
      uint8_t WavesPerGroup() const { return wavesPerGroup; }
      bool IsDetectSupported() const { return isDetectSupported; }
      bool IsBreakSupported() const { return isBreakSupported; }
      EndiannessConfig Endianness() { return endianness; }

      bool IsLarge() const {
        switch (model) {
        case BRIG_MACHINE_LARGE: return true;
        case BRIG_MACHINE_SMALL: return false;
        default: assert(false); return false;
        }
      }

      // Segments
    public:
      class ConfigBase {
      protected:
        Arena* ap;
      public:
        ConfigBase(CoreConfig* cc) : ap(cc->Ap()) { }
      };

      class GridsConfig : public ConfigBase {
      private:
        hexl::VectorSequence<uint32_t> *dimensions;
        hexl::GridGeometry defaultGeometry, trivialGeometry, allWavesIdGeometry;
        hexl::Sequence<hexl::Grid> *defaultGeometrySet, *trivialGeometrySet, *allWavesIdSet;
        hexl::VectorSequence<hexl::Grid> *simple, *degenerate, *dimension, *boundary24, *boundary32,
          *severalwaves, *workgroup256, *limitGrids, *singleGroup, *atomic, *mmodel, *barrier, *fbarrier, 
          *images, *memfence, *partial;

      public:
        GridsConfig(CoreConfig* cc);

        hexl::Sequence<uint32_t>* Dimensions() { return dimensions; }

        hexl::Grid DefaultGeometry() { return &defaultGeometry; }
        hexl::Grid TrivialGeometry() { return &trivialGeometry; }

        hexl::Sequence<hexl::Grid>* DefaultGeometrySet() { return defaultGeometrySet; }
        hexl::Sequence<hexl::Grid>* TrivialGeometrySet() { return trivialGeometrySet; }
        hexl::Sequence<hexl::Grid>* SimpleSet() { return simple; }
        hexl::Sequence<hexl::Grid>* DegenerateSet() { return degenerate; }
        hexl::Sequence<hexl::Grid>* DimensionSet() { return dimension; }
        hexl::Sequence<hexl::Grid>* Boundary32Set() { return boundary32; }
        hexl::Sequence<hexl::Grid>* Boundary24Set() { return boundary24; }
        hexl::Sequence<hexl::Grid>* SeveralWavesSet() { return severalwaves; }
        hexl::Sequence<hexl::Grid>* BarrierSet() { return barrier; }
        hexl::Sequence<hexl::Grid>* FBarrierSet() { return fbarrier; }
        hexl::Sequence<hexl::Grid>* AllWavesIdSet() { return allWavesIdSet; }
        hexl::Sequence<hexl::Grid>* WorkGroupsSize256() { return workgroup256; }
        hexl::Sequence<hexl::Grid>* LimitGridSet() { return limitGrids; }
        hexl::Sequence<hexl::Grid>* SingleGroupSet() { return singleGroup; }
        hexl::Sequence<hexl::Grid>* AtomicSet() { return atomic; }
        hexl::Sequence<hexl::Grid>* MModelSet() { return mmodel; }
        hexl::Sequence<hexl::Grid>* ImagesSet() { return images; }
        hexl::Sequence<hexl::Grid>* MemfenceSet() { return memfence; }
        hexl::Sequence<hexl::Grid>* PartialSet() { return partial; }
      };

      class SegmentsConfig : public ConfigBase {
      private:
        hexl::Sequence<BrigSegment>* all;
        hexl::Sequence<BrigSegment>* variable;
        hexl::Sequence<BrigSegment>* atomic;
        hexl::Sequence<BrigSegment>* initializable;
        hexl::Sequence<BrigSegment>* moduleScope;
        hexl::Sequence<BrigSegment>* functionScope;
        hexl::Sequence<BrigSegment>* singleList[BRIG_SEGMENT_MAX];
        hexl::Sequence<uint32_t>* staticGroupSize;

      public:
        SegmentsConfig(CoreConfig* cc);

        bool CanStore(BrigSegment8_t segment);
        bool HasNullptr(BrigSegment8_t segment);
        bool HasFlatAddress(BrigSegment8_t segment);
        bool HasAddress(BrigSegment8_t segment);
        bool CanPassAddressToKernel(BrigSegment8_t segment);

        hexl::Sequence<BrigSegment>* All() { return all; }
        hexl::Sequence<BrigSegment>* Variable() { return variable; }
        hexl::Sequence<BrigSegment>* Atomic() { return atomic; }
        hexl::Sequence<BrigSegment>* InitializableSegments() { return initializable; }
        hexl::Sequence<BrigSegment>* ModuleScopeVariableSegments() { return moduleScope; }
        hexl::Sequence<BrigSegment>* FunctionScopeVariableSegments() { return functionScope; }
        hexl::Sequence<BrigSegment>* Single(BrigSegment segment);
        hexl::Sequence<uint32_t>* StaticGroupSize() { return staticGroupSize; }
      };

      class TypesConfig : public ConfigBase {
      private:
        hexl::Sequence<BrigType> *compound, *compoundIntegral, *compoundFloating, *packed, *packed128, *atomic, *memModel, *memfence;
        hexl::Sequence<size_t>* registerSizes;

      public:
        TypesConfig(CoreConfig* cc);

        hexl::Sequence<BrigType>* Compound() { return compound; }
        hexl::Sequence<BrigType>* Packed() { return packed; }
        hexl::Sequence<BrigType>* Packed128Bit() { return packed128; }
        hexl::Sequence<BrigType>* Atomic() { return atomic; }
        hexl::Sequence<BrigType>* MemModel() { return memModel; }
        hexl::Sequence<BrigType>* Memfence() { return memfence; }
        hexl::Sequence<BrigType>* CompoundIntegral() { return compoundIntegral; }
        hexl::Sequence<BrigType>* CompoundFloating() { return compoundFloating; }
        hexl::Sequence<size_t>* RegisterSizes() { return registerSizes; }
      };

      class VariablesConfig : public ConfigBase {
      private:
        hexl::Sequence<VariableSpec>* bySegmentType;
        hexl::Sequence<VariableSpec>* byTypeAlign[BRIG_SEGMENT_MAX];
        hexl::Sequence<VariableSpec>* byTypeDimensionAlign[BRIG_SEGMENT_MAX];
        hexl::Sequence<uint64_t> *dim0, *dims, *initializerDims;
        hexl::Sequence<Location> *autoLocation, *initializerLocations;
        hexl::Sequence<BrigLinkage> *moduleScopeLinkage;
        hexl::VectorSequence<BrigAlignment>* allAlignment;
        hexl::EnumSequence<AnnotationLocation>* annotationLocations;

      public:
        VariablesConfig(CoreConfig* cc);

        hexl::Sequence<VariableSpec>* BySegmentType() { return bySegmentType; }
        hexl::Sequence<VariableSpec>* ByTypeAlign(BrigSegment segment) { return byTypeAlign[segment]; }
        hexl::Sequence<VariableSpec>* ByTypeDimensionAlign(BrigSegment segment) { return byTypeDimensionAlign[segment]; }

        hexl::Sequence<uint64_t>* Dim0() { return dim0; }
        hexl::Sequence<uint64_t>* Dims() { return dims; }
        hexl::Sequence<uint64_t>* InitializerDims() { return initializerDims; }

        hexl::Sequence<Location>* AutoLocation() { return autoLocation; }
        hexl::Sequence<Location>* InitializerLocations() { return initializerLocations; }

        hexl::Sequence<BrigAlignment>* AllAlignment() { return allAlignment; }
        
        hexl::Sequence<BrigLinkage>* ModuleScopeLinkage() { return moduleScopeLinkage; }

        hexl::Sequence<AnnotationLocation>* AnnotationLocations() { return annotationLocations; }
      };

      class QueuesConfig : public ConfigBase {
      private:
        hexl::Sequence<UserModeQueueType>* types;
        hexl::Sequence<BrigSegment>* segments;
        hexl::Sequence<BrigOpcode>* ldOpcodes;
        hexl::Sequence<BrigOpcode>* addCasOpcodes;
        hexl::Sequence<BrigOpcode>* stOpcodes;
        hexl::Sequence<BrigMemoryOrder>* ldMemoryOrders;
        hexl::Sequence<BrigMemoryOrder>* addCasMemoryOrders;
        hexl::Sequence<BrigMemoryOrder>* stMemoryOrders;

      public:
        QueuesConfig(CoreConfig* cc);

        hexl::Sequence<UserModeQueueType>* Types() { return types; }
        hexl::Sequence<BrigSegment>* Segments() { return segments; }
        hexl::Sequence<BrigOpcode>* LdOpcodes() { return ldOpcodes; }
        hexl::Sequence<BrigOpcode>* AddCasOpcodes() { return addCasOpcodes; }
        hexl::Sequence<BrigOpcode>* StOpcodes() { return stOpcodes; }
        hexl::Sequence<BrigMemoryOrder>* LdMemoryOrders() { return ldMemoryOrders; }
        hexl::Sequence<BrigMemoryOrder>* AddCasMemoryOrders() { return addCasMemoryOrders; }
        hexl::Sequence<BrigMemoryOrder>* StMemoryOrders() { return stMemoryOrders; }
      };

      class MemoryConfig : public ConfigBase {
      private:
        hexl::Sequence<BrigMemoryOrder> *allMemoryOrders, *signalSendMemoryOrders, *signalWaitMemoryOrders, *memfenceMemoryOrders;
        hexl::Sequence<BrigMemoryScope> *allMemoryScopes, *memfenceMemoryScopes;
        hexl::Sequence<BrigAtomicOperation> *allAtomics, *limitedAtomics, *atomicOperations, *signalSendAtomics, *signalWaitAtomics;
        hexl::Sequence<BrigSegment> *memfenceSegments;
        hexl::Sequence<BrigOpcode> *ldStOpcodes, *atomicOpcodes;

      public:
        MemoryConfig(CoreConfig* cc);
        hexl::Sequence<BrigMemoryOrder>* AllMemoryOrders() { return allMemoryOrders; }
        hexl::Sequence<BrigMemoryOrder>* SignalSendMemoryOrders() { return signalSendMemoryOrders; }
        hexl::Sequence<BrigMemoryOrder>* SignalWaitMemoryOrders() { return signalWaitMemoryOrders; }
        hexl::Sequence<BrigMemoryOrder>* MemfenceMemoryOrders() { return memfenceMemoryOrders; }
        hexl::Sequence<BrigMemoryScope>* AllMemoryScopes() { return allMemoryScopes; }
        hexl::Sequence<BrigMemoryScope>* MemfenceMemoryScopes() { return memfenceMemoryScopes; }
        hexl::Sequence<BrigAtomicOperation>* AllAtomics() { return allAtomics; }
        hexl::Sequence<BrigAtomicOperation>* LimitedAtomics() { return limitedAtomics; }
        hexl::Sequence<BrigAtomicOperation>* AtomicOperations() { return atomicOperations; }
        hexl::Sequence<BrigAtomicOperation>* SignalSendAtomics() { return signalSendAtomics; }
        hexl::Sequence<BrigAtomicOperation>* SignalWaitAtomics() { return signalWaitAtomics; }
        hexl::Sequence<BrigSegment>* MemfenceSegments() { return memfenceSegments; }
        hexl::Sequence<BrigOpcode>* LdStOpcodes() { return ldStOpcodes; }
        hexl::Sequence<BrigOpcode>* AtomicOpcodes() { return atomicOpcodes; }
      };

      class ControlDirectivesConfig : public ConfigBase {
      private:
        ControlDirectives none, dimensionRelated,
          gridGroupRelated, gridSizeRelated,
          workitemIdRelated, workitemAbsIdRelated, workitemFlatIdRelated, workitemFlatAbsIdRelated,
          degenerateRelated, boundary24WorkitemAbsIdRelated, boundary24WorkitemFlatAbsIdRelated, boundary24WorkitemFlatIdRelated;
        hexl::Sequence<ControlDirectives> *noneSets, *dimensionRelatedSets, *gridGroupRelatedSets,
          *gridSizeRelatedSets,
          *workitemIdRelatedSets, *workitemAbsIdRelatedSets, *workitemFlatIdRelatedSets, *workitemFlatAbsIdRelatedSets,
          *degenerateRelatedSets, *boundary24WorkitemAbsIdRelatedSets, *boundary24WorkitemFlatAbsIdRelatedSets, *boundary24WorkitemFlatIdRelatedSets;
        hexl::Sequence<BrigKind>* pragmaOperandTypes;
        hexl::Sequence<uint32_t>* validExceptionNumbers;
        hexl::Sequence<BrigControlDirective> *exceptionDirectives, *geometryDirectives;
        hexl::Sequence<std::string> *validExtensions;

        static ControlDirectives Array(Arena* ap, const BrigControlDirective *values, size_t count);
        static hexl::Sequence<ControlDirectives>* DSubsets(Arena* ap, const ControlDirectives& set);

      public:
        ControlDirectivesConfig(CoreConfig* cc);

        const ControlDirectives& None() { return none; }
        hexl::Sequence<ControlDirectives>* NoneSets() { return noneSets; }

        const ControlDirectives& DimensionRelated() { return dimensionRelated; }
        hexl::Sequence<ControlDirectives>* DimensionRelatedSets() { return dimensionRelatedSets; }

        const ControlDirectives& GridGroupRelated() { return gridGroupRelated; }
        hexl::Sequence<ControlDirectives>* GridGroupRelatedSets() { return gridGroupRelatedSets; }

        const ControlDirectives& GridSizeRelated() { return gridSizeRelated; }
        hexl::Sequence<ControlDirectives>* GridSizeRelatedSets() { return gridSizeRelatedSets; }
        const ControlDirectives& WorkitemIdRelated() { return workitemIdRelated; }
        hexl::Sequence<ControlDirectives>* WorkitemIdRelatedSets() { return workitemIdRelatedSets; }
        const ControlDirectives& WorkitemAbsIdRelated() { return workitemAbsIdRelated; }
        hexl::Sequence<ControlDirectives>* WorkitemAbsIdRelatedSets() { return workitemAbsIdRelatedSets; }
        const ControlDirectives& WorkitemFlatIdRelated() { return workitemFlatIdRelated; }
        hexl::Sequence<ControlDirectives>* WorkitemFlatIdRelatedSets() { return workitemFlatIdRelatedSets; }
        const ControlDirectives& WorkitemFlatAbsIdRelated() { return workitemFlatAbsIdRelated; }
        hexl::Sequence<ControlDirectives>* WorkitemFlatAbsIdRelatedSets() { return workitemFlatAbsIdRelatedSets; }
        const ControlDirectives& DegenerateRelated() { return degenerateRelated; }
        hexl::Sequence<ControlDirectives>* DegenerateRelatedSets() { return degenerateRelatedSets; }
        const ControlDirectives& Boundary24WorkitemAbsIdRelated() { return boundary24WorkitemAbsIdRelated; }
        hexl::Sequence<ControlDirectives>* Boundary24WorkitemAbsIdRelatedSets() { return boundary24WorkitemAbsIdRelatedSets; }
        const ControlDirectives& Boundary24WorkitemFlatAbsIdRelated() { return boundary24WorkitemFlatAbsIdRelated; }
        hexl::Sequence<ControlDirectives>* Boundary24WorkitemFlatAbsIdRelatedSets() { return boundary24WorkitemFlatAbsIdRelatedSets; }
        const ControlDirectives& Boundary24WorkitemFlatIdRelated()  { return boundary24WorkitemFlatIdRelated; }
        hexl::Sequence<ControlDirectives>* Boundary24WorkitemFlatIdRelatedSets()  { return boundary24WorkitemFlatIdRelatedSets; }

        hexl::Sequence<BrigKind>* PragmaOperandTypes() { return pragmaOperandTypes; }
        
        hexl::Sequence<uint32_t>* ValidExceptionNumbers() { return validExceptionNumbers; }
        hexl::Sequence<BrigControlDirective>* ExceptionDirectives() { return exceptionDirectives; }
        hexl::Sequence<BrigControlDirective>* GeometryDirectives() { return geometryDirectives; }

        hexl::Sequence<std::string>* ValidExtensions() { return validExtensions; }
      };

      class ControlFlowConfig : public ConfigBase {
      private:
        hexl::Sequence<BrigWidth> *allWidths;
        hexl::VectorSequence<BrigWidth> *workgroupWidths;
        hexl::VectorSequence<BrigWidth> *cornerWidths;
        hexl::Sequence<ConditionInput>* conditionInputs;
        hexl::Sequence<Condition>* binaryConditions;
        hexl::Sequence<Condition>* nestedConditions;
        hexl::Sequence<BrigType>* sbrTypes;
        hexl::Sequence<Condition>* switchConditions;
        hexl::Sequence<Condition>* nestedSwitchConditions;

      public:
        ControlFlowConfig(CoreConfig* cc);
        hexl::Sequence<BrigWidth>* AllWidths() { return allWidths; }
        hexl::Sequence<BrigWidth>* WorkgroupWidths() { return workgroupWidths; }
        hexl::Sequence<BrigWidth>* CornerWidths() { return cornerWidths; }
        hexl::Sequence<ConditionInput>* ConditionInputs() { return conditionInputs; }
        hexl::Sequence<Condition>* BinaryConditions() { return binaryConditions; }
        hexl::Sequence<Condition>* NestedConditions() { return nestedConditions; }
        hexl::Sequence<BrigType>* SbrTypes() { return sbrTypes; }
        hexl::Sequence<Condition>* SwitchConditions() { return switchConditions; }
        hexl::Sequence<Condition>* NestedSwitchConditions() { return nestedSwitchConditions; }
      };

      class FunctionsConfig : public ConfigBase {
      private:
        hexl::VectorSequence<unsigned>* scallFunctionsNumber;
        hexl::VectorSequence<unsigned>* scallIndexValue;
        hexl::VectorSequence<unsigned>* scallNumberRepeating;
        hexl::VectorSequence<BrigType>* scallIndexType;

      public:
        FunctionsConfig(CoreConfig* cc);

        hexl::Sequence<unsigned>* ScallFunctionsNumber() { return scallFunctionsNumber; }
        hexl::Sequence<unsigned>* ScallIndexValue() { return scallIndexValue; }
        hexl::Sequence<unsigned>* ScallNumberRepeating() { return scallNumberRepeating; }
        hexl::Sequence<BrigType>* ScallIndexType() { return scallIndexType; }
      };

      class ImageConfig : public ConfigBase {
      private:
        hexl::VectorSequence<ImageGeometry*>* defaultImageGeometry;
        hexl::Sequence<BrigImageGeometry>* imageGeometryProps;
        hexl::Sequence<BrigImageChannelOrder>* imageChannelOrders;
        hexl::Sequence<BrigImageChannelType>* imageChannelTypes;
        hexl::Sequence<BrigImageQuery>* imageQueryTypes;
        hexl::Sequence<BrigType>* imageAccessTypes;
        hexl::Sequence<unsigned>* imageArray;
        hexl::Sequence<uint32_t>* numberRW;
        hexl::Sequence<BrigType>* rdCoordTypes;

      public:
        ImageConfig(CoreConfig* cc);
        hexl::VectorSequence<hexl::ImageGeometry*>* DefaultImageGeometrySet() { return defaultImageGeometry; }
        hexl::Sequence<BrigImageGeometry>* ImageGeometryProps() { return imageGeometryProps; }
        hexl::Sequence<BrigImageChannelOrder>* ImageChannelOrders() { return imageChannelOrders; }
        hexl::Sequence<BrigImageChannelType>* ImageChannelTypes() { return imageChannelTypes; };
        hexl::Sequence<BrigImageQuery>* ImageQueryTypes() { return imageQueryTypes; };
        hexl::Sequence<BrigType>* ImageAccessTypes() { return imageAccessTypes; };
        hexl::Sequence<unsigned>* ImageArraySets() { return imageArray; };
        hexl::Sequence<uint32_t>* NumberOfRwImageHandles() { return numberRW; }
        hexl::Sequence<BrigType>* ImageRdCoordinateTypes() { return rdCoordTypes; }
      };

      class SamplerConfig : public ConfigBase {
      private:
        hexl::Sequence<BrigSamplerCoordNormalization>* samplerCoords;
        hexl::Sequence<BrigSamplerFilter>* samplerFilters;
        hexl::Sequence<BrigSamplerAddressing>* samplerAddressings;
        hexl::Sequence<SamplerParams*>* allSamplers;
        hexl::Sequence<BrigSamplerQuery>* samplerQueryTypes;

      public:
        SamplerConfig(CoreConfig* cc);

        hexl::Sequence<BrigSamplerCoordNormalization>* SamplerCoords() { return samplerCoords; }
        hexl::Sequence<BrigSamplerFilter>* SamplerFilters() { return samplerFilters; }
        hexl::Sequence<BrigSamplerAddressing>* SamplerAddressings() { return samplerAddressings; }
        hexl::Sequence<SamplerParams*>* All() { return allSamplers; }
        hexl::Sequence<BrigSamplerQuery>* SamplerQueryTypes() { return samplerQueryTypes; }
      };

      GridsConfig& Grids() { return grids; }
      SegmentsConfig& Segments() { return segments; }
      TypesConfig& Types() { return types; }
      VariablesConfig& Variables() { return variables; }
      QueuesConfig& Queues() { return queues; }
      MemoryConfig& Memory() { return memory; }
      ControlDirectivesConfig& Directives() { return directives; }
      ControlFlowConfig& ControlFlow() { return controlFlow; }
      FunctionsConfig& Functions() { return functions; }
      ImageConfig& Images() { return images; }
      SamplerConfig& Samplers() { return samplers; }

    private:
      GridsConfig grids;
      SegmentsConfig segments;
      TypesConfig types;
      VariablesConfig variables;
      QueuesConfig queues;
      MemoryConfig memory;
      ControlDirectivesConfig directives;
      ControlFlowConfig controlFlow;
      FunctionsConfig functions;
      ImageConfig images;
      SamplerConfig samplers;
    };

  }

  template <>
  inline void Print(const emitter::CoreConfig& cc, std::ostream& out) { /* cc.Print(out); */ }
}

#endif // CORE_CONFIG_HPP
