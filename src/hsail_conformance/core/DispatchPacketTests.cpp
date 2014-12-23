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

#include "DispatchPacketTests.hpp"
#include "HCTests.hpp"
#include "MObject.hpp"

using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

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

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

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
public:
  WorkitemAbsIdTest(Location codeLocation,
    Grid geometry, unsigned testDim,
    ControlDirectives directives)
    : DispatchPacketDimTest(codeLocation, geometry, testDim, directives) { }

  void ExpectedResults(Values* result) const {
    for(uint16_t z = 0; z < geometry->GridSize(2); z++)
      for(uint16_t y = 0; y < geometry->GridSize(1); y++)
        for(uint16_t x = 0; x < geometry->GridSize(0); x++) {
          Dim point(x,y,z);
          result->push_back(Value(MV_UINT32, geometry->WorkitemAbsId(point, testDim)));
        }
  }

  TypedReg Result() {
    return be.EmitWorkitemAbsId(false);
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

  BrigTypeX ResultType() const { return dest64 ? BRIG_TYPE_U64 : BRIG_TYPE_U32; }

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

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

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

class PacketIdTest : public DispatchPacketBaseTest {
public:
  PacketIdTest(Location codeLocation,
    Grid geometry,
    ControlDirectives directives)
    : DispatchPacketBaseTest(codeLocation, geometry, directives) { }

  BrigTypeX ResultType() const { return BRIG_TYPE_U64; }

  Value ExpectedResult() const {
    return Value(MV_UINT64, 0);
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
    
  BrigTypeX ResultType() const { return be.PointerType(); }
  
  Value ExpectedResult() const {
    return Value(MV_EXPR, S("signaldesc"));
  }

  TypedReg Result() {
    return be.EmitPacketCompletionSig();
  }
};

class BoundaryBaseTest : public DispatchPacketBaseTest {
private:
  bool dest64;

protected:
  static const uint64_t numBoundaryValues = 128;

public:
  BoundaryBaseTest(Location codeLocation_, Grid geometry,
    ControlDirectives directives, const bool dest64_ = false)
    : DispatchPacketBaseTest(codeLocation_, geometry, directives), dest64(dest64_) { }

  BrigTypeX ResultType() const { return dest64 ? BRIG_TYPE_U64 : BRIG_TYPE_U32; }

  void Name(std::ostream& out) const {
    DispatchPacketBaseTest::Name(out);
    out << "_" << (dest64  ? "64" : "32");
  }
  
  const uint64_t Boundary() const {
    return geometry->GridSize() - numBoundaryValues;
  }

  size_t OutputBufferSize() const {
    return numBoundaryValues;
  }

  void ExpectedResults(Values* result) const {
    for (uint64_t i = 0; i < numBoundaryValues; ++i) {
      result->push_back(ExpectedResult(i));
    }
  }

  void KernelCode() {
    TypedReg result64 = be.EmitWorkitemFlatAbsId(true);
    //Store condition
    TypedReg reg_c = be.AddTReg(BRIG_TYPE_B1);
    //cmp_ge c0, s0, n
    Operand boundary = be.Immed(BRIG_TYPE_U64, Boundary());
    be.EmitCmp(reg_c->Reg(), result64, boundary, BRIG_COMPARE_GE);
    SRef then = "@then";
    //cbr c0, @then
    be.EmitCbr(reg_c, then);
    SRef endif = "@endif";
    //br @endif
    be.EmitBr(endif);
     //@then:
    be.EmitLabel(then);
    TypedReg index = be.AddTReg(BRIG_TYPE_U64);
    //sub s1, s0, n
    be.EmitArith(BRIG_OPCODE_SUB, index, result64, boundary);  
    //store result
    PointerReg addrReg = be.AddAReg(BRIG_SEGMENT_GLOBAL);
     //ld_kernarg_align
    be.EmitLoad(output->Variable().segment(), addrReg, be.Address(output->Variable()));
    //mul_u64
    be.EmitArith(BRIG_OPCODE_MUL, index, index, be.Immed(BRIG_TYPE_U64, dest64 ? 8 : 4));
    //add_u64
    be.EmitArith(BRIG_OPCODE_ADD, addrReg, addrReg, index->Reg());
    //insert workitemXXXid instruction
    TypedReg result = Result();
    //st_global_align
    be.EmitStore(result, addrReg, 0, false);
    //@endif:
    be.EmitLabel(endif);
  }
};

class BoundaryDimTest : public BoundaryBaseTest {
protected:
  unsigned testDim;

public:
  BoundaryDimTest(Location codeLocation,
    Grid geometry, ControlDirectives directives,
    unsigned testDim_, bool dest64 = false)
    : BoundaryBaseTest(codeLocation, geometry, directives, dest64),
      testDim(testDim_) { }

  void Name(std::ostream& out) const {
    BoundaryBaseTest::Name(out); out << "_" << testDim;
  }
};

class WorkitemIdBoundaryTest : public BoundaryDimTest {
public:
  WorkitemIdBoundaryTest(Location codeLocation_,
    Grid geometry,
    ControlDirectives directives, unsigned testDim, bool dest64)
    : BoundaryDimTest(codeLocation_, geometry, directives, testDim, dest64) { }

  bool IsValid() const { return BoundaryBaseTest::IsValid() && IsResultType(BRIG_TYPE_U32); }
  
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
    return be.EmitWorkitemAbsId(ResultType() == BRIG_TYPE_U64 ? true : false);
  }
};

class WorkitemFlatIdBoundaryTest : public BoundaryBaseTest {
public:
  WorkitemFlatIdBoundaryTest(Location codeLocation_,
    Grid geometry,
    ControlDirectives directives, bool dest64)
    : BoundaryBaseTest(codeLocation_, geometry, directives, dest64) { }

  bool IsValid() const { return IsResultType(BRIG_TYPE_U32); }

  Value ExpectedResult(uint64_t i) const {
    Dim point = geometry->Point(geometry->GridSize() - numBoundaryValues + i);
    return Value(ResultValueType(), geometry->WorkitemFlatId(point));
  }
  
  TypedReg Result() {
    return be.EmitWorkitemFlatId();
  }
};

class WorkitemFlatAbsIdBoundaryTest : public BoundaryBaseTest {
public:
  WorkitemFlatAbsIdBoundaryTest(Location codeLocation_,
    Grid geometry,
    ControlDirectives directives, bool dest64)
    : BoundaryBaseTest(codeLocation_, geometry, directives, dest64) { }
  
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
  TestForEach<CurrentWorkgroupSizeTest>(ap, it, "dispatchpacket/currentworkgroupsize/basic", CodeLocations(), cc->Grids().All(), cc->Grids().Dimensions(), cc->Directives().GridGroupRelatedSets());
  TestForEach<CurrentWorkgroupSizeTest>(ap, it, "dispatchpacket/currentworkgroupsize/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<DimensionTest>(ap, it, "dispatchpacket/dim/basic", CodeLocations(), cc->Grids().DimensionSet(), cc->Directives().DimensionRelatedSets());

  TestForEach<GridGroupsTest>(ap, it, "dispatchpacket/gridgroups/basic", CodeLocations(), cc->Grids().All(), cc->Grids().Dimensions(), cc->Directives().GridGroupRelatedSets());
  TestForEach<GridGroupsTest>(ap, it, "dispatchpacket/gridgroups/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<GridSizeTest>(ap, it, "dispatchpacket/gridsize/basic", CodeLocations(), cc->Grids().All(), cc->Grids().Dimensions(), cc->Directives().GridSizeRelatedSets());
  //TestForEach<GridSizeBoundaryTest>(ap, it, "dispatchpacket/gridsize/boundary32", CodeLocations(), cc->Grids().Boundary32Set(), cc->Directives().GridSizeRelatedSets(), cc->Grids().Dimensions(), Bools::All());

  TestForEach<WorkgroupIdTest>(ap, it, "dispatchpacket/workgroupid/basic", CodeLocations(), cc->Grids().All(), cc->Grids().Dimensions(), cc->Directives().DimensionRelatedSets());
  TestForEach<WorkgroupIdTest>(ap, it, "dispatchpacket/workgroupid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<WorkgroupSizeTest>(ap, it, "dispatchpacket/workgroupsize/basic", CodeLocations(), cc->Grids().All(), cc->Grids().Dimensions(), cc->Directives().GridGroupRelatedSets());
  TestForEach<WorkgroupSizeTest>(ap, it, "dispatchpacket/workgroupsize/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<WorkitemAbsIdTest>(ap, it, "dispatchpacket/workitemabsid/basic", CodeLocations(), cc->Grids().All(), cc->Grids().Dimensions(), cc->Directives().WorkitemAbsIdRelatedSets());
  TestForEach<WorkitemAbsIdTest>(ap, it, "dispatchpacket/workitemabsid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());
  TestForEach<WorkitemAbsIdBoundaryTest>(ap, it, "dispatchpacket/workitemabsid/boundary24", CodeLocations(), cc->Grids().Boundary24Set(), cc->Directives().Boundary24WorkitemAbsIdRelatedSets(), cc->Grids().Dimensions(), Bools::All());

  TestForEach<WorkitemFlatAbsIdTest>(ap, it, "dispatchpacket/workitemflatabsid/basic", CodeLocations(), cc->Grids().All(), cc->Directives().WorkitemFlatAbsIdRelatedSets(), Bools::All());
  TestForEach<WorkitemFlatAbsIdTest>(ap, it, "dispatchpacket/workitemflatabsid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Directives().DegenerateRelatedSets(), Bools::All());
  TestForEach<WorkitemFlatAbsIdBoundaryTest>(ap, it, "dispatchpacket/workitemflatabsid/boundary32", CodeLocations(), cc->Grids().Boundary32Set(), cc->Directives().WorkitemFlatAbsIdRelatedSets(), Bools::All());
  TestForEach<WorkitemFlatAbsIdBoundaryTest>(ap, it, "dispatchpacket/workitemflatabsid/boundary24", CodeLocations(), cc->Grids().Boundary24Set(), cc->Directives().Boundary24WorkitemFlatAbsIdRelatedSets(), Bools::All());
  
  TestForEach<WorkitemFlatIdTest>(ap, it, "dispatchpacket/workitemflatid/basic", CodeLocations(), cc->Grids().All(), cc->Directives().WorkitemFlatIdRelatedSets());
  TestForEach<WorkitemFlatIdTest>(ap, it, "dispatchpacket/workitemflatid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Directives().DegenerateRelatedSets());
  TestForEach<WorkitemFlatIdBoundaryTest>(ap, it, "dispatchpacket/workitemflatid/boundary32", CodeLocations(), cc->Grids().Boundary32Set(), cc->Directives().WorkitemFlatIdRelatedSets(), Bools::All());
  TestForEach<WorkitemFlatIdBoundaryTest>(ap, it, "dispatchpacket/workitemflatid/boundary24", CodeLocations(), cc->Grids().Boundary24Set(), cc->Directives().Boundary24WorkitemFlatIdRelatedSets(), Bools::All());

  TestForEach<WorkitemIdTest>(ap, it, "dispatchpacket/workitemid/basic", CodeLocations(), cc->Grids().All(), cc->Grids().Dimensions(), cc->Directives().WorkitemIdRelatedSets());
  TestForEach<WorkitemIdTest>(ap, it, "dispatchpacket/workitemid/degenerate", CodeLocations(), cc->Grids().DegenerateSet(), cc->Grids().Dimensions(), cc->Directives().DegenerateRelatedSets());

  TestForEach<PacketIdTest>(ap, it, "dispatchpacket/packetid/basic", CodeLocations(), cc->Grids().All(), cc->Directives().NoneSets());
  TestForEach<PacketCompletionSigTest>(ap, it, "dispatchpacket/packetcompletionsig/basic", CodeLocations(), cc->Grids().All(), cc->Directives().NoneSets());
}

} // hsail_conformance
