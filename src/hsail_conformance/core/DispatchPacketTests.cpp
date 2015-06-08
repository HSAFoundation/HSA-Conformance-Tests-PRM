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

#include "DispatchPacketTests.hpp"
#include "HCTests.hpp"
#include "MObject.hpp"
#include "UtilTests.hpp"

using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;
using namespace hsail_conformance::utils;

namespace hsail_conformance {

class DispatchPacketBaseTest:  public Test {
protected:
  ControlDirectives directives;

public:
  DispatchPacketBaseTest(Location codeLocation, 
      Grid geometry,
      ControlDirectives directives_): Test(codeLocation, geometry),
      directives(directives_)
  {
    specList.Add(directives);
  }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '_' << directives;
  }
};

class DispatchPacketDimTest : public DispatchPacketBaseTest {
protected:
  unsigned testDim;

public:
  DispatchPacketDimTest(Location codeLocation,
    Grid geometry, unsigned testDim_,
    ControlDirectives directives)
    : DispatchPacketBaseTest(codeLocation, geometry, directives),
      testDim(testDim_) { }

  void Name(std::ostream& out) const {
    DispatchPacketBaseTest::Name(out); out << "_" << testDim;
  }
};

class CurrentWorkgroupSizeTest : public DispatchPacketDimTest {
public:
  CurrentWorkgroupSizeTest(Location codeLocation,
    Grid geometry, unsigned testDim,
    ControlDirectives directives)
    : DispatchPacketDimTest(codeLocation, geometry, testDim, directives) { }

  bool IsValid() const {
    return !geometry->isPartial() || !directives->Has(BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS);    
  }

  void ExpectedResults(Values* result) const {
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++) {
          Dim point(x,y,z);
          result->push_back(Value(MV_UINT32, geometry->CurrentWorkgroupSize(point, testDim)));
        }
  }

  TypedReg Result() {
    return be.EmitCurrentWorkgroupSize(testDim);
  }
};

class DimensionTest : public DispatchPacketBaseTest {
public:
  DimensionTest(Location codeLocation,
    Grid geometry,
    ControlDirectives directives)
    : DispatchPacketBaseTest(codeLocation, geometry, directives) { }

  TypedReg Result() {
    return be.EmitDim();
  }

  Value ExpectedResult() const { return Value(MV_UINT32, U32(geometry->Dimensions())); }
};

class GridGroupsTest : public DispatchPacketDimTest {
public:
  GridGroupsTest(Location codeLocation,
    Grid geometry, unsigned testDim,
    ControlDirectives directives)
    : DispatchPacketDimTest(codeLocation, geometry, testDim, directives) { }

  Value ExpectedResult() const { return Value(MV_UINT32, geometry->GridGroups(testDim)); }

  bool IsValid() const {
    return !geometry->isPartial() || !directives->Has(BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS);
  }

  TypedReg Result() {
    return be.EmitGridGroups(testDim);
  }
};

class GridSizeTest : public DispatchPacketDimTest {
public:
  GridSizeTest(Location codeLocation,
    Grid geometry, unsigned testDim,
    ControlDirectives directives)
    : DispatchPacketDimTest(codeLocation, geometry, testDim, directives) { }

  Value ExpectedResult() const { return Value(MV_UINT32, geometry->GridSize(testDim)); }

  TypedReg Result() {
    return be.EmitGridSize(testDim);
  }
};

class WorkgroupIdTest : public DispatchPacketDimTest {
public:
  WorkgroupIdTest(Location codeLocation,
    Grid geometry, unsigned testDim,
    ControlDirectives directives)
    : DispatchPacketDimTest(codeLocation, geometry, testDim, directives) { }

  void ExpectedResults(Values* result) const {
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++) {
          Dim point(x,y,z);
          result->push_back(Value(MV_UINT32, geometry->WorkgroupId(point, testDim)));
        }
  }

  TypedReg Result() {
    return be.EmitWorkgroupId(testDim);
  }
};

