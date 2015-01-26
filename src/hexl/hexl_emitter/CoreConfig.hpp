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

#ifndef CORE_CONFIG_HPP
#define CORE_CONFIG_HPP

#include "Brig.h"
#include "Arena.hpp"
#include "MObject.hpp"
#include "Sequence.hpp"
#include "EmitterCommon.hpp"
#include <sstream>
#include <cassert>

#define BRIG_SEGMENT_MAX Brig::BRIG_SEGMENT_EXTSPACE0

namespace hexl {

  namespace emitter {

    enum EndiannessConfig {
      ENDIANNESS_LITTLE,
      ENDIANNESS_BIG
    };

    class CoreConfig {
    private:
      Arena* ap;
      Brig::BrigVersion32_t majorVersion;
      Brig::BrigVersion32_t minorVersion;
      Brig::BrigMachineModel8_t model;
      Brig::BrigProfile8_t profile;
      uint32_t wavesize;

    public:
      static const char *CONTEXT_KEY;

      static CoreConfig* Get(hexl::Context *context) { return context->Get<CoreConfig*>(CONTEXT_KEY); }

      CoreConfig(Brig::BrigVersion32_t majorVersion_ = Brig::BRIG_VERSION_HSAIL_MAJOR,
                 Brig::BrigVersion32_t minorVersion_ = Brig::BRIG_VERSION_HSAIL_MINOR,
                 Brig::BrigMachineModel8_t model_ = (sizeof(void *) == 8 ? Brig::BRIG_MACHINE_LARGE : Brig::BRIG_MACHINE_SMALL),
                 Brig::BrigProfile8_t profile_ = Brig::BRIG_PROFILE_FULL);

      Arena* Ap() { return ap; }
      Brig::BrigVersion32_t MajorVersion() const { return majorVersion; }
      Brig::BrigVersion32_t MinorVersion() const { return minorVersion; }
      Brig::BrigMachineModel8_t Model() const { return model; }
      Brig::BrigProfile8_t Profile() const { return profile; }
      uint32_t Wavesize() const { return wavesize; }
      EndiannessConfig Endianness() { return ENDIANNESS_LITTLE; }

