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

#include "AddressTests.hpp"
#include "BrigEmitter.hpp"
#include "BasicHexlTests.hpp"
#include "HCTests.hpp"
#include <sstream>

using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class StofNullTest : public Test {
private:
  BrigSegment8_t segment;

public:
  StofNullTest(BrigSegment8_t segment_)
    : segment(segment_) { }

  void Name(std::ostream& out) const {
    out << "stof/null/" << segment2str(segment);
  }

  bool IsValid() const {
    return segment != BRIG_SEGMENT_FLAT && cc->Segments().HasNullptr(segment);
  }

protected:
  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const { return Value(MV_UINT32, 1); }

  TypedReg Result() {
    // Emit nullptr for segment.
    PointerReg segNull = be.AddAReg(segment);
    be.EmitNullPtr(segNull);
    // Convert segment nullptr to flat using stof.
    PointerReg convNull = be.AddAReg(BRIG_SEGMENT_FLAT);
    be.EmitStof(convNull, segNull);
    // Emit nullptr for flat segment.
    PointerReg flatNull = be.AddAReg(BRIG_SEGMENT_FLAT);
    be.EmitNullPtr(flatNull);
    // Compare and return the result.
    TypedReg c = be.AddCTReg();
    be.EmitCmp(c->Reg(), convNull, flatNull, BRIG_COMPARE_EQ);
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCvt(result, c);
    return result;
  }
};

class StofIdentityTest : public Test {
private:
  AddressSpec addressSpec;
  bool segmentStore;
  bool nonull;

public:
  StofIdentityTest(const AddressSpec& addressSpec_,
    bool segmentStore_, bool nonull_)
    : addressSpec(addressSpec_), segmentStore(segmentStore_), nonull(nonull_) { }

  void Name(std::ostream& out) const {
    out << "stof/identity/" << addressSpec
        << "_" << (segmentStore ? "s" : "f") << (nonull ? "n" : "");
  }

protected:
  BrigType ResultType() const { return addressSpec->Type(); }

  Value ExpectedResult() const { return be.GenerateTestValue(addressSpec->Type()); }

  DirectiveVariable in;

  void SetupDispatch(DispatchSetup* dsetup) {
    Test::SetupDispatch(dsetup);
    unsigned id = dsetup->MSetup().Count();
    MObject* in = NewMValue(id++, "Input", MEM_GLOBAL, addressSpec->VType(), U64(42));
    dsetup->MSetup().Add(in);
    dsetup->MSetup().Add(NewMValue(id++, "Input (arg)", MEM_KERNARG, MV_REF, R(in->Id())));
  }

  void KernelArguments() {
    Test::KernelArguments();
    in = be.EmitVariableDefinition(be.IName(), BRIG_SEGMENT_KERNARG, be.PointerType());
  }

  TypedReg Result() {
    PointerReg inp = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    be.EmitLoad(in.segment(), inp, be.Address(in));
    TypedReg data = be.AddTReg(addressSpec->Type());
    be.EmitLoad(data, inp);
//    PointerReg segmentAddr = addressSpec->AddAReg();
//    addressSpec->EmitLda(segmentAddr);
    PointerReg flatAddr = be.AddAReg(BRIG_SEGMENT_FLAT);
//    be.EmitStof(flatAddr, segmentAddr, nonull);
    if (segmentStore) {
      // Segment store / flat load
  //    be.EmitStore(data, segmentAddr);
      be.EmitLoad(data, flatAddr);
    } else {
      // Flat store / segment load
      be.EmitStore(data, flatAddr);
    //  be.EmitLoad(data, segmentAddr);
    }
    return data;
  }

  bool IsValid() const {
    return Test::IsValid() /*&&
           addressSpec->Segment() != BRIG_SEGMENT_FLAT &&
           cc->Segments().CanStore(addressSpec->Segment()) &&
           cc->Segments().HasFlatAddress(addressSpec->Segment()) */;
  }

};


class FtosNullTest : public Test {
private:
  BrigSegment8_t segment;

public:
  FtosNullTest(BrigSegment8_t segment_)
    : segment(segment_) { }

  void Name(std::ostream& out) const {
    out << "ftos/null/" << segment2str(segment);
  }

