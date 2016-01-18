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

#include "BarrierTests.hpp"
#include "HsailRuntime.hpp"
#include "BrigEmitter.hpp"
#include "HCTests.hpp"
#include "Scenario.hpp"
#include "UtilTests.hpp"

using namespace hexl;
using namespace hexl::scenario;
using namespace hexl::emitter;
using namespace HSAIL_ASM;

namespace hsail_conformance {
class BarrierTest : public Test {
protected:
  BrigAtomicOperation atomicOp;
  BrigSegment segment;
  BrigMemoryOrder memoryOrder;
  BrigMemoryScope memoryScope;
  bool noret;
  bool isSigned;
  uint8_t equivClass;
  int64_t initialValue;
  int64_t expectedValue;
  int64_t immDest;
  int64_t immSrc0;
  int64_t immSrc1;
  DirectiveVariable globalVar;
  Buffer outDest;
  uint32_t workgroupSizeX;
  uint32_t gridSizeX;
  uint32_t sizeX;

public:
  BarrierTest(Grid geometry_, BrigAtomicOperation atomicOp_, BrigSegment segment_, BrigMemoryOrder memoryOrder_, BrigMemoryScope memoryScope_, bool noret_, bool isSigned_ = false)
    : Test(KERNEL, geometry_),
      atomicOp(atomicOp_), segment(segment_), memoryOrder(memoryOrder_), memoryScope(memoryScope_), noret(noret_), isSigned(isSigned_), equivClass(0),
      initialValue(0), expectedValue(1), immDest(0), immSrc0(1), immSrc1(0) { }

  void Name(std::ostream& out) const { out << (noret ? "atomicnoret" : "atomic")
                                           << "_" << atomicOperation2str(atomicOp) << "_"
                                           << (segment != BRIG_SEGMENT_FLAT ? segment2str(segment) : "")
                                           << (segment != BRIG_SEGMENT_FLAT ? "_" : "")
                                           << memoryOrder2str(be.AtomicMemoryOrder(atomicOp, memoryOrder))
                                           << "_" << memoryScope2str(memoryScope)
                                           << (isSigned ? "/signed" : "")
                                           << "/" << geometry; }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const {
    switch (atomicOp) {
      case BRIG_ATOMIC_WRAPINC:
      case BRIG_ATOMIC_ADD:     return Value(MV_UINT32, U32(sizeX * uint32_t(expectedValue)));
      case BRIG_ATOMIC_AND:
      case BRIG_ATOMIC_WRAPDEC: return Value(MV_UINT32, U32(0));
//      case BRIG_ATOMIC_EXCH:
      case BRIG_ATOMIC_MAX:     return Value(MV_UINT32, U32(sizeX-1));
      case BRIG_ATOMIC_OR:
      case BRIG_ATOMIC_XOR:     return Value(MV_UINT32, U32(~(-1 << int32_t(sizeX/te->CoreCfg()->Wavesize()))));
      case BRIG_ATOMIC_MIN:
      default:                  return Value(MV_UINT32, U32(1));
    }
  }

  void EmulateAtomicOperation() {
    switch (atomicOp) {
      case BRIG_ATOMIC_AND:     initialValue = ~(-1 << int64_t(workgroupSizeX/te->CoreCfg()->Wavesize())); break;
//      case BRIG_ATOMIC_EXCH:    immSrc0 = sizeX * 3; break;
      case BRIG_ATOMIC_MIN:     initialValue = sizeX + 1; immSrc0 = sizeX; break;
      case BRIG_ATOMIC_SUB:     initialValue = sizeX + 1; break;
      case BRIG_ATOMIC_WRAPINC: initialValue = 0;         immSrc0 = sizeX; break;
      case BRIG_ATOMIC_WRAPDEC: initialValue = sizeX;     immSrc0 = sizeX + 1; break;
      default: break;
    }
    expectedValue = GetExpectedAtomicValue(immDest, initialValue, immSrc0, immSrc1);
  }

  int64_t GetExpectedAtomicValue(int64_t &dest, int64_t original, int64_t src0, int64_t src1 = 0) {
    dest = original;
    switch (atomicOp) {
      case BRIG_ATOMIC_LD: // ??
      case BRIG_ATOMIC_ST: // ??
      case BRIG_ATOMIC_EXCH:    return src0;
      case BRIG_ATOMIC_ADD:     return original + src0;
      case BRIG_ATOMIC_AND:     return original & src0;
      case BRIG_ATOMIC_OR:      return original | src0;
      case BRIG_ATOMIC_XOR:     return original ^ src0;
      case BRIG_ATOMIC_SUB:     return original - src0;
      case BRIG_ATOMIC_CAS:     return (original == src0) ? src1 : original;
      case BRIG_ATOMIC_MAX:     return (original < src0) ? src0 : original;
      case BRIG_ATOMIC_MIN:     return (original > src0) ? src0 : original;
      case BRIG_ATOMIC_WRAPINC: return (original >= src0) ? 0 : ++original;
      case BRIG_ATOMIC_WRAPDEC: return (original == 0 || original > src0) ? src0 : --original;
      default: assert(false); return 0;
    }
  }

