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

#include "BarrierTests.hpp"
#include "HsailRuntime.hpp"
#include "BrigEmitter.hpp"
#include "HCTests.hpp"
#include "Scenario.hpp"

using namespace hexl;
using namespace hexl::scenario;
using namespace hexl::emitter;
using namespace HSAIL_ASM;
using namespace Brig;

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

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

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
    workgroupSizeX = geometry->WorkgroupSize(0);
    gridSizeX = geometry->GridSize(0);
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
    BrigTypeX vtype = be.AtomicValueType(atomicOp, isSigned);
    TypedReg dest = NULL, src0 = be.AddTReg(vtype), src1 = NULL, wiID = NULL, idInWF = NULL, cReg = NULL;
    if (!noret) dest = be.AddTReg(vtype);
    if (BRIG_ATOMIC_MIN != atomicOp && BRIG_ATOMIC_MAX != atomicOp && BRIG_ATOMIC_EXCH != atomicOp)
      be.EmitMov(src0->Reg(), be.Immed(vtype, immSrc0), src0->TypeSizeBits());
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
          be.EmitMov(waveID->Reg(), be.Immed(waveID->Type(), sizeX), waveID->TypeSizeBits());
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
        be.EmitStof(flatAddr, flatAddr);
        addr = be.Address(flatAddr);
        break;
      }
      case BRIG_SEGMENT_GROUP: {
        TypedReg initValueReg = be.AddTReg(be.PointerType());
        be.EmitMov(initValueReg->Reg(), be.Immed(initValueReg->Type(), initialValue), initValueReg->TypeSizeBits());
        be.EmitStore(segment, initValueReg, addr);
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
      be.EmitMov(r0->Reg(), be.Immed(wiID->Type(), 0), wiID->TypeSizeBits());
      cReg = be.AddTReg(BRIG_TYPE_B1);
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
///      be.EmitMov(src0, be.Immed(src0->Type(), 555));
///      be.EmitAtomic(src0, addr, dest, src1, atomicOp, memoryOrder, memoryScope, segment, isSigned, equivClass);
///    }
    be.EmitBarrier();
    // TODO: dest verification
///    TypedReg destOutReg = be.AddTReg(BRIG_TYPE_U32);
///    be.EmitCvt(destOutReg->Reg(), destOutReg->Type(), dest->Reg(), BRIG_TYPE_U64);
///    outDest->EmitStoreData(destOutReg);
    TypedReg addrReg = be.AddTReg(globalVar.type());
    be.EmitAtomic(addrReg, addr, NULL, NULL, BRIG_ATOMIC_LD, memoryOrder, memoryScope, segment, false, equivClass);
    // TODO: remove cvt after fixing ResultType() for return value based on Model (large|small)
    be.EmitCvt(result, addrReg);
    return result;
  }
};

void BarrierTests::Iterate(hexl::TestSpecIterator& it) {
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<BarrierTest>(ap, it, "barrier/atomics", cc->Grids().SeveralWavesInGroupSet(), cc->Memory().AllAtomics(), cc->Segments().Atomic(), cc->Memory().AllMemoryOrders(), cc->Memory().AllMemoryScopes(), Bools::All(), Bools::All());
}

}