  bool IsValid() const {
    return segment != BRIG_SEGMENT_FLAT && cc->Segments().HasNullptr(segment);
  }

protected:
  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const { return Value(MV_UINT32, 1); }

  TypedReg Result() {
    // Emit nullptr for flat segment.
    PointerReg flatNull = be.AddAReg(BRIG_SEGMENT_FLAT);
    be.EmitNullPtr(flatNull);
    // Convert flat nullptr to segment using ftos.
    PointerReg convNull = be.AddAReg(segment);
    be.EmitFtos(convNull, flatNull);
    // Emit segment nullptr.
    PointerReg segNull = be.AddAReg(segment);
    be.EmitNullPtr(segNull);
    // Compare and return the result.
    TypedReg c = be.AddCTReg();
    be.EmitCmp(c->Reg(), convNull, segNull, BRIG_COMPARE_EQ);
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCvt(result, c);
    return result;
  }
};

/*
class FtosIdentityTest : public Test {
private:
  AddressSpec addressSpec;
  bool segmentStore;
  bool nonull;

public:
  FtosIdentityTest(const AddressSpec& addressSpec_,
    bool segmentStore_, bool nonull_)
    : addressSpec(addressSpec_), segmentStore(segmentStore_), nonull(nonull_) {
    specList.Add(&addressSpec);
  }

  void Name(std::ostream& out) const {
    out << "ftos/identity/" << addressSpec
        << "_" << (segmentStore ? "s" : "f") << (nonull ? "n" : "");
  }

protected:
  BrigType ResultType() const { return addressSpec->Type(); }

  Value ExpectedResult() const { return be.GenerateTestValue(addressSpec->Type()); }

  DirectiveVariable in;

  void KernelArguments() {
    Test::KernelArguments();
    in = be.EmitVariableDefinition(be.IName(), BRIG_SEGMENT_KERNARG, be.PointerType());
  }

  void SetupDispatch(DispatchSetup* dsetup) {
    Test::SetupDispatch(dsetup);
    unsigned id = dsetup->MSetup().Count();
    MObject* in = NewMValue(id++, "Input", MEM_GLOBAL, addressSpec->VType(), U64(42));
    dsetup->MSetup().Add(in);
    dsetup->MSetup().Add(NewMValue(id++, "Input (arg)", MEM_KERNARG, MV_REF, R(in->Id())));
  }

  TypedReg Result() {
    PointerReg inp = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    be.EmitLoad(in.segment(), inp, be.Address(in));
    // Convert address
    PointerReg segmentAddress = addressSpec->AddAReg();
    addressSpec->EmitLda(segmentAddress);
    PointerReg flatAddress = be.AddAReg(BRIG_SEGMENT_FLAT);
    be.EmitStof(flatAddress, segmentAddress, nonull);
    PointerReg segmentAddress1 = addressSpec->AddAReg();
    be.EmitFtos(segmentAddress1, flatAddress, nonull);

    // Emit loads/stores
    TypedReg data = be.AddTReg(addressSpec->Type());
    be.EmitLoad(data, inp);
    // be.EmitFtos(2, varSpec.Segment(), 2);
    if (segmentStore) {
      // Segment store / flat load
      be.EmitStore(data, segmentAddress1);
      be.EmitLoad(data, flatAddress);
    } else {
      // Flat store / segment load
      be.EmitStore(data, flatAddress);
      be.EmitLoad(data, segmentAddress1);
    }
    return data;
  }

  bool IsValid() const {
    return Test::IsValid() &&
           addressSpec->Segment() != BRIG_SEGMENT_FLAT &&
           cc->Segments().CanStore(addressSpec->Segment()) &&
           cc->Segments().HasFlatAddress(addressSpec->Segment());
  }
};
*/

class SegmentpNullTest : public Test {
private:
  BrigSegment8_t segment;

public:
  SegmentpNullTest(BrigSegment8_t segment_)
    : segment(segment_) { }

  void Name(std::ostream& out) const {
    out << "segmentp/null/" << segment2str(segment);
  }

  bool IsValid() const {
    return segment != BRIG_SEGMENT_FLAT && cc->Segments().HasFlatAddress(segment);
  }

protected:
  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const { return Value(MV_UINT32, 1); }