  bool IsValid() const {
    // TODO: implement tests for the following 4 excluded atomics
    switch (atomicOp) {
      case BRIG_ATOMIC_CAS:
      case BRIG_ATOMIC_EXCH:
      case BRIG_ATOMIC_LD:
      case BRIG_ATOMIC_ST: return false;
      default: break;
    }
    if (isSigned) {
      switch (atomicOp) {
        // 6.6.1., 6.7.1. Explanation of Modifiers. Type (signed is aplicable only for ADD, SUB, MAX, MIN)
        case BRIG_ATOMIC_ADD:
        case BRIG_ATOMIC_SUB:
        case BRIG_ATOMIC_MAX:
        case BRIG_ATOMIC_MIN: break;
        default:              return false;
      }
    }
    // 6.6.1., 6.7.1. Explanation of Modifiers. Order
    if ((BRIG_ATOMIC_ST == atomicOp || BRIG_ATOMIC_LD == atomicOp) &&
        (BRIG_MEMORY_ORDER_SC_ACQUIRE == memoryOrder || BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE == memoryOrder))
      return false;
    // 6.7.1 ret mode is not applicable for ST
    if (BRIG_ATOMIC_ST == atomicOp && !noret) return false;
    // 6.7.1 noret mode is not applicable for EXCH
    if (BRIG_ATOMIC_EXCH == atomicOp && noret) return false;
    // 6.6.1., 6.7.1. Explanation of Modifiers. Scope
    switch (segment) {
      case BRIG_SEGMENT_FLAT:
      case BRIG_SEGMENT_GLOBAL:
        // TODO: For a flat address, any value can be used, but if the address references the group segment,
        // cmp and sys behave as if wg was specified
        if (BRIG_MEMORY_SCOPE_WORKITEM == memoryScope || BRIG_MEMORY_SCOPE_NONE == memoryScope) return false;
        break;
      case BRIG_SEGMENT_GROUP:
        if (memoryScope != BRIG_MEMORY_SCOPE_WAVEFRONT && memoryScope != BRIG_MEMORY_SCOPE_WORKGROUP) return false;
        break;
      default: return false;
    }
    return true;
  }

  void Init() {
    Test::Init();
    // TODO: dest register buffer
    //outDest = kernel->NewBuffer("dest", HOST_RESULT_BUFFER, MV_UINT32, geometry->GridSize());
    //for (uint64_t i = 0; i < uint64_t(geometry->GridSize()); ++i) {
    //  outDest->AddData(Value(MV_UINT32, i));
    //}
    workgroupSizeX = geometry->WorkgroupSize();
    gridSizeX = geometry->GridSize();
    sizeX = (BRIG_SEGMENT_GROUP == segment) ? workgroupSizeX : gridSizeX;

    EmulateAtomicOperation();
  }

  void ModuleVariables() {
    BrigSegment segmentForGlobalVar = segment;
    std::string globalVarName = "global_var";
    if (cc->Segments().Atomic()->Has(segment)) {
      switch (segment) {
        case BRIG_SEGMENT_GROUP: globalVarName = "group_var"; break;
        case BRIG_SEGMENT_FLAT: segmentForGlobalVar = BRIG_SEGMENT_GLOBAL; break;
        default: break;
      }
    }
    globalVar = be.EmitVariableDefinition(globalVarName, segmentForGlobalVar, be.AtomicValueIntType(isSigned));
    if (segment != BRIG_SEGMENT_GROUP)
      globalVar.init() = be.Immed(be.AtomicValueType(atomicOp, isSigned), initialValue);
  }

  // TODO: dest verification, which is obligatory for atomicity verification of at least MAX & MIN:
  // to be sure of sequential intermediate result, in other words, atomic operation took place for all workitems
  TypedReg Result() {
    TypedReg result = be.AddTReg(ResultType());
    BrigType vtype = be.AtomicValueType(atomicOp, isSigned);
    TypedReg dest = NULL, src0 = be.AddTReg(vtype), src1 = NULL, wiID = NULL, idInWF = NULL, cReg = NULL, addrReg = NULL;
    if (!noret) dest = be.AddTReg(vtype);
    if (BRIG_ATOMIC_MIN != atomicOp && BRIG_ATOMIC_MAX != atomicOp && BRIG_ATOMIC_EXCH != atomicOp)
      be.EmitMov(src0, immSrc0);
    switch (atomicOp) {
      case BRIG_ATOMIC_AND:
      case BRIG_ATOMIC_EXCH:
      case BRIG_ATOMIC_MAX:
      case BRIG_ATOMIC_MIN:
      case BRIG_ATOMIC_OR:
      case BRIG_ATOMIC_XOR: {
        // for group segment: workitemid_u32 $s1, 0;
        // for global, flat:  workitemflatid_u32 $s1;
        wiID = (BRIG_SEGMENT_GROUP == segment) ? be.EmitWorkitemId(0) : be.EmitWorkitemFlatAbsId(false);
        TypedReg waveID = be.AddTReg(wiID->Type());
        if (BRIG_ATOMIC_MAX == atomicOp) {
          be.EmitCvt(src0, wiID);
        } else if (BRIG_ATOMIC_EXCH == atomicOp) {
          be.EmitCvt(src0->Reg(), be.AtomicValueIntType(), wiID->Reg(), wiID->Type());
        } else if (BRIG_ATOMIC_MIN == atomicOp) {
          be.EmitMov(waveID, sizeX);
          be.EmitArith(BRIG_OPCODE_SUB, wiID, waveID, wiID->Reg());
          be.EmitCvt(src0, wiID);
        } else {
          // dense wavefront id {0..n}:
          // div_u32 $s1, $s1, WAVESIZE;
          be.EmitArith(BRIG_OPCODE_DIV, waveID, wiID, be.Wavesize());
          // Shif left the value (immSrc0) of the source register (src0) by wave id:
          // shl_u64 $d0, $d0, $s1;
          be.EmitArith(BRIG_OPCODE_SHL, src0, src0, waveID->Reg());
          if (BRIG_ATOMIC_OR != atomicOp) {
            // NOT the source register value:
            // not_b64 $d0, $d0;
            be.EmitArith(BRIG_OPCODE_NOT, src0, src0->Reg());
          }
        }
        break;
      }
      default: break;
    }
    OperandAddress addr = be.Address(globalVar);
    switch (segment) {
      case BRIG_SEGMENT_FLAT: {
        PointerReg flatAddr = be.AddAReg(globalVar.segment());
        be.EmitLda(flatAddr, addr);
        addr = be.Address(flatAddr);
        break;
      }
      case BRIG_SEGMENT_GROUP: {
        TypedReg initValueReg = be.AddTReg(be.PointerType());
        be.EmitMov(initValueReg, initialValue);
        if (!wiID) wiID = be.EmitWorkitemId(0);
        // atomic store for global group_var should occur only for the first workitem
        cReg = be.AddTReg(BRIG_TYPE_B1);
        SRef s_label_skip_store = "@skip_store";
        be.EmitCmp(cReg->Reg(), wiID, be.Immed(wiID->Type(), 1), BRIG_COMPARE_NE);
        be.EmitCbr(cReg, s_label_skip_store);
        be.EmitAtomic(NULL, addr, initValueReg, NULL, BRIG_ATOMIC_ST, memoryOrder, memoryScope, segment, false, equivClass);
        // be.EmitStore(segment, initValueReg, addr);
        be.EmitLabel(s_label_skip_store);
        be.EmitBarrier();
        break;
      }
      default: break;
    }
    SRef s_label_skip_first_wi_in_wf = "@skip_first_wi_in_wf";
    if (BRIG_ATOMIC_XOR == atomicOp) {
      // perform XOR operation for all workitems in a wavefront, except the first one (with idInWF = 0)
      // in order to get odd XOR operations, because XOR operation is reversible or involutive operation,
      // i.e. even XOR operations lead to the result, which is always equal to the initial value: x XOR y XOR y = x
      idInWF = be.AddTReg(wiID->Type());
      // idInWF register contains "id" for workitem in a wavefront = {0,..,WAVESIZE}
      be.EmitArith(BRIG_OPCODE_REM, idInWF, wiID->Reg(), be.Wavesize());
      TypedReg r0 = be.AddTReg(wiID->Type());
      // TODO: add EmitCmp with operands (for immed) besides registers in order not to emit additional move
      be.EmitMov(r0, (uint64_t) 0);
      if (!cReg) cReg = be.AddTReg(BRIG_TYPE_B1);
      // Only workitems with "id" in wavefront grater than 0 will perform atomic XOR,
      // i.e. first workitem in every wavefront will be skipped for XOR
      be.EmitCmp(cReg->Reg(), idInWF, r0, BRIG_COMPARE_GT);
      be.EmitCbr(cReg, s_label_skip_first_wi_in_wf);
    }
    be.EmitAtomic(dest, addr, src0, src1, atomicOp, memoryOrder, memoryScope, segment, isSigned, equivClass);
    if (BRIG_ATOMIC_XOR == atomicOp) be.EmitLabel(s_label_skip_first_wi_in_wf);
    // TODO: finish EXCH
///    if (BRIG_ATOMIC_EXCH == atomicOp) {
///      be.EmitBarrier();
///      cReg = be.AddTReg(BRIG_TYPE_B1);
///      be.EmitCmp(cReg->Reg(), src0, dest, BRIG_COMPARE_GT);
///      be.EmitCbr(cReg, s_label_skip_first_wi_in_wf);
///      be.EmitMov(src0, 555);
///      be.EmitAtomic(src0, addr, dest, src1, atomicOp, memoryOrder, memoryScope, segment, isSigned, equivClass);
///    }
    be.EmitBarrier();
    // TODO: dest verification
///    TypedReg destOutReg = be.AddTReg(BRIG_TYPE_U32);
///    be.EmitCvt(destOutReg->Reg(), destOutReg->Type(), dest->Reg(), BRIG_TYPE_U64);
///    outDest->EmitStoreData(destOutReg);
    if (!addrReg) addrReg = be.AddTReg(globalVar.type());
    be.EmitAtomic(addrReg, addr, NULL, NULL, BRIG_ATOMIC_LD, memoryOrder, memoryScope, segment, false, equivClass);
    // TODO: remove cvt after fixing ResultType() for return value based on Model (large|small)
    be.EmitCvt(result, addrReg);
    return result;
  }
};


class FBarrierDoubleBranchTest: public Test {
private:
  FBarrier fb;

protected:
  FBarrier Fb() const { return fb; }