class WorkgroupSizeTest : public DispatchPacketDimTest {
public:
  WorkgroupSizeTest(Location codeLocation,
    Grid geometry, unsigned testDim,
    ControlDirectives directives)
    : DispatchPacketDimTest(codeLocation, geometry, testDim, directives) { }

  bool IsValid() const {
    return !geometry->isPartial() || !directives->Has(BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS);    
  }

  Value ExpectedResult() const {
    return Value(MV_UINT32, geometry->WorkgroupSize(testDim));
  }

  TypedReg Result() {
    return be.EmitWorkgroupSize(testDim);
  }
};

class WorkitemIdTest : public DispatchPacketDimTest {
public:
  WorkitemIdTest(Location codeLocation,
    Grid geometry, unsigned testDim,
    ControlDirectives directives)
    : DispatchPacketDimTest(codeLocation, geometry, testDim, directives) { }
    
  void ExpectedResults(Values* result) const {
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++) {
          Dim point(x,y,z);
          result->push_back(Value(MV_UINT32, geometry->WorkitemId(point, testDim)));
        }
  }

  TypedReg Result() {
    return be.EmitWorkitemId(testDim);
  }
};

class WorkitemAbsIdTest : public DispatchPacketDimTest  {
private:
  bool dest64;

public:
  WorkitemAbsIdTest(Location codeLocation,
    Grid geometry, unsigned testDim,
    ControlDirectives directives, bool dest64_)
    : DispatchPacketDimTest(codeLocation, geometry, testDim, directives),
      dest64(dest64_) { }

  void Name(std::ostream& out) const {
    DispatchPacketDimTest::Name(out);
    out << "_" << (dest64  ? "64" : "32");
  }

  BrigType ResultType() const { return dest64 ? BRIG_TYPE_U64 : BRIG_TYPE_U32; }

  void ExpectedResults(Values* result) const {
    ValueType type = ResultValueType();
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++) {
          Dim point(x,y,z);
          result->push_back(Value(type, geometry->WorkitemAbsId(point, testDim)));
        }
  }

  TypedReg Result() {
    return be.EmitWorkitemAbsId(testDim, dest64);
  }
};

class WorkitemFlatAbsIdTest : public DispatchPacketBaseTest {
private:
  bool dest64;

public:
  WorkitemFlatAbsIdTest(Location codeLocation,
    Grid geometry,
    ControlDirectives directives, const bool dest64_)
    : DispatchPacketBaseTest(codeLocation, geometry, directives), dest64(dest64_) { }

  BrigType ResultType() const { return dest64 ? BRIG_TYPE_U64 : BRIG_TYPE_U32; }

  void Name(std::ostream& out) const {
    DispatchPacketBaseTest::Name(out);
    out << "_" << (dest64  ? "64" : "32");
  }

  void ExpectedResults(Values* result) const {
    for (uint64_t i = 0; i < geometry->GridSize(); ++i)
      result->push_back(Value((dest64 ?  MV_UINT64 : MV_UINT32), i));
  }

  TypedReg Result() {
    return be.EmitWorkitemFlatAbsId(ResultType() == BRIG_TYPE_U64 ? true : false);
  }
};

class WorkitemFlatIdTest : public DispatchPacketBaseTest {
public:
  WorkitemFlatIdTest(Location codeLocation,
    Grid geometry,
    ControlDirectives directives)
    : DispatchPacketBaseTest(codeLocation, geometry, directives) { }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  void ExpectedResults(Values* result) const {
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++) {
          result->push_back(Value(MV_UINT32, geometry->WorkitemFlatId(Dim(x,y,z))));
        }
  }

  TypedReg Result() {
    return be.EmitWorkitemFlatId();
  }
};

class CurrentWorkitemFlatIdTest : public DispatchPacketBaseTest {
public:
  CurrentWorkitemFlatIdTest(Location codeLocation,
    Grid geometry,
    ControlDirectives directives)
    : DispatchPacketBaseTest(codeLocation, geometry, directives) { }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  void ExpectedResults(Values* result) const {
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++) {
          result->push_back(Value(MV_UINT32, geometry->CurrentWorkitemFlatId(Dim(x,y,z))));
        }
  }

  TypedReg Result() {
    return be.EmitCurrentWorkitemFlatId();
  }
};