      bool IsLarge() const {
        switch (model) {
        case Brig::BRIG_MACHINE_LARGE: return true;
        case Brig::BRIG_MACHINE_SMALL: return false;
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
        hexl::VectorSequence<uint32_t> dimensions;
        hexl::GridGeometry defaultGeometry, trivialGeometry;
        hexl::Sequence<hexl::Grid> *defaultGeometrySet, *trivialGeometrySet;
        hexl::VectorSequence<hexl::Grid> *simple, *degenerate, *dimension, *boundary24, *boundary32, *severalwaves, *severalwavesingroup;

      public:
        GridsConfig(CoreConfig* cc);

        hexl::Sequence<uint32_t>* Dimensions() { return &dimensions; }

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
        hexl::Sequence<hexl::Grid>* SeveralWavesInGroupSet() { return severalwavesingroup; }
      };

      class SegmentsConfig : public ConfigBase {
      private:
        hexl::Sequence<Brig::BrigSegment>* all;
        hexl::Sequence<Brig::BrigSegment>* variable;
        hexl::Sequence<Brig::BrigSegment>* atomic;
        hexl::Sequence<Brig::BrigSegment>* initializable;
        hexl::Sequence<Brig::BrigSegment>* singleList[BRIG_SEGMENT_MAX];

      public:
        SegmentsConfig(CoreConfig* cc);

        bool CanStore(Brig::BrigSegment8_t segment);
        bool HasFlatAddress(Brig::BrigSegment8_t segment);
        bool HasAddress(Brig::BrigSegment8_t segment);
        bool CanPassAddressToKernel(Brig::BrigSegment8_t segment);

        hexl::Sequence<Brig::BrigSegment>* All() { return all; }
        hexl::Sequence<Brig::BrigSegment>* Variable() { return variable; }
        hexl::Sequence<Brig::BrigSegment>* Atomic() { return atomic; }
        hexl::Sequence<Brig::BrigSegment>* InitializableSegments() { return initializable; }
        hexl::Sequence<Brig::BrigSegment>* Single(Brig::BrigSegment segment);
      };

      class TypesConfig : public ConfigBase {
      private:
        hexl::Sequence<Brig::BrigTypeX> *compound, *compoundIntegral, *compoundFloating, *packed, *packed128;

      public:
        TypesConfig(CoreConfig* cc);

        hexl::Sequence<Brig::BrigTypeX>* Compound() { return compound; }
        hexl::Sequence<Brig::BrigTypeX>* Packed() { return packed; }
        hexl::Sequence<Brig::BrigTypeX>* Packed128Bit() { return packed128; }
        const hexl::Sequence<Brig::BrigTypeX>* CompoundIntegral() { return compoundIntegral; }
        const hexl::Sequence<Brig::BrigTypeX>* CompoundFloating() { return compoundFloating; }
      };

      class VariablesConfig : public ConfigBase {
      private:
        hexl::Sequence<VariableSpec>* bySegmentType;
        hexl::Sequence<VariableSpec>* byTypeAlign[BRIG_SEGMENT_MAX];
        hexl::Sequence<VariableSpec>* byTypeDimensionAlign[BRIG_SEGMENT_MAX];
        hexl::Sequence<uint64_t> *dim0, *dims, *initializerDims;
        hexl::Sequence<Location> *autoLocation, *initializerLocations;
        hexl::VectorSequence<Brig::BrigAlignment> allAlignment;

      public:
        VariablesConfig(CoreConfig* cc);

        hexl::Sequence<VariableSpec>* BySegmentType() { return bySegmentType; }
        hexl::Sequence<VariableSpec>* ByTypeAlign(Brig::BrigSegment segment) { return byTypeAlign[segment]; }
        hexl::Sequence<VariableSpec>* ByTypeDimensionAlign(Brig::BrigSegment segment) { return byTypeDimensionAlign[segment]; }

        hexl::Sequence<uint64_t>* Dim0() { return dim0; }
        hexl::Sequence<uint64_t>* Dims() { return dims; }
        hexl::Sequence<uint64_t>* InitializerDims() { return initializerDims; }

        hexl::Sequence<Location>* AutoLocation() { return autoLocation; }
        hexl::Sequence<Location>* InitializerLocations() { return initializerLocations; }

        hexl::Sequence<Brig::BrigAlignment>* AllAlignment() { return &allAlignment; }
      };

      class QueuesConfig : public ConfigBase {
      private:
        hexl::Sequence<UserModeQueueType>* types;
        hexl::Sequence<Brig::BrigSegment>* segments;
        hexl::Sequence<Brig::BrigOpcode>* ldOpcodes;
        hexl::Sequence<Brig::BrigOpcode>* addCasOpcodes;
        hexl::Sequence<Brig::BrigOpcode>* stOpcodes;
        hexl::Sequence<Brig::BrigMemoryOrder>* ldMemoryOrders;
        hexl::Sequence<Brig::BrigMemoryOrder>* addCasMemoryOrders;
        hexl::Sequence<Brig::BrigMemoryOrder>* stMemoryOrders;

      public:
        QueuesConfig(CoreConfig* cc);

        hexl::Sequence<UserModeQueueType>* Types() { return types; }
        hexl::Sequence<Brig::BrigSegment>* Segments() { return segments; }
        hexl::Sequence<Brig::BrigOpcode>* LdOpcodes() { return ldOpcodes; }
        hexl::Sequence<Brig::BrigOpcode>* AddCasOpcodes() { return addCasOpcodes; }
        hexl::Sequence<Brig::BrigOpcode>* StOpcodes() { return stOpcodes; }
        hexl::Sequence<Brig::BrigMemoryOrder>* LdMemoryOrders() { return ldMemoryOrders; }
        hexl::Sequence<Brig::BrigMemoryOrder>* AddCasMemoryOrders() { return addCasMemoryOrders; }
        hexl::Sequence<Brig::BrigMemoryOrder>* StMemoryOrders() { return stMemoryOrders; }
      };

      class MemoryConfig : public ConfigBase {
      private:
        hexl::Sequence<Brig::BrigMemoryOrder> *allMemoryOrders, *signalSendMemoryOrders, *signalWaitMemoryOrders;
        hexl::Sequence<Brig::BrigMemoryScope> *allMemoryScopes; 
        hexl::Sequence<Brig::BrigAtomicOperation> *allAtomics, *signalSendAtomics, *signalWaitAtomics;
        hexl::Sequence<Brig::BrigSegment> *memfenceSegments;

      public:
        MemoryConfig(CoreConfig* cc);
        hexl::Sequence<Brig::BrigMemoryOrder>* AllMemoryOrders() { return allMemoryOrders; }
        hexl::Sequence<Brig::BrigMemoryOrder>* SignalSendMemoryOrders() { return signalSendMemoryOrders; }
        hexl::Sequence<Brig::BrigMemoryOrder>* SignalWaitMemoryOrders() { return signalWaitMemoryOrders; }
        hexl::Sequence<Brig::BrigMemoryScope>* AllMemoryScopes() { return allMemoryScopes; }
        hexl::Sequence<Brig::BrigAtomicOperation>* AllAtomics() { return allAtomics; }
        hexl::Sequence<Brig::BrigAtomicOperation>* SignalSendAtomics() { return signalSendAtomics; }
        hexl::Sequence<Brig::BrigAtomicOperation>* SignalWaitAtomics() { return signalWaitAtomics; }
        hexl::Sequence<Brig::BrigSegment>* MemfenceSegments() { return memfenceSegments; }
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
        hexl::Sequence<Brig::BrigKinds>* pragmaOperandTypes;
        hexl::Sequence<uint32_t>* validExceptionNumbers;
        hexl::Sequence<Brig::BrigControlDirective> *exceptionDirectives, *geometryDirectives;

        static ControlDirectives Array(Arena* ap, const Brig::BrigControlDirective *values, size_t count);
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

        hexl::Sequence<Brig::BrigKinds>* PragmaOperandTypes() { return pragmaOperandTypes; }
        
        hexl::Sequence<uint32_t>* ValidExceptionNumbers() { return validExceptionNumbers; }
        hexl::Sequence<Brig::BrigControlDirective>* ExceptionDirectives() { return exceptionDirectives; }
        hexl::Sequence<Brig::BrigControlDirective>* GeometryDirectives() { return geometryDirectives; }
      };

      class ControlFlowConfig : public ConfigBase {
      private:
        hexl::Sequence<Brig::BrigWidth> *allWidths;
        hexl::VectorSequence<Brig::BrigWidth> *workgroupWidths;
        hexl::Sequence<ConditionInput>* conditionInputs;
        hexl::Sequence<Condition>* binaryConditions;
        hexl::Sequence<Brig::BrigTypeX>* sbrTypes;
        hexl::Sequence<Condition>* switchConditions;

      public:
        ControlFlowConfig(CoreConfig* cc);
        hexl::Sequence<Brig::BrigWidth>* AllWidths() { return allWidths; }
        hexl::Sequence<Brig::BrigWidth>* WorkgroupWidths() { return workgroupWidths; }
        hexl::Sequence<ConditionInput>* ConditionInputs() { return conditionInputs; }
        hexl::Sequence<Condition>* BinaryConditions() { return binaryConditions; }
        hexl::Sequence<Brig::BrigTypeX>* SbrTypes() { return sbrTypes; }
        hexl::Sequence<Condition>* SwitchConditions() { return switchConditions; }
      };

      GridsConfig& Grids() { return grids; }
      SegmentsConfig& Segments() { return segments; }
      TypesConfig& Types() { return types; }
      VariablesConfig& Variables() { return variables; }
      QueuesConfig& Queues() { return queues; }
      MemoryConfig& Memory() { return memory; }
      ControlDirectivesConfig& Directives() { return directives; }
      ControlFlowConfig& ControlFlow() { return controlFlow; }

    private:
      GridsConfig grids;
      SegmentsConfig segments;
      TypesConfig types;
      VariablesConfig variables;
      QueuesConfig queues;
      MemoryConfig memory;
      ControlDirectivesConfig directives;
      ControlFlowConfig controlFlow;
    };

  }
}

#endif // CORE_CONFIG_HPP