  virtual void EmitFirstBranch(TypedReg wiId, PointerReg globalOffset) = 0;
  virtual void EmitSecondBranch(TypedReg wiId, PointerReg globalOffset) = 0;

  virtual void EmitInit() {
    // init and join fbarrier
    Fb()->EmitInitfbarInFirstWI();
    Fb()->EmitJoinfbar();
    be.EmitBarrier();
  }

  virtual void EmitRelease() {
    be.EmitBarrier();
    Fb()->EmitLeavefbar();
    be.EmitBarrier();
    Fb()->EmitReleasefbarInFirstWI();
  }

  virtual TypedReg EmitWorkitemId() {
    return be.EmitCurrentWorkitemFlatId();
  }

  virtual TypedReg EmitBranchCondition(TypedReg wiId) {
    // if wiId < WAVESIZE
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp->Reg(), wiId, be.Wavesize(), BRIG_COMPARE_LT);
    return cmp;
  }

  virtual PointerReg EmitGlobalOffset(TypedReg wiId) {
    auto wiAbsId = be.EmitWorkitemFlatAbsId(true);
    auto offset = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    auto typeSize = be.Immed(offset->Type(), getBrigTypeNumBytes(ResultType()));
    be.EmitArith(BRIG_OPCODE_MAD, offset, wiAbsId, typeSize, output->Address());
    return offset;
  }

public:
  explicit FBarrierDoubleBranchTest(Grid geometry = 0): Test(Location::KERNEL, geometry) {}

  void Init() override {
    Test::Init();
    fb = kernel->NewFBarrier("fb");
  }

  void Name(std::ostream& out) const override {
    out << geometry;
  }

  void KernelCode() override {
    auto elseLabel = "@else";
    auto endifLabel = "@endif";

    EmitInit();

    auto wiId = EmitWorkitemId();
    auto globalOffset = EmitGlobalOffset(wiId);

    // first branch
    auto cmp = EmitBranchCondition(wiId);
    assert(cmp->Type() == BRIG_TYPE_B1);
    be.EmitArith(BRIG_OPCODE_NOT, cmp, cmp->Reg());
    be.EmitCbr(cmp->Reg(), elseLabel);
    EmitFirstBranch(wiId, globalOffset);
    be.EmitBr(endifLabel);

    // second branch
    be.EmitLabel(elseLabel);
    EmitSecondBranch(wiId, globalOffset);
    be.EmitLabel(endifLabel);

    EmitRelease();
  }

};


class FBarrierBasicTest: public FBarrierDoubleBranchTest {
private:
  static const uint32_t VALUE1 = 123;
  static const uint32_t VALUE2 = 456;

protected:

  TypedReg EmitWorkitemId() override {
    return be.EmitWorkitemFlatId();
  }

  PointerReg EmitGlobalOffset(TypedReg wiId) override {
    // organize output array in chunks by work-groups and wavefronts
    auto wgSize = be.EmitWorkgroupSize();
    auto wgFlatId = be.EmitWorkgroupFlatId();

    auto offset32 = be.AddTReg(wgFlatId->Type());
    // size of wg flattened id must be 32-bit as well as wi flattened id
    assert(wiId->Type() == wgFlatId->Type());
    auto typeSize = be.Immed(offset32->Type(), getBrigTypeNumBytes(ResultType()));
    // offset32 = (wgSize * wgFlatId + wiFlatId) * typeSize
    be.EmitArith(BRIG_OPCODE_MAD, offset32, wgSize, wgFlatId, wiId->Reg());
    be.EmitArith(BRIG_OPCODE_MUL, offset32, offset32, typeSize);

    // convert offset32 to global offset
    auto offset = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    if (offset->Type() == offset32->Type()) {
      be.EmitMov(offset, offset32);
    } else {
      be.EmitCvt(offset, offset32);
    }
    // global offset must be same size as output address
    be.EmitArith(BRIG_OPCODE_ADD, offset, offset, output->Address());
    return offset;
  }

  TypedReg EmitBranchCondition(TypedReg wiId) override {
    // is work-item - odd or even
    auto waveId = be.AddTReg(wiId->Type());
    be.EmitArith(BRIG_OPCODE_DIV, waveId, wiId, be.Wavesize());
    auto arith = be.AddTReg(wiId->Type());
    be.EmitArith(BRIG_OPCODE_AND, arith, waveId, be.Immed(arith->Type(), 1));
    auto isOdd = be.AddCTReg();
    be.EmitCmp(isOdd->Reg(), arith, be.Immed(arith->Type(), 1), BRIG_COMPARE_EQ);
    return isOdd;
  }

  void EmitFirstBranch(TypedReg wiId, PointerReg globalOffset) override {
    // odd
    // store data in output
    auto value = be.AddInitialTReg(ResultType(), VALUE1);
    be.EmitAtomicStore(value, globalOffset);
    // wait on fbarrrier
    be.EmitMemfence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_MEMORY_SCOPE_NONE);
    Fb()->EmitWaitfbar();
  }

  void EmitSecondBranch(TypedReg wiId, PointerReg globalOffset) override {
    // even
    // wait on fbarrrier
    Fb()->EmitWaitfbar();
    be.EmitMemfence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_MEMORY_SCOPE_NONE);
    // store data in output
    auto value = be.AddInitialTReg(ResultType(), VALUE2);
    be.EmitAtomicStore(value, globalOffset);
    // store data in neighbours wave output memory region
    auto outputAddr = be.AddAReg(globalOffset->Segment());
    auto typeSize = be.AddTReg(outputAddr->Type());
    be.EmitMov(typeSize, getBrigTypeNumBytes(ResultType()));
    be.EmitArith(BRIG_OPCODE_MAD, outputAddr, typeSize, be.Wavesize(), globalOffset);
    be.EmitAtomicStore(value, outputAddr);
  }

public:
  explicit FBarrierBasicTest(Grid geometry): FBarrierDoubleBranchTest(geometry) {}

  bool IsValid() const override {
    return FBarrierDoubleBranchTest::IsValid()
        && !geometry->isPartial(); // no partial work-groups
  }

  void Init() override {
    FBarrierDoubleBranchTest::Init();
    assert((geometry->WorkgroupSize() % (2 * te->CoreCfg()->Wavesize())) == 0); // group size is multiple of WAVESIZE and there are even number of waves in work-group
  }

  BrigType ResultType() const override {
    return te->CoreCfg()->IsLarge() ? BRIG_TYPE_U64 : BRIG_TYPE_U32;
  }

  Value ExpectedResult() const override { return Value(Brig2ValueType(ResultType()), VALUE2); }
};


class FBarrierExampleTest: public FBarrierDoubleBranchTest {
protected:
  static const uint32_t VALUE1 = 123;
  static const uint32_t VALUE2 = 456;
  static const uint32_t VALUE3 = 789;
  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;

  virtual TypedReg EmitSecondBranchCondition(TypedReg wiId) {
    // if wiId < 2 * WAVESIZE
    auto mul = be.AddTReg(BRIG_TYPE_U32);
    be.EmitArith(BRIG_OPCODE_MUL, mul, be.Wavesize(), be.Immed(mul->Type(), 2));
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp->Reg(), wiId, mul, BRIG_COMPARE_LT);
    return cmp;
  }

  virtual void EmitThirdBranch(TypedReg wiId, PointerReg globalOffset) = 0;

public:
  explicit FBarrierExampleTest(Grid geometry) : FBarrierDoubleBranchTest(geometry) {}

  BrigType ResultType() const override { return VALUE_TYPE; }

  void ExpectedResults(Values* result) const override {
    result->reserve(geometry->GridSize());
    for (uint32_t z = 0; z < geometry->GridSize(2); ++z) {
      for (uint32_t y = 0; y < geometry->GridSize(1); ++y) {
        for (uint32_t x = 0; x < geometry->GridSize(0); ++x) {
          Dim point(x, y, z);
          auto wiId = geometry->CurrentWorkitemFlatId(point);
          if (wiId < te->CoreCfg()->Wavesize()) {
            result->push_back(Value(Brig2ValueType(VALUE_TYPE), VALUE1));
          } else if (wiId < 2 * te->CoreCfg()->Wavesize()) {
            result->push_back(Value(Brig2ValueType(VALUE_TYPE), VALUE2));
          } else {
            result->push_back(Value(Brig2ValueType(VALUE_TYPE), VALUE3));
          }
        }
      }
    }
  }
};



class FBarrierFirstExampleTest: public FBarrierExampleTest {
protected:

  virtual void EmitFirstBranch(TypedReg wiId, PointerReg globalOffset) override {
    // store VALUE1 in output
    be.EmitStore(ResultType(), be.Immed(ResultType(), VALUE1), globalOffset);
    // wait fbar
    be.EmitMemfence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_SYSTEM, BRIG_MEMORY_SCOPE_SYSTEM, BRIG_MEMORY_SCOPE_NONE);
    Fb()->EmitWaitfbar();
    be.EmitMemfence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_SYSTEM, BRIG_MEMORY_SCOPE_SYSTEM, BRIG_MEMORY_SCOPE_NONE);
    //leavefbar
    Fb()->EmitLeavefbar();
  }

  virtual void EmitSecondBranch(TypedReg wiId, PointerReg globalOffset) override {
    auto else2Label = "@else2";
    auto endif2Label = "@endif2";

    auto cmp = EmitSecondBranchCondition(wiId);
    be.EmitArith(BRIG_OPCODE_NOT, cmp, cmp->Reg());
    be.EmitCbr(cmp->Reg(), else2Label);
    // store VALUE2 in output
    be.EmitStore(ResultType(), be.Immed(ResultType(), VALUE2), globalOffset);
    //leavefbar
    Fb()->EmitLeavefbar();
    be.EmitBr(endif2Label);

    // else
    be.EmitLabel(else2Label);
    EmitThirdBranch(wiId, globalOffset);
    be.EmitLabel(endif2Label);
  }

  virtual void EmitThirdBranch(TypedReg wiId, PointerReg globalOffset) override {
    // store VALUE3 in output
    be.EmitStore(ResultType(), be.Immed(ResultType(), VALUE3), globalOffset);
    //leavefbar
    Fb()->EmitLeavefbar();
  }

  virtual void EmitRelease() {
    be.EmitBarrier();
    Fb()->EmitReleasefbarInFirstWI();
  }

public:
  explicit FBarrierFirstExampleTest(Grid geometry) : FBarrierExampleTest(geometry) {}
};


class FBarrierSecondExampleTest: public FBarrierExampleTest {
private:
  FBarrier fb1;

  FBarrier Fb1() const { return fb1; }

protected:
  virtual void EmitInit() override {
    Fb()->EmitInitfbarInFirstWI();
    Fb1()->EmitInitfbarInFirstWI();
    Fb()->EmitJoinfbar();
    be.EmitBarrier();
  }

  virtual void EmitRelease() override {
    be.EmitBarrier();
    Fb()->EmitLeavefbar();
    be.EmitBarrier();
    Fb()->EmitReleasefbarInFirstWI();
    Fb1()->EmitReleasefbarInFirstWI();
  }

  virtual void EmitFirstBranch(TypedReg wiId, PointerReg globalOffset) override {
    // join to fb1 and wait on fb0
    Fb1()->EmitJoinfbar();
    Fb()->EmitWaitfbar();
    // store VALUE1 in output
    be.EmitStore(ResultType(), be.Immed(ResultType(), VALUE1), globalOffset);
    // wait fb1
    be.EmitMemfence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_SYSTEM, BRIG_MEMORY_SCOPE_SYSTEM, BRIG_MEMORY_SCOPE_NONE);
    Fb1()->EmitWaitfbar();
    be.EmitMemfence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_SYSTEM, BRIG_MEMORY_SCOPE_SYSTEM, BRIG_MEMORY_SCOPE_NONE);
    //leave fb1
    Fb1()->EmitLeavefbar();
  }

  virtual void EmitSecondBranch(TypedReg wiId, PointerReg globalOffset) override {
    auto else2Label = "@else2";
    auto endif2Label = "@endif2";

    auto cmp = EmitSecondBranchCondition(wiId);
    be.EmitArith(BRIG_OPCODE_NOT, cmp, cmp->Reg());
    be.EmitCbr(cmp->Reg(), else2Label);
    // store VALUE2 in output
    be.EmitStore(ResultType(), be.Immed(ResultType(), VALUE2), globalOffset);
    //wait fb0
    Fb()->EmitWaitfbar();
    be.EmitBr(endif2Label);

    // else
    be.EmitLabel(else2Label);
    EmitThirdBranch(wiId, globalOffset);
    be.EmitLabel(endif2Label);
  }

  virtual void EmitThirdBranch(TypedReg wiId, PointerReg globalOffset) override {
    // store VALUE3 in output
    be.EmitStore(ResultType(), be.Immed(ResultType(), VALUE3), globalOffset);
    //wait fb0
    Fb()->EmitWaitfbar();
  }

public:
  explicit FBarrierSecondExampleTest(Grid geometry) : FBarrierExampleTest(geometry) {}

  void Init() override {
    FBarrierExampleTest::Init();
    fb1 = kernel->NewFBarrier("fb1");
  }
};


class FBarrierThirdExampleTest: public FBarrierDoubleBranchTest {
private:
  FBarrier cfb;
  Variable buffer;
  TypedReg waveId;
  TypedReg counter;
  PointerReg groupOffset;

  static const uint32_t DATA_ITEM_COUNT = 8;

  FBarrier Pfb() const { return Fb(); }
  FBarrier Cfb() const { return cfb; }

protected:

  virtual void EmitInit() override {
    // init and join fbarrier
    Pfb()->EmitInitfbarInFirstWI();
    Cfb()->EmitInitfbarInFirstWI();
    Pfb()->EmitJoinfbar();
    Cfb()->EmitJoinfbar();
    be.EmitBarrier();
  }

  virtual void EmitRelease() override {
    // leave fbarriers
    be.EmitBarrier();
    Pfb()->EmitLeavefbar();
    Cfb()->EmitLeavefbar();
    be.EmitBarrier();
    Pfb()->EmitReleasefbarInFirstWI();
    Cfb()->EmitReleasefbarInFirstWI();
  }

  virtual TypedReg EmitWorkitemId() {
    return be.EmitWorkitemFlatId();
  }