class PacketIdTest : public DispatchPacketBaseTest {
public:
  PacketIdTest(Location codeLocation,
    Grid geometry,
    ControlDirectives directives)
    : DispatchPacketBaseTest(codeLocation, geometry, directives) { }

  BrigType ResultType() const override { return BRIG_TYPE_U64; }

  Value ExpectedResult() const {
    std::string *name = new std::string(dispatch->StrId());
    *name += ".dispatchpacketid";
    return Value(MV_EXPR, S(name->c_str()));
  }
  
  TypedReg Result() {
    return be.EmitPacketId();
  }
};

class PacketCompletionSigTest : public DispatchPacketBaseTest {
public:
  PacketCompletionSigTest(Location codeLocation,
    Grid geometry,
    ControlDirectives directives)
    : DispatchPacketBaseTest(codeLocation, geometry, directives) { }
    
  BrigType ResultType() const { return BRIG_TYPE_U64; }
  
  Value ExpectedResult() const {
    std::string *name = new std::string(dispatch->StrId());
    *name += ".packetcompletionsig";
    return Value(MV_EXPR, S(name->c_str()));
  }

  TypedReg Result() {
    return be.EmitPacketCompletionSig();
  }
};

class DispatchBoundaryBaseTest: public BoundaryTest {
private:
  bool dest64;

protected:
  static const uint64_t numBoundaryValues = 128;
  ControlDirectives directives;

public:
  DispatchBoundaryBaseTest(Location codeLocation_, Grid geometry_,
    ControlDirectives directives_, const bool dest64_ = false)
    : BoundaryTest(numBoundaryValues, codeLocation_, geometry_), dest64(dest64_), directives(directives_)
  {  
    specList.Add(directives);
  }

  BrigType ResultType() const { return dest64 ? BRIG_TYPE_U64 : BRIG_TYPE_U32; }

  void Name(std::ostream& out) const {
    out << CodeLocationString() << '_' << geometry << '_' << directives
        << "_" << (dest64  ? "64" : "32");
  }
};

class BoundaryDimTest : public DispatchBoundaryBaseTest {
protected:
  unsigned testDim;

public:
  BoundaryDimTest(Location codeLocation,
    Grid geometry, ControlDirectives directives,
    unsigned testDim_, bool dest64 = false)
    : DispatchBoundaryBaseTest(codeLocation, geometry, directives, dest64),
      testDim(testDim_) { }

  void Name(std::ostream& out) const {
    DispatchBoundaryBaseTest::Name(out); out << "_" << testDim;
  }
};

class WorkitemIdBoundaryTest : public BoundaryDimTest {
public:
  WorkitemIdBoundaryTest(Location codeLocation_,
    Grid geometry,
    ControlDirectives directives, unsigned testDim, bool dest64)
    : BoundaryDimTest(codeLocation_, geometry, directives, testDim, dest64) { }

  bool IsValid() const { return BoundaryDimTest::IsValid() && IsResultType(BRIG_TYPE_U32); }
  
  Value ExpectedResult(uint64_t i) const {
    Dim point = geometry->Point(geometry->GridSize() - numBoundaryValues + i);
    return Value(MV_UINT32, geometry->WorkitemId(point, testDim));
  }
  
  TypedReg Result() {
    return be.EmitWorkitemId(testDim);
  }
};

class WorkitemAbsIdBoundaryTest : public BoundaryDimTest {
public:
  WorkitemAbsIdBoundaryTest(Location codeLocation_,
    Grid geometry,
    ControlDirectives directives, unsigned testDim, bool dest64)
    : BoundaryDimTest(codeLocation_, geometry, directives, testDim, dest64) { }

  Value ExpectedResult(uint64_t i) const {
    Dim point = geometry->Point(geometry->GridSize() - numBoundaryValues + i);
    return Value(ResultValueType(), geometry->WorkitemAbsId(point, testDim));
  }
  
