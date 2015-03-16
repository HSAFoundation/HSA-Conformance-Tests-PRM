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

#include "UserModeQueueTests.hpp"
#include "HCTests.hpp"
#include "MObject.hpp"
#include "hsa.h"

using namespace hexl;
using namespace hexl::emitter;
using namespace hexl::scenario;
using namespace HSAIL_ASM;

namespace hsail_conformance {

class UserModeQueueTest : public Test {
protected:
  UserModeQueueType queueType;
  UserModeQueue queue;

public:
  UserModeQueueTest(Location codeLocation, UserModeQueueType queueType_)
    : Test(codeLocation), queueType(queueType_), queue(0)
  {
  }
  
  void Init() {
    Test::Init();
    queue = kernel->NewQueue("queue", queueType);
  }
};

class LdBasicIndexTest : public UserModeQueueTest {
protected:
  BrigOpcode opcode;
  BrigSegment segment;
  BrigMemoryOrder memoryOrder;

public:
  LdBasicIndexTest(UserModeQueueType queueType, BrigOpcode opcode_, BrigSegment segment_, BrigMemoryOrder memoryOrder_)
    : UserModeQueueTest(KERNEL, queueType),
    opcode(opcode_), segment(segment_), memoryOrder(memoryOrder_) { }

  void Name(std::ostream& out) const {
    out << opcode2str(opcode) << "/basic/" << opcode2str(opcode) << "_" << segment2str(segment) << "_" << memoryOrder2str(memoryOrder);
  }

  BrigType ResultType() const { return BRIG_TYPE_U64; }

  Value ExpectedResult() const { return Value(MV_UINT64, 1); }

  TypedReg Result() {
    TypedReg index = be.AddTReg(BRIG_TYPE_U64);
    switch (opcode) {
    case BRIG_OPCODE_LDQUEUEREADINDEX: queue->EmitLdQueueReadIndex(segment, memoryOrder, index); break;
    case BRIG_OPCODE_LDQUEUEWRITEINDEX: queue->EmitLdQueueWriteIndex(segment, memoryOrder, index); break;
    default:
      assert(0);
    }
    TypedReg result = be.AddTReg(ResultType());
    be.EmitCmpTo(result, index, be.Immed(index->Type(), 0), BRIG_COMPARE_EQ);
    return result;
  }
};

class AddCasBasicIndexTest : public UserModeQueueTest {
protected:
  BrigOpcode opcode;
  BrigSegment segment;
  BrigMemoryOrder memoryOrder;
  
public:
  AddCasBasicIndexTest(UserModeQueueType queueType, BrigOpcode opcode_, BrigSegment segment_, BrigMemoryOrder memoryOrder_)
    : UserModeQueueTest(KERNEL, queueType),
    opcode(opcode_), segment(segment_), memoryOrder(memoryOrder_) { }
    
  void Name(std::ostream& out) const {
    out << opcode2str(opcode) << "/basic/" << opcode2str(opcode) << "_" << segment2str(segment) << "_" << memoryOrder2str(memoryOrder);
  }

  BrigType ResultType() const { return BRIG_TYPE_U64; }

  Value ExpectedResult() const { return Value(MV_UINT64, 1); }

  TypedReg Result() {
    Operand src0, src1;
    TypedReg index = be.AddTReg(BRIG_TYPE_U64);
    src0 = be.Brigantine().createImmed(0,ResultType());
    switch (opcode) {
    case BRIG_OPCODE_ADDQUEUEWRITEINDEX:  queue->EmitAddQueueWriteIndex(segment, memoryOrder, index, src0); break;
    case BRIG_OPCODE_CASQUEUEWRITEINDEX: 
      src1 = be.Brigantine().createImmed(1,ResultType());
      queue->EmitCasQueueWriteIndex(segment, memoryOrder, index, src0, src1); 
      break;
    default:
      assert(0);
    }
    TypedReg result = be.AddTReg(ResultType());
    be.EmitCmpTo(result, index, be.Immed(index->Type(), 0), BRIG_COMPARE_EQ);
    return result;
  }
};

class StBasicIndexTest : public UserModeQueueTest {
protected:
  BrigOpcode opcode;
  BrigSegment segment;
  BrigMemoryOrder memoryOrder;

public:
  StBasicIndexTest(UserModeQueueType queueType, BrigOpcode opcode_, BrigSegment segment_, BrigMemoryOrder memoryOrder_)
    : UserModeQueueTest(KERNEL, queueType),
    opcode(opcode_), segment(segment_), memoryOrder(memoryOrder_) { }