  virtual TypedReg EmitBranchCondition(TypedReg wiId) {
    // waveId
    waveId = be.AddTReg(wiId->Type());
    be.EmitArith(BRIG_OPCODE_DIV, waveId, wiId, be.Wavesize());

    // group offset
    auto arith = be.AddTReg(wiId->Type());
    be.EmitArith(BRIG_OPCODE_DIV, arith, waveId, be.Immed(arith->Type(), 2));
    auto laneId = be.AddTReg(waveId->Type());
    be.EmitLaneid(laneId);
    be.EmitArith(BRIG_OPCODE_MAD, arith, arith, be.Wavesize(), laneId);
    groupOffset = be.AddAReg(BRIG_SEGMENT_GROUP);
    auto bufferAddress = be.AddAReg(buffer->Segment());
    be.EmitLda(bufferAddress, be.Address(buffer->Variable()));
    be.EmitArith(BRIG_OPCODE_MAD, groupOffset, arith, be.Immed(groupOffset->Type(), getBrigTypeNumBytes(ResultType())), bufferAddress);

    // counter
    counter = be.AddTReg(ResultType());
    be.EmitMov(counter, (uint64_t) 0);

    // is work-item - producer
    be.EmitArith(BRIG_OPCODE_AND, arith, waveId, be.Immed(arith->Type(), 1));
    auto isProducer = be.AddCTReg();
    be.EmitCmp(isProducer->Reg(), arith, be.Immed(arith->Type(), 1), BRIG_COMPARE_EQ);
    return isProducer;
  }

  virtual void EmitFirstBranch(TypedReg wiId, PointerReg globalOffset) override {
    // producer
    auto producerLoopLabel = "@producer_loop";
    be.EmitLabel(producerLoopLabel);
    // wait on consumer fbarrrier
    Cfb()->EmitWaitfbar();
    // fill group buffer with data and signal consumers
    be.EmitAtomicStore(counter, groupOffset);
    be.EmitMemfence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_MEMORY_SCOPE_NONE);
    Pfb()->EmitArrivefbar();
    // producer store data in output
    auto outputAddr = be.AddAReg(globalOffset->Segment());
    auto cvt = be.AddTReg(outputAddr->Type());
    auto counterShift = be.Immed(cvt->Type(), geometry->GridSize() * getBrigTypeNumBytes(ResultType()));
    be.EmitCvtOrMov(cvt, counter);
    be.EmitArith(BRIG_OPCODE_MAD, outputAddr, cvt, counterShift, globalOffset);
    be.EmitStore(counter, outputAddr);
    // loop
    auto cmp = be.AddCTReg();
    be.EmitArith(BRIG_OPCODE_ADD, counter, counter, be.Immed(counter->Type(), 1));
    be.EmitCmp(cmp->Reg(), counter, be.Immed(counter->Type(), DATA_ITEM_COUNT), BRIG_COMPARE_LT);
    be.EmitCbr(cmp->Reg(), producerLoopLabel);
  }

  virtual void EmitSecondBranch(TypedReg wiId, PointerReg globalOffset) override {
    // consumer
    // initial arrive to consumer fbarrier
    Cfb()->EmitArrivefbar();
    auto consumerLoopLabel = "@consumer_loop";
    be.EmitLabel(consumerLoopLabel);
    // wait on producer fbarrrier
    Pfb()->EmitWaitfbar();
    be.EmitMemfence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_MEMORY_SCOPE_WORKGROUP, BRIG_MEMORY_SCOPE_NONE);
    // read produced data
    auto data = be.AddTReg(ResultType());
    be.EmitAtomicLoad(data, groupOffset);
    // if counter != DATA_ITEM_COUNT - 1 then signal producers
    auto cmp = be.AddCTReg();
    be.EmitCmp(cmp->Reg(), counter, be.Immed(counter->Type(), DATA_ITEM_COUNT - 1), BRIG_COMPARE_EQ);
    auto signalProducerLabel = "@signal_producer";
    be.EmitCbr(cmp->Reg(), signalProducerLabel);
    Cfb()->EmitArrivefbar();
    be.EmitLabel(signalProducerLabel);
    // consumer store data in output
    auto outputAddr = be.AddAReg(globalOffset->Segment());
    auto cvt = be.AddTReg(outputAddr->Type());
    be.EmitCvtOrMov(cvt, counter);
    auto counterShift = be.Immed(cvt->Type(), geometry->GridSize() * getBrigTypeNumBytes(ResultType()));
    be.EmitArith(BRIG_OPCODE_MAD, outputAddr, cvt, counterShift, globalOffset);
    be.EmitStore(data, outputAddr);
    // loop
    be.EmitArith(BRIG_OPCODE_ADD, counter, counter, be.Immed(counter->Type(), 1));
    be.EmitCmp(cmp->Reg(), counter, be.Immed(counter->Type(), DATA_ITEM_COUNT), BRIG_COMPARE_LT);
    be.EmitCbr(cmp->Reg(), consumerLoopLabel);
  }

public:
  explicit FBarrierThirdExampleTest(Grid geometry): FBarrierDoubleBranchTest(geometry) {}

  bool IsValid() const override {
    return FBarrierDoubleBranchTest::IsValid()
        && !geometry->isPartial(); // no partial work-groups
  }

  void Init() override {
    FBarrierDoubleBranchTest::Init();
    assert((geometry->WorkgroupSize() % (2 * te->CoreCfg()->Wavesize())) == 0); // group size is multiple of WAVESIZE and there are even number of waves in work-group
    cfb = kernel->NewFBarrier("consumed_fb");
    buffer = kernel->NewVariable("buffer", BRIG_SEGMENT_GROUP, ResultType(), Location::KERNEL, BRIG_ALIGNMENT_NONE, geometry->WorkgroupSize() / 2);
  }

  BrigType ResultType() const override {
    return te->CoreCfg()->IsLarge() ? BRIG_TYPE_U64 : BRIG_TYPE_U32;
  }

  uint64_t ResultDim() const override { return DATA_ITEM_COUNT; }

  void ExpectedResults(Values* result) const override {
    result->reserve(OutputBufferSize());
    for (uint32_t i = 0; i < DATA_ITEM_COUNT; ++i) {
      for (uint32_t j = 0; j < geometry->GridSize(); ++j) {
        result->push_back(Value(Brig2ValueType(ResultType()), i));
      }
    }
  }
};

class LdfTest: public Test {
private:
  FBarrier fb;

  static const int RESULT_VALUE = 1;
  static const BrigType RESULT_TYPE = BrigType::BRIG_TYPE_U32;

public:
  explicit LdfTest(bool): Test() {}