  TypedReg Result() {
    return be.EmitWorkitemAbsId(testDim, ResultType() == BRIG_TYPE_U64 ? true : false);
  }
};

class WorkitemFlatIdBoundaryTest : public DispatchBoundaryBaseTest {
public:
  WorkitemFlatIdBoundaryTest(Location codeLocation_,
    Grid geometry,
    ControlDirectives directives, bool dest64)
    : DispatchBoundaryBaseTest(codeLocation_, geometry, directives, dest64) { }

  bool IsValid() const { return IsResultType(BRIG_TYPE_U32); }

  Value ExpectedResult(uint64_t i) const {
    Dim point = geometry->Point(geometry->GridSize() - numBoundaryValues + i);
    return Value(ResultValueType(), geometry->WorkitemFlatId(point));
  }
  
  TypedReg Result() {
    return be.EmitWorkitemFlatId();
  }
};

class WorkitemFlatAbsIdBoundaryTest : public DispatchBoundaryBaseTest {
public:
  WorkitemFlatAbsIdBoundaryTest(Location codeLocation_,
    Grid geometry,
    ControlDirectives directives, bool dest64)
    : DispatchBoundaryBaseTest(codeLocation_, geometry, directives, dest64) { }
  
  Value ExpectedResult(uint64_t i) const {
    /*
    Dim point = geometry->Point(geometry->GridSize() - numBoundaryValues + i);
    return Value(ResultValueType(), geometry->WorkitemFlatAbsId(point));
    */
    return Value(ResultValueType(), geometry->GridSize() - numBoundaryValues + i);
  }

  TypedReg Result() {
    return be.EmitWorkitemFlatAbsId(ResultType() == BRIG_TYPE_U64 ? true : false);
  }
};

class GridSizeBoundaryTest : public BoundaryDimTest {
public:
  GridSizeBoundaryTest(Location codeLocation_,
    Grid geometry,
    ControlDirectives directives, unsigned testDim, bool dest64_)
    : BoundaryDimTest(codeLocation_, geometry, directives, testDim, dest64_) { }
  
  Value ExpectedResult(uint64_t i) const {
    return Value(ResultValueType(), geometry->GridSize(testDim));
  }

  TypedReg Result() {
    return be.EmitGridSize(testDim);
  }
};

void DispatchPacketOperationsTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<CurrentWorkgroupSizeTest>(ap, it, "dispatchpacket/currentworkgroupsize/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Grids().Dimensions(), cc->Directives().GridGroupRelatedSets());
  TestForEach<CurrentWorkgroupSizeTest>(ap, it, "dispatchpacket/currentworkgroupsize/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<DimensionTest>(ap, it, "dispatchpacket/dim/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Directives().DimensionRelatedSets());

  TestForEach<GridGroupsTest>(ap, it, "dispatchpacket/gridgroups/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Grids().Dimensions(), cc->Directives().GridGroupRelatedSets());
  TestForEach<GridGroupsTest>(ap, it, "dispatchpacket/gridgroups/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<GridSizeTest>(ap, it, "dispatchpacket/gridsize/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Grids().Dimensions(), cc->Directives().GridSizeRelatedSets());
  //TestForEach<GridSizeBoundaryTest>(ap, it, "dispatchpacket/gridsize/boundary32", CodeLocations(), cc->Grids().Boundary32Set(), cc->Directives().GridSizeRelatedSets(), cc->Grids().Dimensions(), Bools::All());

  TestForEach<WorkgroupIdTest>(ap, it, "dispatchpacket/workgroupid/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Grids().Dimensions(), cc->Directives().DimensionRelatedSets());
  TestForEach<WorkgroupIdTest>(ap, it, "dispatchpacket/workgroupid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<WorkgroupSizeTest>(ap, it, "dispatchpacket/workgroupsize/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Grids().Dimensions(), cc->Directives().GridGroupRelatedSets());
  TestForEach<WorkgroupSizeTest>(ap, it, "dispatchpacket/workgroupsize/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<WorkitemAbsIdTest>(ap, it, "dispatchpacket/workitemabsid/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Grids().Dimensions(), cc->Directives().WorkitemAbsIdRelatedSets(), Bools::All());
  TestForEach<WorkitemAbsIdTest>(ap, it, "dispatchpacket/workitemabsid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets(), Bools::All());
  TestForEach<WorkitemAbsIdBoundaryTest>(ap, it, "dispatchpacket/workitemabsid/boundary32", CodeLocations(), cc->Grids().Boundary32Set(), cc->Directives().WorkitemAbsIdRelatedSets(), cc->Grids().Dimensions(), Bools::All());
  TestForEach<WorkitemAbsIdBoundaryTest>(ap, it, "dispatchpacket/workitemabsid/boundary24", CodeLocations(), cc->Grids().Boundary24Set(), cc->Directives().Boundary24WorkitemAbsIdRelatedSets(), cc->Grids().Dimensions(), Bools::All());

  TestForEach<WorkitemFlatAbsIdTest>(ap, it, "dispatchpacket/workitemflatabsid/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Directives().WorkitemFlatAbsIdRelatedSets(), Bools::All());
  TestForEach<WorkitemFlatAbsIdTest>(ap, it, "dispatchpacket/workitemflatabsid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Directives().DegenerateRelatedSets(), Bools::All());
  TestForEach<WorkitemFlatAbsIdBoundaryTest>(ap, it, "dispatchpacket/workitemflatabsid/boundary32", CodeLocations(), cc->Grids().Boundary32Set(), cc->Directives().WorkitemFlatAbsIdRelatedSets(), Bools::All());
  TestForEach<WorkitemFlatAbsIdBoundaryTest>(ap, it, "dispatchpacket/workitemflatabsid/boundary24", CodeLocations(), cc->Grids().Boundary24Set(), cc->Directives().Boundary24WorkitemFlatAbsIdRelatedSets(), Bools::All());
  
  TestForEach<WorkitemFlatIdTest>(ap, it, "dispatchpacket/workitemflatid/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Directives().WorkitemFlatIdRelatedSets());
  TestForEach<WorkitemFlatIdTest>(ap, it, "dispatchpacket/workitemflatid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Directives().DegenerateRelatedSets());
  TestForEach<WorkitemFlatIdBoundaryTest>(ap, it, "dispatchpacket/workitemflatid/boundary32", CodeLocations(), cc->Grids().Boundary32Set(), cc->Directives().WorkitemFlatIdRelatedSets(), Bools::All());
  TestForEach<WorkitemFlatIdBoundaryTest>(ap, it, "dispatchpacket/workitemflatid/boundary24", CodeLocations(), cc->Grids().Boundary24Set(), cc->Directives().Boundary24WorkitemFlatIdRelatedSets(), Bools::All());

  TestForEach<CurrentWorkitemFlatIdTest>(ap, it, "dispatchpacket/currentworkitemflatid/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Directives().WorkitemFlatIdRelatedSets());
  TestForEach<CurrentWorkitemFlatIdTest>(ap, it, "dispatchpacket/currentworkitemflatid/partial", CodeLocations(), cc->Grids().PartialSet(), cc->Directives().WorkitemFlatIdRelatedSets());
  TestForEach<CurrentWorkitemFlatIdTest>(ap, it, "dispatchpacket/currentworkitemflatid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Directives().DegenerateRelatedSets());

  TestForEach<WorkitemIdTest>(ap, it, "dispatchpacket/workitemid/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Grids().Dimensions(), cc->Directives().WorkitemIdRelatedSets());
  TestForEach<WorkitemIdTest>(ap, it, "dispatchpacket/workitemid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<PacketIdTest>(ap, it, "dispatchpacket/packetid/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Directives().NoneSets());
  TestForEach<PacketCompletionSigTest>(ap, it, "dispatchpacket/packetcompletionsig/basic", CodeLocations(), cc->Grids().SimpleSet(), cc->Directives().NoneSets());
}

} // hsail_conformance
