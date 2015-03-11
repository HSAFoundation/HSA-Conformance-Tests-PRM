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

#include "SignalTests.hpp"
#include "HsailRuntime.hpp"
#include "BrigEmitter.hpp"
#include "HCTests.hpp"
#include "Scenario.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace hexl::scenario;
using namespace scenario;
using namespace HSAIL_ASM;
using namespace Brig;

namespace hsail_conformance {
///// BASE (VALUE) /////
class SignalBaseTest : public Test {
protected:
  DirectiveVariable signalArg;
  BrigMemoryOrder memoryOrder;
  BrigAtomicOperation atomicOp;
  bool noret;
  bool isSigned;
  std::string baseName;
  hsa_signal_value_t initialValue;
  hsa_signal_value_t expectedValue;

public:
  SignalBaseTest(BrigMemoryOrder memoryOrder_, BrigAtomicOperation atomicOp_, bool noret_, bool isSigned_ = false)
    : memoryOrder(memoryOrder_), atomicOp(atomicOp_), noret(noret_), isSigned(isSigned_), baseName("value"),
      initialValue(1), expectedValue(0) { }

  void Name(std::ostream& out) const { out << baseName
                                           << "/" << memoryOrder2str(be.AtomicMemoryOrder(atomicOp, memoryOrder))
                                           << (isSigned ? "/signed" : "")
                                           << (noret ? "/noret" : "/ret")
                                           << "/" << atomicOperation2str(atomicOp); }

  BrigTypeX ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const { return Value(MV_UINT32, U32(1)); }

  void GeometryInit() {
    // TODO: check how signal tests work with non-trivial geometry and remove this to use default geometry.
    geometry = cc->Grids().TrivialGeometry();
  }

  void ScenarioInit() {
    Test::ScenarioInit();
    CommandSequence& commands0 = te->TestScenario()->Commands();
    commands0.CreateSignal("signal", initialValue);
    CommandSequence& commands1 = te->TestScenario()->Commands(1);
    commands0.StartThread(1);
    commands1.WaitSignal("signal", expectedValue);
  }

  hexl::Test* Create() {
    // not yet used
    GetContext()->Put(Defaults::SIGNAL_ATOMIC_ID, Value(MV_UINT32, atomicOp));
    return Test::Create();
  }

  void KernelArguments() {
    Test::KernelArguments();
    signalArg = be.EmitVariableDefinition("%signal", BRIG_SEGMENT_KERNARG, be.SignalType());
  }

  void SetupDispatch(DispatchSetup* dsetup) {
    Test::SetupDispatch(dsetup);
    dsetup->MSetup().Add(NewMValue(unsigned(dsetup->MSetup().Count()), "Signal", MEM_KERNARG, MV_EXPR, S("signal")));
  }

  bool IsValid() const {
    if (isSigned) {
      switch (atomicOp) {
        // 6.8.1. Explanation of Modifiers. Type (signed is aplicable only for ADD, SUB)
        case BRIG_ATOMIC_ADD:
        case BRIG_ATOMIC_SUB:
          return true;
        default:
          return false;
      }
    }
    if (BRIG_ATOMIC_ST == atomicOp) {
      // 6.8.1 ret mode is not applicable for ST
      if (!noret) {return false;}
      // 6.8.1 memory order for ST can be only:
      // rlx (relaxed) or screl (sequentially consistent release)
      if (memoryOrder != BRIG_MEMORY_ORDER_RELAXED &&
          memoryOrder != BRIG_MEMORY_ORDER_SC_RELEASE) {return false;}
    }
    // 6.8.1 noret mode is not applicable for EXCH
    if (noret && BRIG_ATOMIC_EXCH == atomicOp) {return false;}
    return true;
  }

  ValueType GetSignalValueType() {
    return te->CoreCfg()->IsLarge() ? MV_INT64 : MV_INT32;
  }