  void Name(std::ostream& out) const override {}

  BrigType ResultType() const override { return RESULT_TYPE; }
  hexl::Value ExpectedResult() const override { return hexl::Value(hexl::Brig2ValueType(RESULT_TYPE), RESULT_VALUE); }

  void Init() override {
    Test::Init();
    fb = kernel->NewFBarrier("fb");
  }

  TypedReg Result() override {
    auto ldf = be.AddTReg(BRIG_TYPE_U32);
    fb->EmitLdf(ldf);
    be.EmitInitfbarInFirstWI(ldf);
    be.EmitJoinfbar(ldf);
    be.EmitBarrier();
    be.EmitArrivefbar(ldf);
    be.EmitBarrier();
    be.EmitWaitfbar(ldf);
    be.EmitBarrier();
    be.EmitLeavefbar(ldf);
    be.EmitBarrier();
    be.EmitReleasefbarInFirstWI(ldf);

    return be.AddInitialTReg(ResultType(), RESULT_VALUE);
  }
};


class FBarrierPairOperationTest: public FBarrierDoubleBranchTest {
private:
  FBarrier fb1; // addition fbarrier that ensures simultaneous pair operations in different waves

  static const BrigType VALUE_TYPE = BRIG_TYPE_U32;
  static const uint32_t VALUE = 123456789;
  static const uint32_t ITERATION_NUMBER = 32;

protected:
  FBarrier Fb1() const { return fb1; }

  void EmitFirstBranch(TypedReg wiId, PointerReg globalOffset) override {
    EmitPreFirstLoop();
    auto counter = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(counter, (uint64_t) 0);
    // loop
    auto loopLabel = "@loop1";
    be.EmitLabel(loopLabel);

    EmitFirstLoop();

    // iterate
    auto cmp = be.AddCTReg();
    be.EmitArith(BRIG_OPCODE_ADD, counter, counter, be.Immed(counter->Type(), 1));
    be.EmitCmp(cmp->Reg(), counter, be.Immed(counter->Type(), ITERATION_NUMBER), BRIG_COMPARE_LT);
    be.EmitCbr(cmp->Reg(), loopLabel);
    EmitPostFirstLoop();
  }

  void EmitSecondBranch(TypedReg wiId, PointerReg globalOffset) override {
    EmitPreSecondLoop();
    auto counter = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(counter, (uint64_t) 0);
    // loop
    auto loopLabel = "@loop2";
    be.EmitLabel(loopLabel);

    EmitSecondLoop();

    // iterate
    auto cmp = be.AddCTReg();
    be.EmitArith(BRIG_OPCODE_ADD, counter, counter, be.Immed(counter->Type(), 1));
    be.EmitCmp(cmp->Reg(), counter, be.Immed(counter->Type(), ITERATION_NUMBER), BRIG_COMPARE_LT);
    be.EmitCbr(cmp->Reg(), loopLabel);
    EmitPostSecondLoop();
  }

  void EmitInit() override {
    FBarrierDoubleBranchTest::EmitInit();
    Fb1()->EmitInitfbarInFirstWI();
    Fb1()->EmitJoinfbar();
    be.EmitBarrier();
  }

  void EmitRelease() override {
    FBarrierDoubleBranchTest::EmitRelease();
    Fb1()->EmitLeavefbar();
    be.EmitBarrier();
    Fb1()->EmitReleasefbarInFirstWI();
    auto result = be.AddTReg(ResultType());
    be.EmitMov(result, VALUE);
    output->EmitStoreData(result);
  }

  virtual void EmitFirstLoop() = 0;
  virtual void EmitSecondLoop() = 0;
  virtual void EmitPreFirstLoop() {}
  virtual void EmitPreSecondLoop() {}
  virtual void EmitPostFirstLoop() {}
  virtual void EmitPostSecondLoop() {}

public:
  explicit FBarrierPairOperationTest(Grid geometry): FBarrierDoubleBranchTest(geometry) {}

  void Init() override {
    FBarrierDoubleBranchTest::Init();
    fb1 = kernel->NewFBarrier("fb1");
  }

  BrigType ResultType() const override { return VALUE_TYPE; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(VALUE_TYPE), VALUE); }
};


class FBarrierWaitArriveTest: public FBarrierPairOperationTest {
protected:
  void EmitFirstLoop() override {
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
    // wait fbarrier
    Fb()->EmitWaitfbar();
  }

  void EmitSecondLoop() override {
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
    // arrive fbarrier
    Fb()->EmitArrivefbar();
  }

public:
  explicit FBarrierWaitArriveTest(Grid geometry): FBarrierPairOperationTest(geometry) {}
};


class FBarrierWaitLeaveTest: public FBarrierPairOperationTest {
protected:
  void EmitFirstLoop() override {
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
    // wait fbarrier
    Fb()->EmitWaitfbar();
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
  }

  void EmitSecondLoop() override {
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
    // leave fbarrier
    Fb()->EmitLeavefbar();
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
    // join to fbarrier
    Fb()->EmitJoinfbar();
  }

public:
  explicit FBarrierWaitLeaveTest(Grid geometry): FBarrierPairOperationTest(geometry) {}
};


class FBarrierArriveLeaveTest: public FBarrierPairOperationTest {
protected:
  void EmitFirstLoop() override {
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
    // arrive fbarrier
    Fb()->EmitArrivefbar();
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
  }

  void EmitSecondLoop() override {
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
    // leave fbarrier
    Fb()->EmitLeavefbar();
    // wait on addition fbarrier
    Fb1()->EmitWaitfbar();
    // join to fbarrier
    Fb()->EmitJoinfbar();
  }

public:
  explicit FBarrierArriveLeaveTest(Grid geometry): FBarrierPairOperationTest(geometry) {}
};


class FBarrierWaitRaceTest: public Test {
private:
  FBarrier fb;

  static const uint32_t ITERATION_NUMBER = 256;
  static const int RESULT_VALUE = 1;
  static const BrigType RESULT_TYPE = BrigType::BRIG_TYPE_U32;

public:
  explicit FBarrierWaitRaceTest(Grid geometry): Test(Location::KERNEL, geometry) {}