  void Name(std::ostream& out) const {
    out << opcode2str(opcode) << "/trivial/" << opcode2str(opcode) << "_" << segment2str(segment) << "_" << memoryOrder2str(memoryOrder);
  }

  BrigType ResultType() const { return BRIG_TYPE_U64; }

  Value ExpectedResult() const { return Value(MV_UINT64, 1); }

  TypedReg Result() {
    TypedReg index = be.AddTReg(ResultType());
    be.EmitMov(index, be.Immed(index->Type(), 1));
    switch (opcode) {
    case BRIG_OPCODE_STQUEUEREADINDEX: queue->EmitStQueueReadIndex(segment, memoryOrder, index); break;
    case BRIG_OPCODE_STQUEUEWRITEINDEX: queue->EmitStQueueWriteIndex(segment, memoryOrder, index); break;
    default:
      assert(0);
    }
    be.EmitStore(index, queue->Address(segment), 0, false);
    TypedReg result = be.AddTReg(ResultType());
    be.EmitCmpTo(result, index, be.Immed(index->Type(), 1), BRIG_COMPARE_EQ);
    return result;
  }
};

/*
class AgentEnqueueKernelDispatchTest : public UserModeQueueTest {
private:
  Variables::Spec in, out;
  Variables::Spec completionSignalKernarg;
  Variables::Spec childKernarg;
  DirectiveKernel mainKernel;
  DirectiveKernel childKernel;

  static const char *CHILD_KERNEL_NAME;
public:
  AgentEnqueueKernelDispatchTest() 
    : UserModeQueueTest(KERNEL, DISPATCH_QUEUE),
      in(BRIG_SEGMENT_KERNARG, BRIG_TYPE_U64),
      out(BRIG_SEGMENT_KERNARG, BRIG_TYPE_U64),
      completionSignalKernarg(BRIG_SEGMENT_KERNARG, BRIG_TYPE_SIG64),
      childKernarg(BRIG_SEGMENT_GLOBAL, BRIG_TYPE_U32, 64, BRIG_ALIGNMENT_64)
  {
    specList.Add(&in); specList.Add(&out);
    specList.Add(&completionSignalKernarg);
    specList.Add(&childKernarg);
  }

  void Name(std::ostream& out) const {
    out << "kerneldispatch";
  }

  BrigType ResultType() const { return BRIG_TYPE_U32; }

  Value ExpectedResult() const { return Value(MV_UINT32, U32(1)); }

  void Module() {
    EmitChildKernel();
    Test::Module();
  }

  void ScenarioInit() {
    Test::ScenarioInit();
    CommandSequence& commands0 = te->TestScenario()->Commands(0);
    commands0.CreateSignal("completionSignal", 1);
    commands0.Finalize("code_child", Defaults::PROGRAM_ID, childKernel.brigOffset());
  }

  void ScenarioCodes() {
    te->TestScenario()->Commands().Finalize("code_child", Defaults::PROGRAM_ID, childKernel.brigOffset());
  }

  void EmitInitializeAql(PointerReg p, TypedReg kernelObject, PointerReg kernargAddress, TypedReg completionSignal) {
    //EmitDispatchStore(, p, header);
    //EmitDispatchStore(?, p, setup);
    EmitDispatchStoreI(BRIG_TYPE_U16, be.Immed(BRIG_TYPE_U16, geometry->WorkgroupSize(0)), p, workgroup_size_x);
    EmitDispatchStoreI(BRIG_TYPE_U16, be.Immed(BRIG_TYPE_U16, geometry->WorkgroupSize(1)), p, workgroup_size_y);
    EmitDispatchStoreI(BRIG_TYPE_U16, be.Immed(BRIG_TYPE_U16, geometry->WorkgroupSize(2)), p, workgroup_size_z);
    EmitDispatchStoreI(BRIG_TYPE_U16, be.Immed(BRIG_TYPE_U16, 0), p, reserved0);
    EmitDispatchStoreI(BRIG_TYPE_U32, be.Immed(BRIG_TYPE_U32, geometry->GridSize(0)), p, grid_size_x);
    EmitDispatchStoreI(BRIG_TYPE_U32, be.Immed(BRIG_TYPE_U32, geometry->GridSize(1)), p, grid_size_y);
    EmitDispatchStoreI(BRIG_TYPE_U32, be.Immed(BRIG_TYPE_U32, geometry->GridSize(2)), p, grid_size_z);
    EmitDispatchStoreI(BRIG_TYPE_U32, be.Immed(BRIG_TYPE_U32, 0), p, private_segment_size);
    EmitDispatchStoreI(BRIG_TYPE_U32, be.Immed(BRIG_TYPE_U32, 0), p, group_segment_size);
    EmitDispatchStore(kernelObject, p, kernel_object);
    EmitDispatchStore(kernargAddress, p, kernarg_address);
#ifndef HSA_LARGE_MODEL
    EmitDispatchStoreI(BRIG_TYPE_U32, be.Immed(BRIG_TYPE_U32, 0), p, reserved1);
#endif // HSA_LARGE_MODEL
    EmitDispatchStoreI(BRIG_TYPE_U64, be.Immed(BRIG_TYPE_U64, 0), p, reserved2);
    EmitDispatchStore(completionSignal, p, completion_signal);
  }

  void EmitReservePacket(PointerReg baseAddress, TypedReg* packetId, PointerReg* packet) {
    TypedReg packetId64 = be.AddTReg(BRIG_TYPE_U64);
    queue->EmitAddQueueWriteIndex(BRIG_SEGMENT_GLOBAL, BRIG_MEMORY_ORDER_RELAXED, packetId64, be.Immed(BRIG_TYPE_U64, 1));
    // TODO: check overflow and loop

    if (packetId64->Type() == be.PointerType()) {
      *packetId = packetId64;
    } else {
      *packetId = be.AddAReg();
      be.EmitCvt(*packetId, packetId64);
    }

    *packet = be.AddAReg();
    be.EmitArith(BRIG_OPCODE_MUL, *packet, *packetId, be.Immed(be.PointerType(), 8));
    be.EmitArith(BRIG_OPCODE_ADD, *packet, baseAddress, (*packet)->Reg());
  }

  TypedReg EmitChildKernelObject() {
    TypedReg result = be.AddTReg(BRIG_TYPE_U64);
    PointerReg kernelDescriptorArray = be.AddAReg();
    be.EmitLdk(kernelDescriptorArray, CHILD_KERNEL_NAME);
    TypedReg offset = be.AddTReg(BRIG_TYPE_U32);
    be.EmitAgentId(offset);
    be.EmitArith(BRIG_OPCODE_MUL, offset, offset, be.Immed(BRIG_TYPE_U32, 8));
    TypedReg offset1;
    if (kernelDescriptorArray->Type() != offset->Type()) {
      offset1 = be.AddAReg();
      be.EmitCvt(offset1, offset);
    } else {
      offset1 = offset;
    }
    be.EmitArith(BRIG_OPCODE_ADD, kernelDescriptorArray, kernelDescriptorArray, offset1->Reg());
    be.EmitLoad(result, kernelDescriptorArray);
    return result;
  }

  PointerReg EmitChildKernarg() {
    PointerReg kernarg = be.AddAReg();
    be.EmitLda(kernarg, childKernarg.Variable());
    PointerReg mainIn = be.AddAReg();
    in.EmitLoadTo(mainIn);
    be.EmitStore(BRIG_SEGMENT_GLOBAL, mainIn, be.Address(kernarg, 0));
    PointerReg mainOut = be.AddAReg();
    out.EmitLoadTo(mainOut);
    be.EmitStore(BRIG_SEGMENT_GLOBAL, mainOut, be.Address(kernarg, 8));
    return kernarg;
  }

  void SetupExtraKernelArguments(hexl::DispatchSetup* dsetup) {
    unsigned id = dsetup->MSetup().Count();
    uint32_t sizes[] = { geometry->GridSize32(), 1, 1 };
    MBuffer* min = new MBuffer(id++, "Input", MEM_GLOBAL, MV_UINT32, 1, sizes);
    dsetup->MSetup().Add(min);
    dsetup->MSetup().Add(NewMValue(id++, "Input (arg)", MEM_KERNARG, MV_REF, R(min->Id())));
    MBuffer* mout = new MBuffer(id++, "Output", MEM_GLOBAL, MV_UINT32, 1, sizes);
    dsetup->MSetup().Add(mout);
    dsetup->MSetup().Add(NewMValue(id++, "Output (arg)", MEM_KERNARG, MV_REF, R(mout->Id())));
    MBuffer* signal = NewMValue(id++, "Completion signal (arg)", MEM_KERNARG, MV_EXPR, S("completionSignal"));
    dsetup->MSetup().Add(signal);
  }
  
  TypedReg Result() {
    mainKernel = be.CurrentKernel();
    PointerReg baseAddress = queue->BaseAddress();
    TypedReg packetId;
    PointerReg packet;
    EmitReservePacket(baseAddress, &packetId, &packet);


    PointerReg kernargAddress = EmitChildKernarg();

    TypedReg childKernelObject = EmitChildKernelObject();

    TypedReg completionSignal = be.AddTReg(be.SignalType());
    completionSignalKernarg.EmitLoadTo(completionSignal);

    EmitInitializeAql(packet, childKernelObject, kernargAddress, completionSignal);

    uint16_t header =
      (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE) |
      (1 << HSA_PACKET_HEADER_BARRIER) |
      (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
      (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);
    uint16_t setup = 
      (geometry->NDim() << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS);
    be.EmitStore(BRIG_SEGMENT_GLOBAL, BRIG_TYPE_U32, be.Immed(BRIG_TYPE_U32, header | (setup << 16)), be.Address(packet));
    be.EmitSignalOp(queue->DoorbellSignal(), packetId, 0, BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_SC_RELEASE);
    TypedReg waitResult = be.AddTReg(be.SignalValueIntType());
    Operand expected = be.Immed(be.SignalValueIntType(), 0);
    be.EmitSignalWaitLoop(waitResult, completionSignal, expected, BRIG_ATOMIC_WAIT_EQ, BRIG_MEMORY_ORDER_SC_ACQUIRE);
    TypedReg result = be.AddTReg(BRIG_TYPE_U32);
    be.EmitCmpTo(result, waitResult, expected, BRIG_COMPARE_EQ);
    return result;
  }

  void EmitChildKernel() {
    childKernel = be.StartKernel(CHILD_KERNEL_NAME);
    DirectiveVariable in = be.EmitVariableDefinition(be.IName(), BRIG_SEGMENT_KERNARG, be.PointerType());
    DirectiveVariable out = be.EmitVariableDefinition(be.OName(), BRIG_SEGMENT_KERNARG, be.PointerType());
    be.StartBody();
    TypedReg data = be.AddTReg(BRIG_TYPE_U32);
    be.EmitLoadFromBuffer(data, in);
    be.EmitArith(BRIG_OPCODE_ADD, data, data, be.Immed(data->Type(), 1));
    be.EmitStoreToBuffer(data, out);
    be.EndKernel();
  }
};

const char *AgentEnqueueKernelDispatchTest::CHILD_KERNEL_NAME = "&child_kernel";
*/

void UserModeQueueTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  Arena* ap = cc->Ap();
  TestForEach<LdBasicIndexTest>(ap, it, Path(), cc->Queues().Types(), cc->Queues().LdOpcodes(), cc->Queues().Segments(), cc->Queues().LdMemoryOrders());
  TestForEach<AddCasBasicIndexTest>(ap, it, Path(), cc->Queues().Types(), cc->Queues().AddCasOpcodes(), cc->Queues().Segments(), cc->Queues().AddCasMemoryOrders());
  TestForEach<StBasicIndexTest>(ap, it, Path(), cc->Queues().Types(), cc->Queues().StOpcodes(), cc->Queues().Segments(), cc->Queues().StMemoryOrders());
  //it(Path() + "/agentenqueue", new AgentEnqueueKernelDispatchTest());
}

}