  TypedReg Result() {
    // Emit flat nullptr.
    PointerReg flatNull = be.AddAReg(BRIG_SEGMENT_FLAT);
    TypedReg c = be.AddCTReg();
    be.EmitNullPtr(flatNull);
    // Emit segmentp and return the result.
    be.EmitSegmentp(c, flatNull, segment);
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCvt(result, c);
    return result;
  }
};

/*
class SegmentpVariableTest : public Test {
private:
  AddressSpec addressSpec;
  bool nonull;

public:
  SegmentpVariableTest(const AddressSpec& addressSpec_, bool nonull_)
    : addressSpec(addressSpec_), nonull(nonull_) {
    specList.Add(&addressSpec);
  }

  void Name(std::ostream& out) const {
    out << "segmentp/variable/" << addressSpec << (nonull ? "_n" : "");
  }

  bool IsValid() const {
    return cc->Segments().HasFlatAddress(addressSpec->Segment());
  }

protected:
  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    return Value(MV_UINT32, 1);
  }

  DirectiveVariable in;

  TypedReg Result() {
    PointerReg segmentAddr = addressSpec->AddAReg();
    PointerReg flatAddr = be.AddAReg(BRIG_SEGMENT_FLAT);
    addressSpec->EmitLda(segmentAddr);
    be.EmitStof(flatAddr, segmentAddr, nonull);
    TypedReg c = be.AddCTReg();
    be.EmitSegmentp(c, flatAddr, addressSpec->Segment(), nonull);
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCvt(result, c);
    return result;  
  }
};
*/
/*
class LdaAlignmentTest : public Test {
private:
  AddressSpec addressSpec;
  bool checkFlat;

public:
  LdaAlignmentTest(const AddressSpec& addressSpec_, bool checkFlat_)
    : addressSpec(addressSpec_), checkFlat(checkFlat_) {
    specList.Add(&addressSpec);
  }

  void Name(std::ostream& out) const {
    out << "lda/alignment/" << addressSpec << "_" << (checkFlat ? "f" : "s");
  }

  virtual bool IsValid() const {
    if (!addressSpec->IsValid()) { return false; }
    if (!cc->Segments().HasAddress(addressSpec->Segment())) { return false; }
    if (checkFlat && !cc->Segments().HasFlatAddress(addressSpec->Segment())) { return false; }
    return true;
  }

protected:
  BrigType ResultType() const { return be.PointerType(addressSpec->Segment()); }

  Value ExpectedResult() const { return Value(Brig2ValueType(ResultType()), 42); }

  TypedReg Result() {
    PointerReg segmentAddr = addressSpec->AddAReg();
    addressSpec->EmitLda(segmentAddr);
    uint32_t align = (uint32_t) align2num(addressSpec->Align());
    PointerReg checkAddr;
    if (checkFlat) {
      checkAddr = be.AddAReg(BRIG_SEGMENT_FLAT);
      be.EmitStof(checkAddr, segmentAddr);
    } else {
      checkAddr = segmentAddr;
    }
    be.EmitArith(BRIG_OPCODE_REM, checkAddr, checkAddr, be.Brigantine().createImmed(align, checkAddr->Type()));
    be.EmitArith(BRIG_OPCODE_ADD, checkAddr, checkAddr, be.Brigantine().createImmed(42, checkAddr->Type()));
    return checkAddr;
  }
};
*/

void AddressArithmeticTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  static const char *base = "address";
  TestForEach<StofNullTest>(ap, it, base, cc->Segments().All());
//  TestForEach<StofIdentityTest>(ap, it, base, Address::All(), Bools::All(), Bools::All());
  TestForEach<FtosNullTest>(ap, it, base, cc->Segments().All());
  //TestForEach<FtosIdentityTest>(ap, it, base, Address::All(), Bools::All(), Bools::All());
  TestForEach<SegmentpNullTest>(ap, it, base, cc->Segments().All());
  //TestForEach<SegmentpVariableTest>(ap, it, base, Address::All(), Bools::All());
  //TestForEach<LdaAlignmentTest>(ap, it, base, Address::All(), Bools::All());
}

}