  void Name(std::ostream& out) const override {
    out << geometry;
  }

  BrigType ResultType() const override { return RESULT_TYPE; }
  hexl::Value ExpectedResult() const override { return hexl::Value(hexl::Brig2ValueType(RESULT_TYPE), RESULT_VALUE); }

  bool IsValid() const override {
    uint32_t wavesize = 64;
    return Test::IsValid()
        && geometry->WorkgroupSize() > wavesize;
  }

  void Init() override {
    Test::Init();
    fb = kernel->NewFBarrier("fb");
  }

  TypedReg Result() override {
    fb->EmitInitfbarInFirstWI();
    fb->EmitJoinfbar();
    be.EmitBarrier();

    auto counter = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(counter, (uint64_t) 0);
    // loop
    auto loopLabel = "@loop2";
    be.EmitLabel(loopLabel);

    fb->EmitWaitfbar();

    // iterate
    auto cmp = be.AddCTReg();
    be.EmitArith(BRIG_OPCODE_ADD, counter, counter, be.Immed(counter->Type(), 1));
    be.EmitCmp(cmp->Reg(), counter, be.Immed(counter->Type(), ITERATION_NUMBER), BRIG_COMPARE_LT);
    be.EmitCbr(cmp->Reg(), loopLabel);

    be.EmitBarrier();
    fb->EmitLeavefbar();
    be.EmitBarrier();
    fb->EmitReleasefbarInFirstWI();

    return be.AddInitialTReg(ResultType(), RESULT_VALUE);
  }
};


class FBarrierJoinLeaveTest: public Test {
private:
  FBarrier fb;

  static const uint32_t VALUE = 123456789;
  static const uint32_t LOOP_COUNT = 32;

public:
  explicit FBarrierJoinLeaveTest(Grid geometry): Test(Location::KERNEL, geometry) {}

  void Name(std::ostream& out) const override {
    out << geometry;
  }

  void Init() override {
    Test::Init();
    fb = kernel->NewFBarrier("fb");
  }

  BrigType ResultType() const override { return BRIG_TYPE_U32; }
  Value ExpectedResult() const override { return Value(Brig2ValueType(ResultType()), VALUE); }

  TypedReg Result() override {
    auto loop = "@loop";

    fb->EmitInitfbarInFirstWI();

    // execute join/leave in cycle
    auto count = be.AddInitialTReg(BRIG_TYPE_U32, 0);
    be.EmitLabel(loop);
    fb->EmitJoinfbar();
    fb->EmitLeavefbar();
    be.EmitArith(BRIG_OPCODE_ADD, count, count, be.Immed(count->Type(), 1));
    auto lt = be.AddCTReg();
    be.EmitCmp(lt, count, be.Immed(count->Type(), LOOP_COUNT), BRIG_COMPARE_LT);
    be.EmitCbr(lt, loop);

    be.EmitBarrier();
    fb->EmitReleasefbarInFirstWI();

    return be.AddInitialTReg(ResultType(), VALUE);
  }
};


class FBarrierJoinLeaveJoinTest: public Test {
private:
  FBarrier fb;

  static const int RESULT_VALUE = 1;
  static const BrigType RESULT_TYPE = BrigType::BRIG_TYPE_U32;

public:
  explicit FBarrierJoinLeaveJoinTest(bool): Test() {}

  void Name(std::ostream& out) const override {}

  BrigType ResultType() const override { return RESULT_TYPE; }
  hexl::Value ExpectedResult() const override { return hexl::Value(hexl::Brig2ValueType(RESULT_TYPE), RESULT_VALUE); }

  void Init() override {
    Test::Init();
    fb = kernel->NewFBarrier("fb");
  }

  TypedReg Result() override {
    fb->EmitInitfbarInFirstWI();
    be.EmitBarrier();
    fb->EmitJoinfbar();
    fb->EmitLeavefbar();
    fb->EmitJoinfbar();
    be.EmitBarrier();
    fb->EmitWaitfbar();
    be.EmitBarrier();
    fb->EmitLeavefbar();
    be.EmitBarrier();
    fb->EmitReleasefbarInFirstWI();

    return be.AddInitialTReg(ResultType(), RESULT_VALUE);
  }
};


void BarrierTests::Iterate(hexl::TestSpecIterator& it) {
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<BarrierTest>(ap, it, "barrier/atomics", cc->Grids().BarrierSet(), cc->Memory().AllAtomics(), cc->Segments().Atomic(), cc->Memory().AllMemoryOrders(), cc->Memory().AllMemoryScopes(), Bools::All(), Bools::All());

  TestForEach<FBarrierBasicTest>(ap, it, "fbarrier/basic", cc->Grids().FBarrierEvenWaveSet());
  TestForEach<FBarrierFirstExampleTest>(ap, it, "fbarrier/example1", cc->Grids().FBarrierSet());
  TestForEach<FBarrierSecondExampleTest>(ap, it, "fbarrier/example2", cc->Grids().FBarrierSet());
  TestForEach<FBarrierThirdExampleTest>(ap, it, "fbarrier/example3", cc->Grids().FBarrierEvenWaveSet());

  TestForEach<LdfTest>(ap, it, "fbarrier/ldf", Bools::Value(true));
  TestForEach<FBarrierJoinLeaveTest>(ap, it, "fbarrier/join_leave", cc->Grids().FBarrierSet());
  TestForEach<FBarrierJoinLeaveJoinTest>(ap, it, "fbarrier/join_leave_join", Bools::Value(true));

  TestForEach<FBarrierWaitArriveTest>(ap, it, "fbarrier/wait_arrive", cc->Grids().FBarrierSet());
  TestForEach<FBarrierWaitLeaveTest>(ap, it, "fbarrier/wait_leave", cc->Grids().FBarrierSet());
  TestForEach<FBarrierArriveLeaveTest>(ap, it, "fbarrier/arrive_leave", cc->Grids().FBarrierSet());

  TestForEach<FBarrierWaitRaceTest>(ap, it, "fbarrier/wait_race", cc->Grids().FBarrierSet());
}

}