  hsa_signal_value_t GetExpectedSignalValue(hsa_signal_value_t &dest, hsa_signal_value_t signal_value, hsa_signal_value_t src0, hsa_signal_value_t src1 = 0) {
    dest = signal_value;
    switch (atomicOp) {
      case BRIG_ATOMIC_ST:
      case BRIG_ATOMIC_EXCH: return src0;
      case BRIG_ATOMIC_ADD:  return signal_value + src0;
      case BRIG_ATOMIC_AND:  return signal_value & src0;
      case BRIG_ATOMIC_OR:   return signal_value | src0;
      case BRIG_ATOMIC_XOR:  return signal_value ^ src0;
      case BRIG_ATOMIC_SUB:  return signal_value - src0;
      case BRIG_ATOMIC_CAS:  return (signal_value == src0) ? src1 : signal_value;
      default: assert(false); return 0;
    }
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    TypedReg signal = be.AddTReg(be.SignalType());
    be.EmitLoad(signalArg.segment(), signal, be.Address(signalArg));
    BrigTypeX vtype = be.SignalValueIntType(true);
    TypedReg dest = NULL, origin = NULL, src0 = be.AddTReg(vtype), c = be.AddCTReg(), src1 = NULL;
    if (!noret) {
        dest = be.AddTReg(vtype);
        origin = be.AddTReg(vtype);
        // preliminary load signal value to verify dest register after main signal operation
        be.EmitSignalOp(origin, signal, NULL, NULL, BRIG_ATOMIC_LD, memoryOrder);
    }
    hsa_signal_value_t immSrc0 = 0, immSrc1 = 0, immDest = 0;
    switch (atomicOp) {
      default: break;
      case BRIG_ATOMIC_XOR:
      case BRIG_ATOMIC_SUB:
      case BRIG_ATOMIC_CAS:
        immSrc0 = 1; break;
    }
    expectedValue = GetExpectedSignalValue(immDest, initialValue, immSrc0, immSrc1);
    if (BRIG_ATOMIC_CAS == atomicOp) {
      src1 = be.AddTReg(vtype);
      be.EmitMov(src1->Reg(), be.Immed(be.SignalValueBitType(), expectedValue), src1->TypeSizeBits());
    }
    // if register
    be.EmitMov(src0->Reg(), be.Immed(be.SignalValueBitType(), immSrc0), src0->TypeSizeBits());
    std::string passLabel = be.AddLabel();
    std::string failLabel = be.AddLabel();
    // main signal operation to test
    be.EmitSignalOp(dest, signal, src0, src1, atomicOp, memoryOrder, isSigned);
    // retriving testing signal operation result by loading signal value
    be.EmitSignalOp(src0, signal, NULL, NULL, BRIG_ATOMIC_LD, memoryOrder);
    be.EmitCmp(c->Reg(), src0, be.Immed(vtype, expectedValue), BRIG_COMPARE_EQ);
    be.EmitCbr(c, passLabel);
    be.EmitMov(result->Reg(), be.Immed(BRIG_TYPE_U32, 0), 32);
    be.EmitBr(failLabel);
    be.EmitLabel(passLabel);
    be.EmitMov(result->Reg(), be.Immed(BRIG_TYPE_U32, 1), 32);
    if (!noret) {
      be.EmitCmp(c->Reg(), dest, origin, BRIG_COMPARE_EQ);
      // dest == origin: ok, code 1
      be.EmitCbr(c, failLabel);
      // dest != origin: fail, code 2
      be.EmitMov(result->Reg(), be.Immed(BRIG_TYPE_U32, 2), 32);
    }
    be.EmitLabel(failLabel);
    return result;
  }
};

///// WAIT /////
class SignalWaitTest : public SignalBaseTest {
protected:
  uint64_t timeout;

public:
  SignalWaitTest(BrigMemoryOrder memoryOrder_, BrigAtomicOperation atomicOp_)
    : SignalBaseTest(memoryOrder_, atomicOp_, false), timeout(1000) { initialValue = 10; baseName = "wait"; }

  void ScenarioInit() {
    Test::ScenarioInit();
    CommandSequence& commands0 = te->TestScenario()->Commands(0);
    commands0.CreateSignal("signal");
    CommandSequence& commands1 = te->TestScenario()->Commands(1);
    commands0.StartThread(1);
    commands1.SendSignal("signal", initialValue);
  }

  TypedReg Result() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    TypedReg signal = be.AddTReg(be.SignalType());
    be.EmitLoad(signalArg.segment(), signal, be.Address(signalArg));
    BrigTypeX vtype = be.SignalValueIntType(true);
    TypedReg dest = be.AddTReg(vtype), src0 = be.AddTReg(vtype), acquired = be.AddTReg(vtype), c = be.AddCTReg();
    int64_t immSrc0;
    switch (atomicOp) {
    case BRIG_ATOMIC_WAIT_LT:
    case BRIG_ATOMIC_WAIT_NE:
    case BRIG_ATOMIC_WAITTIMEOUT_LT:
    case BRIG_ATOMIC_WAITTIMEOUT_NE:
      immSrc0 = 11; break;
    default:
      immSrc0 = initialValue; break;
    }
    be.EmitMov(src0->Reg(), be.Immed(be.SignalValueBitType(), immSrc0), src0->TypeSizeBits());
    // main signal operation to test, wrapped into loop
    be.EmitSignalWaitLoop(dest, signal, src0->Reg(), atomicOp, memoryOrder, timeout);
    be.EmitSignalOp(acquired, signal, NULL, NULL, BRIG_ATOMIC_LD, memoryOrder);
    be.EmitMov(result->Reg(), be.Immed(BRIG_TYPE_U32, 1), 32);
    be.EmitCmp(c->Reg(), dest, acquired, BRIG_COMPARE_EQ);
    std::string failLabel = be.AddLabel();
    // dest == acquired: ok, code 1
    be.EmitCbr(c, failLabel);
    // dest != acquired: fail, code 2
    be.EmitMov(result->Reg(), be.Immed(BRIG_TYPE_U32, 2), 32);
    be.EmitLabel(failLabel);
    return result;
  }
};

void SignalTests::Iterate(hexl::TestSpecIterator& it) {
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  static const char *base = "signal";
  TestForEach<SignalBaseTest>(ap, it, base, cc->Memory().SignalSendMemoryOrders(), cc->Memory().SignalSendAtomics(), Bools::All(), Bools::All());
  TestForEach<SignalWaitTest>(ap, it, base, cc->Memory().SignalWaitMemoryOrders(), cc->Memory().SignalWaitAtomics());
}

}
