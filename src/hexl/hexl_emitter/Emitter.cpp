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

#include "Emitter.hpp"
#include "BrigEmitter.hpp"
#include "Scenario.hpp"
#include "MObject.hpp"
#include "Sequence.hpp"
#include "CoreConfig.hpp"
#include "hsa.h"

using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl::scenario;

namespace hexl {

namespace Bools {
  Sequence<bool>* All() {
    static class : public Sequence<bool> {
    public:
      void Iterate(Action<bool>& a) const {
        a(false);
        a(true);
      }
    } bools;
    return &bools;
  }
}

std::string Dir2Str(BrigControlDirective d)
{
  switch(d) {
  case BRIG_CONTROL_REQUIREDDIM:
    return "RD";
  case BRIG_CONTROL_REQUIREDGRIDSIZE:
    return "RGS";
  case BRIG_CONTROL_REQUIREDWORKGROUPSIZE:
    return "RWS";
  case BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS:
    return "RNPW";
  case BRIG_CONTROL_MAXFLATGRIDSIZE:
    return "MFGS";
  case BRIG_CONTROL_MAXFLATWORKGROUPSIZE:
    return "MFWS";
  default:
    assert(false); return "UNKND";
  }
}

namespace emitter {

const char *LocationString(Location l)
{
  switch (l) {
  case KERNEL: return "kernel";
  case FUNCTION: return "function";
  case MODULE: return "module";
  default: assert(false); return "<invalid location>";
  }
}

Sequence<Location>* CodeLocations()
{
  static const Location locations[] = { KERNEL, FUNCTION };
  static ArraySequence<Location> sequence(locations, NELEM(locations));
  return &sequence;
}

Sequence<Location>* KernelLocation()
{
  static const Location locations[] = { KERNEL, };
  static ArraySequence<Location> sequence(locations, NELEM(locations));
  return &sequence;
}

Grid Emittable::Geometry() { return te->InitialContext()->Get<Grid>("geometry"); }

void EmittableContainer::Name(std::ostream& out) const
{
  for (size_t i = 0; i < list.size(); ++i) {
    list[i]->Name(out);
    if (i != list.size() - 1) { out << "_"; }
  }
}

Variable EmittableContainer::NewVariable(const std::string& id, Brig::BrigSegment segment, Brig::BrigTypeX type, Location location, Brig::BrigAlignment align, uint64_t dim, bool isConst, bool output)
{
  Variable var = te->NewVariable(id, segment, type, location, align, dim, isConst, output);
  Add(var);
  return var;
}

Variable EmittableContainer::NewVariable(const std::string& id, VariableSpec varSpec)
{
  Variable var = te->NewVariable(id, varSpec);
  Add(var);
  return var;
}

Variable EmittableContainer::NewVariable(const std::string& id, VariableSpec varSpec, bool output)
{
  Variable var = te->NewVariable(id, varSpec, output);
  Add(var);
  return var;
}

Buffer EmittableContainer::NewBuffer(const std::string& id, BufferType type, ValueType vtype, size_t count)
{
  Buffer buffer = te->NewBuffer(id, type, vtype, count);
  Add(buffer);
  return buffer;
}

UserModeQueue EmittableContainer::NewQueue(const std::string& id, UserModeQueueType type)
{
  UserModeQueue queue = te->NewQueue(id, type);
  Add(queue);
  return queue;
}

Kernel EmittableContainer::NewKernel(const std::string& id)
{
  Kernel kernel = te->NewKernel(id);
  Add(kernel);
  return kernel;
}

Function EmittableContainer::NewFunction(const std::string& id)
{
  Function function = te->NewFunction(id);
  Add(function);
  return function;
}

bool EVariableSpec::IsValidVar() const
{
  //if (IsEmpty()) { return true; }
  if (type == BRIG_TYPE_B1) { return false; } // Cannot declare variable of type b1
  if (align < getNaturalAlignment(type)) { return false; }
  return true;
}

bool EVariableSpec::IsValid() const
{
  return IsValidAt(location);
}

bool EVariableSpec::IsValidAt(Location location) const
{
  if (!IsValidVar()) { return false; }
  if (location == MODULE && (segment == BRIG_SEGMENT_ARG || segment == BRIG_SEGMENT_KERNARG || segment == BRIG_SEGMENT_SPILL)) { return false; }
  if (location != MODULE && (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_READONLY)) { return false; } // Finalizer currently only supports global segment variables at module scope.
  if (location == FUNCTION && segment == BRIG_SEGMENT_KERNARG) { return false; }
  return true;
}

void EVariableSpec::Name(std::ostream& out) const
{
  if (!this) { out << "empty"; return; }
  out << segment2str(segment) << "_" << typeX2str(type);
  if (dim > 0) { out << "[" << dim << "]"; }
  out << "_align(" << align2num(align) << ")";
  if (location != AUTO) { out << "@" << LocationString(location); }
}

Location EVariable::RealLocation() const
{
  if (location == AUTO) {
    switch (segment) {
    case BRIG_SEGMENT_GLOBAL:
      return MODULE;
    case BRIG_SEGMENT_PRIVATE:
    case BRIG_SEGMENT_SPILL:
    case BRIG_SEGMENT_GROUP:
    case BRIG_SEGMENT_EXTSPACE0:
    case BRIG_SEGMENT_KERNARG:
      return KERNEL;
    case BRIG_SEGMENT_ARG:
      return FUNCTION;
    default:
      assert(!"Unsupported AUTO in RealLocation()");
      return AUTO;
    }
  } else {
    return location;
  }
}

void EVariable::ModuleVariables()
{
  if (RealLocation() == MODULE) {
    EmitDefinition();
  }
}

void EVariable::KernelVariables()
{
  if (RealLocation() == KERNEL && segment != BRIG_SEGMENT_KERNARG) {
    EmitDefinition();
  }
}

void EVariable::FunctionVariables()
{
  if (RealLocation() == FUNCTION && segment != BRIG_SEGMENT_ARG) {
    EmitDefinition();
  }
}

void EVariable::KernelArguments()
{
  if (RealLocation() == KERNEL && segment == BRIG_SEGMENT_KERNARG) {
    EmitDefinition();
  }
}

void EVariable::FunctionFormalOutputArguments()
{
  if (RealLocation() == FUNCTION && segment == BRIG_SEGMENT_ARG && output) {
    EmitDefinition();
  }
}

void EVariable::FunctionFormalInputArguments()
{
  if (RealLocation() == FUNCTION && segment == BRIG_SEGMENT_ARG && !output) {
    EmitDefinition();
  }
}

std::string EVariable::VariableName() const
{
  switch (RealLocation()) {
  case MODULE:
    return "&" + id;
  default:
    return "%" + id;
  }
}

void EVariable::Name(std::ostream& out) const
{
  if (isConst) { out << "const_"; }
  out << segment2str(segment) << "_" << typeX2str(type);
  if (dim > 0) { out << "[" << dim << "]"; }
  out << "_align(" << align2num(align) << ")";
  if (location != AUTO) { out << "@" << LocationString(location); }
}

TypedReg EVariable::AddDataReg()
{
  assert(dim < (uint64_t) 16); // Let's be reasonable.
  return te->Brig()->AddTReg(type, (uint32_t) dim);
}

void EVariable::EmitDefinition()
{
  assert(!var);
  var = te->Brig()->EmitVariableDefinition(VariableName(), segment, type, align, dim, isConst, output);
  EmitInitializer();
}

void EVariable::EmitInitializer()
{
  assert(var);
  if (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_READONLY) { 
    if (data.numBytes() != 0) {
      te->Brig()->EmitVariableInitializer(var, data.toSRef());
    }
  } // ToDo: create initializers for other segments with help of MObjects
}

void EVariable::EmitLoadTo(TypedReg dst, bool useVectorInstructions)
{
  te->Brig()->EmitLoad(segment, dst, te->Brig()->Address(Variable()), useVectorInstructions);
}

void EVariable::EmitStoreFrom(TypedReg src, bool useVectorInstructions)
{
  te->Brig()->EmitStore(segment, src, te->Brig()->Address(Variable()), useVectorInstructions);
}

void EControlDirectives::Name(std::ostream& out) const
{
  out << spec;
}

void EControlDirectives::Emit()
{
  class EmitAction: public Action<BrigControlDirective> {
  private:
    TestEmitter* te;
    Grid geometry;

  public:
    EmitAction(TestEmitter *te_, Grid geometry_) : te(te_), geometry(geometry_) { }

    void operator()(const BrigControlDirective& d) {
      te->Brig()->EmitControlDirectiveGeometry(d, geometry);
    }
  };
  EmitAction emitAction(te, te->InitialContext()->Get<Grid>("geometry"));
  spec->Iterate(emitAction);
}

void EControlDirectives::FunctionDirectives()
{
  Emit();
}

void EControlDirectives::KernelDirectives()
{
  Emit();
}

size_t EBuffer::Size() const 
{
  return count * ValueTypeSize(vtype);
}

HSAIL_ASM::DirectiveVariable EBuffer::EmitAddressDefinition(BrigSegment segment)
{
  return te->Brig()->EmitVariableDefinition(id, segment, te->Brig()->PointerType());
}

void EBuffer::EmitBufferDefinition()
{
  assert(0);
}

void EBuffer::KernelArguments()
{
  switch (type) {
  case HOST_INPUT_BUFFER:
  case HOST_RESULT_BUFFER:
    variable = EmitAddressDefinition(BRIG_SEGMENT_KERNARG);
    break;
  default:
    break;
  }
}

void EBuffer::KernelVariables()
{
  switch (type) {
  case KERNEL_BUFFER:
    EmitBufferDefinition();
    break;
  default:
    break;
  }
}

void EBuffer::SetupDispatch(DispatchSetup* dsetup)
{
  MBuffer* mout = 0;
  switch (type) {
  case HOST_INPUT_BUFFER:
  case HOST_RESULT_BUFFER:
    {
      unsigned i = dsetup->MSetup().Count();
      uint32_t sizes[] = { (uint32_t) count, 1, 1 };
      mout = new MBuffer(i++, id + ".buffer", MEM_GLOBAL, vtype, 1, sizes);
      dsetup->MSetup().Add(mout);
      dsetup->MSetup().Add(NewMValue(i++, id + ".kernarg", MEM_KERNARG, MV_REF, U64(mout->Id())));
      break;
    }
  default:
    break;
  }
  switch (type) {
  case HOST_INPUT_BUFFER:
    {
      if (data) { mout->Data() = *data; }
      break;
    }
  case HOST_RESULT_BUFFER:
    {
      MRBuffer *mrout = new MRBuffer(dsetup->MSetup().Count(), id + ".result", mout->VType(), mout->Id());
      if (data) { mrout->Data() = *data; }
      dsetup->MSetup().Add(mrout);
      break;
    }
  default:
    break;
  }
}

void EBuffer::ScenarioInit()
{
}

void EBuffer::Validation()
{
}

TypedReg EBuffer::AddDataReg()
{
  return te->Brig()->AddTReg(Value2BrigType(vtype));
}

DirectiveVariable EBuffer::Variable()
{
  switch (type) {
  case HOST_INPUT_BUFFER:
  case HOST_RESULT_BUFFER:
    assert(variable);
    return variable;
  default:
    assert(0); return DirectiveVariable();
  }
}

PointerReg EBuffer::Address(bool flat)
{
  unsigned i = flat ? 1 : 0;
  if (!address[i]) {
    switch (type) {
    case HOST_INPUT_BUFFER:
    case HOST_RESULT_BUFFER:
      address[i] = AddAReg(flat);
      te->Brig()->EmitLoad(BRIG_SEGMENT_KERNARG, address[i], te->Brig()->Address(Variable()));
      break;
    default:
      assert(0);
    }
  }
  return address[i];
}

PointerReg EBuffer::AddAReg(bool flat)
{
  switch (type) {
  case HOST_INPUT_BUFFER:
  case HOST_RESULT_BUFFER:
    return te->Brig()->AddAReg(flat ? BRIG_SEGMENT_FLAT : BRIG_SEGMENT_GLOBAL);
  default:
    assert(0); return 0;
  }
}

OperandAddress EBuffer::DataAddress(TypedReg index, bool flat, uint64_t count)
{
  PointerReg address = Address(flat);
  PointerReg fullAddress = AddAReg(flat);
//  te->Brig()->EmitArith(BRIG_OPCODE_MAD, fullAddress, te->Brig()->WorkitemFlatAbsId(address->IsLarge()), te->Brig()->Immed(address->Type(), TypeSize()), address);
  te->Brig()->EmitArith(BRIG_OPCODE_MAD, fullAddress, index, te->Brig()->Immed(address->Type(), TypeSize() * count), address);
  return te->Brig()->Address(fullAddress);
}

void EBuffer::EmitLoadData(TypedReg dest, TypedReg index, bool flat)
{
  switch (type) {
  case HOST_INPUT_BUFFER:
    te->Brig()->EmitLoad(flat ? BRIG_SEGMENT_FLAT : BRIG_SEGMENT_GLOBAL, dest, DataAddress(index, flat, dest->Count()));
    break;
  default:
    assert(0);
    break;
  }
}

void EBuffer::EmitLoadData(TypedReg dest, bool flat)
{
  TypedReg index = te->Brig()->EmitWorkitemFlatAbsId(Address(flat)->IsLarge());
  EmitLoadData(dest, index, flat);
}

void EBuffer::EmitStoreData(TypedReg src, TypedReg index, bool flat)
{
  switch (type) {
  case HOST_RESULT_BUFFER:
    te->Brig()->EmitStore(flat ? BRIG_SEGMENT_FLAT : BRIG_SEGMENT_GLOBAL, src, DataAddress(index, flat, src->Count()));
    break;
  default:
    assert(0);
    break;
  }
}

void EBuffer::EmitStoreData(TypedReg src, bool flat)
{
  TypedReg index = te->Brig()->EmitWorkitemFlatAbsId(Address(flat)->IsLarge());
  EmitStoreData(src, index, flat);
}

void EUserModeQueue::KernelArguments()
{
  switch (type) {
  case SEPARATE_QUEUE:
    queueKernelArg = te->Brig()->EmitVariableDefinition("%queue", BRIG_SEGMENT_KERNARG, te->Brig()->PointerType());
    break;
  default: break;
  }
}

void EUserModeQueue::ScenarioInit()
{
  switch (type) {
  case SEPARATE_QUEUE:
    te->TestScenario()->Commands().CreateQueue(id);
    break;
  default:
    break;
  }
}

void EUserModeQueue::SetupDispatch(hexl::DispatchSetup* dsetup)
{
  Emittable::SetupDispatch(dsetup);
  switch (type) {
  case SEPARATE_QUEUE:
    dsetup->MSetup().Add(NewMValue(dsetup->MSetup().Count(), "Queue", MEM_KERNARG, MV_EXPR, S(id.c_str())));
    break;
  default:
    break;
  }
}

void EUserModeQueue::StartKernelBody()
{
  switch (type) {
  case SEPARATE_QUEUE:
    assert(!address);
    address = te->Brig()->AddAReg();
    te->Brig()->EmitLoad(BRIG_SEGMENT_KERNARG, address, te->Brig()->Address(queueKernelArg));
    break;
  case DISPATCH_QUEUE:
    assert(!address);
    address = te->Brig()->AddAReg();
    te->Brig()->EmitQueuePtr(address);
    break;
  default:
    break;
  }
}

PointerReg EUserModeQueue::Address(Brig::BrigSegment segment)
{
  switch (segment) {
  case BRIG_SEGMENT_GLOBAL:
    assert(address);
    return address;
  case BRIG_SEGMENT_FLAT:
    if (!flatAddress) {
      flatAddress = te->Brig()->AddAReg(BRIG_SEGMENT_FLAT);
      te->Brig()->EmitStof(flatAddress, Address(BRIG_SEGMENT_GLOBAL));
    }
    return flatAddress;
  default:
    assert(0); return 0;
  }
}

TypedReg EUserModeQueue::DoorbellSignal()
{
  if (!doorbellSignal) { doorbellSignal = EmitLoadDoorbellSignal(); }
  return doorbellSignal;
}

TypedReg EUserModeQueue::EmitLoadDoorbellSignal()
{
  TypedReg result = te->Brig()->AddTReg(te->Brig()->SignalType());
  EmitStructLoad(result, Address(), hsa_queue_t, doorbell_signal);
  return result;
}

TypedReg EUserModeQueue::Size()
{
  if (!size) { size = EmitLoadSize(); }
  return size;
}

TypedReg EUserModeQueue::EmitLoadSize()
{
  TypedReg result = te->Brig()->AddTReg(BRIG_TYPE_U32);
  EmitStructLoad(result, Address(), hsa_queue_t, size);
  return result;
}

PointerReg EUserModeQueue::ServiceQueue()
{
  if (!serviceQueue) { serviceQueue = EmitLoadServiceQueue(); }
  return serviceQueue;
}

PointerReg EUserModeQueue::EmitLoadServiceQueue()
{
  PointerReg result = te->Brig()->AddAReg();
  EmitStructLoad(result, Address(), hsa_queue_t, service_queue);
  return result;
}

PointerReg EUserModeQueue::BaseAddress()
{
  if (!baseAddress) { baseAddress = EmitLoadBaseAddress(); }
  return baseAddress;
}

PointerReg EUserModeQueue::EmitLoadBaseAddress()
{
  PointerReg result = te->Brig()->AddAReg();
  EmitStructLoad(result, Address(), hsa_queue_t, base_address);
  return result;
}

void EUserModeQueue::EmitLdQueueReadIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest)
{
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_LDQUEUEREADINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(Address(segment)));
}

void EUserModeQueue::EmitLdQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest)
{
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_LDQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(Address(segment)));
}

void EUserModeQueue::EmitStQueueReadIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg src)
{
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_STQUEUEREADINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(te->Brig()->Address(Address(segment)), src->Reg());
}

void EUserModeQueue::EmitStQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg src)
{
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_STQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(te->Brig()->Address(Address(segment)), src->Reg());
}

void EUserModeQueue::EmitAddQueueWriteIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg dest, Operand src)
{
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_ADDQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(Address(segment)), src);
}

void EUserModeQueue::EmitCasQueueWriteIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg dest, Operand src0, Operand src1)
{
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_CASQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(Address(segment)), src0, src1);
}

void ESignal::ScenarioInit()
{
  te->TestScenario()->Commands().CreateSignal(id, initialValue);
}

void ESignal::KernelArguments()
{
  kernelArg = te->Brig()->EmitVariableDefinition(id, BRIG_SEGMENT_KERNARG, te->Brig()->SignalType());
}

void ESignal::SetupDispatch(DispatchSetup* dispatch)
{
  dispatch->MSetup().Add(NewMValue(dispatch->MSetup().Count(), id, MEM_KERNARG, MV_EXPR, S(id.c_str())));
}

void EKernel::StartKernel()
{
  kernel = te->Brig()->StartKernel(KernelName());
}

void EKernel::EndKernel()
{
  EmittableContainer::EndKernel();
  te->Brig()->EndKernel();
}

void EKernel::StartKernelBody()
{
  te->Brig()->StartBody();
  EmittableContainer::StartKernelBody();
}

void EFunction::StartFunction()
{
  function = te->Brig()->StartFunction(FunctionName());
}

void EFunction::EndFunction()
{
  te->Brig()->EndFunction();
}

void EFunction::StartFunctionBody()
{
  te->Brig()->StartBody();
}

const char* ConditionType2Str(ConditionType type)
{
  switch (type) {
  case COND_BINARY:
    return "bin";
  case COND_SWITCH:
    return "switch";
  default:
    assert(0); return "<invalid condition type>";
  }
}

const char* ConditionInput2Str(ConditionInput input)
{
  switch (input) {
  case COND_HOST_INPUT:
    return "inp";
  case COND_IMM_PATH0:
    return "imm0";
  case COND_IMM_PATH1:
    return "imm1";
  case COND_WAVESIZE:
    return "wsz";
  default:
    assert(0); return "<invalid condition input>";
  }
}

void ECondition::Name(std::ostream& out) const
{
  out << ConditionType2Str(type) << "_" << ConditionInput2Str(input) << "_" << width2str(width);
}
 
void ECondition::Reset(TestEmitter* te)
{
  Emittable::Reset(te);
  id = "";
  kernarg = DirectiveVariable();
  funcarg = DirectiveVariable();
  kerninp = 0; funcinp = 0;
  condBuffer = 0;
}

std::string ECondition::Id()
{
  if (id.empty()) {
    id = te->Brig()->AddName("cond");
  }
  return id;
}

void ECondition::Init()
{
  switch (input) {
  case COND_HOST_INPUT: {
    ValueType ivtype = Brig2ValueType(itype);
    condBuffer = te->NewBuffer(Id(), HOST_INPUT_BUFFER, ivtype, Geometry()->GridSize());
    for (uint64_t i = 0; i < Geometry()->GridSize(); ++i) {
      condBuffer->AddData(Value(ivtype, InputValue(i)));
    }
    break;
  }
  default:
    break;
  }
}

bool ECondition::IsTrueFor(uint64_t wi)
{
  assert(type == COND_BINARY);
  switch (input) {
  case COND_HOST_INPUT:
    return InputValue(wi) != 0;
  case COND_IMM_PATH0:
    return false;
  case COND_IMM_PATH1:
  case COND_WAVESIZE:
    return true;
  default:
    assert(0); return false;
  }
}

bool ECondition::ExpectThenPath(uint64_t wi)
{
  return IsTrueFor(wi);
}

void ECondition::KernelArguments()
{
  if (condBuffer) { condBuffer->KernelArguments(); }
}

void ECondition::KernelVariables()
{
  if (condBuffer) { condBuffer->KernelVariables(); }
}

void ECondition::KernelInit()
{
  if (condBuffer) { condBuffer->KernelInit(); }
  switch (input) {
  case COND_HOST_INPUT:
    {
      kerninp = condBuffer->AddDataReg();
      condBuffer->EmitLoadData(kerninp);
      break;
    }
  default:
    break;
  }
}

void ECondition::FunctionFormalInputArguments()
{
  switch (input) {
  case COND_HOST_INPUT:
    funcarg = te->Brig()->EmitVariableDefinition(Id(), BRIG_SEGMENT_ARG, itype);
    break;
  default:
    break;
  }
}

void ECondition::FunctionInit()
{
  switch (input) {
  case COND_HOST_INPUT:
    {
      assert(funcarg != 0);
      funcinp = condBuffer->AddDataReg();
      te->Brig()->EmitLoad(BRIG_SEGMENT_ARG, funcinp, te->Brig()->Address(funcarg));        
      break;
    }
  default: break;
  }
}

void ECondition::SetupDispatch(DispatchSetup* dsetup)
{
  if (condBuffer) { condBuffer->SetupDispatch(dsetup); }
}

void ECondition::ScenarioInit()
{
  if (condBuffer) { condBuffer->ScenarioInit(); }
}

void ECondition::Validation()
{
  if (condBuffer) { condBuffer->Validation(); }
}

TypedReg ECondition::InputData()
{
  assert(input == COND_HOST_INPUT);
  if (DirectiveKernel k = te->Brig()->CurrentExecutable()) {
    assert(kerninp);
    return kerninp;
  } else {
    assert(funcinp);
    return funcinp;
  }
}

Operand ECondition::CondOperand()
{
  assert(type == COND_BINARY);
  switch (input) {
  case COND_HOST_INPUT:
    {
      TypedReg c = te->Brig()->AddCTReg();
      te->Brig()->EmitCvt(c, InputData());
      return c->Reg();
    }
  case COND_IMM_PATH0:
    return te->Brig()->Immed(BRIG_TYPE_B1, 0);
  case COND_IMM_PATH1:
    return te->Brig()->Immed(BRIG_TYPE_B1, 1);
  case COND_WAVESIZE:
    return te->Brig()->Wavesize();
  default: assert(0); return Operand();
  }
}

Operand ECondition::EmitIfCond()
{
  return CondOperand();
}

void ECondition::ActualCallArguments(TypedRegList inputs, TypedRegList outputs)
{
  Emittable::ActualCallArguments(inputs, outputs);
  switch (input) {
  case COND_HOST_INPUT:
    inputs->Add(InputData());
  default:
    break;
  }
}

void ECondition::EmitIfThenStart()
{
  lThen = te->Brig()->AddLabel();
  lEnd = te->Brig()->AddLabel();
  te->Brig()->EmitCbr(EmitIfCond(), lThen, width);
  te->Brig()->EmitBr(lEnd);
  te->Brig()->EmitLabel(lThen);
}

void ECondition::EmitIfThenEnd()
{
  te->Brig()->EmitLabel(lEnd);
}

void ECondition::EmitIfThenElseStart()
{
  lThen = te->Brig()->AddLabel();
  lElse = te->Brig()->AddLabel();
  lEnd = te->Brig()->AddLabel();
  te->Brig()->EmitCbr(EmitIfCond(), lThen, width);
  te->Brig()->EmitBr(lElse);
  te->Brig()->EmitLabel(lThen);
}

void ECondition::EmitIfThenElseOtherwise()
{
  te->Brig()->EmitBr(lEnd);
  te->Brig()->EmitLabel(lElse);
}

void ECondition::EmitIfThenElseEnd()
{
  te->Brig()->EmitLabel(lEnd);
}

Operand ECondition::EmitSwitchCond()
{
  assert(type == COND_SWITCH);
  switch (input) {
  case COND_HOST_INPUT:
    return InputData()->Reg();
  case COND_IMM_PATH0:
    return te->Brig()->Immed(itype, 0);
  case COND_IMM_PATH1:
    return te->Brig()->Immed(itype, 1);
  case COND_WAVESIZE:
    return te->Brig()->Wavesize();
  default:
    assert(0); return Operand();
  }
}

void ECondition::EmitSwitchStart()
{
  for (unsigned i = 0; i < SwitchBranchCount(); ++i) {
    labels.push_back(te->Brig()->AddLabel());
  }
  lEnd = te->Brig()->AddLabel(); // End
  te->Brig()->EmitSbr(itype, EmitSwitchCond(), labels, width);
}

void ECondition::EmitSwitchBranchStart(unsigned i)
{
  te->Brig()->EmitBr(lEnd);
  te->Brig()->EmitLabel(labels[i]);
}

void ECondition::EmitSwitchEnd()
{
  //te->Brig()->EmitBr(lEnd);
  te->Brig()->EmitLabel(lEnd);
}

unsigned ECondition::SwitchBranchCount()
{
  switch (input) {
  case COND_HOST_INPUT: return 16;
  case COND_IMM_PATH0: return 2;
  case COND_IMM_PATH1: return 3;
  case COND_WAVESIZE: return te->CoreCfg()->Wavesize() + 1;
  default: assert(0); return 0;
  }
}

unsigned ECondition::ExpectedSwitchPath(uint64_t i)
{
  switch (input) {
  case COND_HOST_INPUT:
    return InputValue(i, width) + 1;
  case COND_IMM_PATH0:
    return 0 + 1;
  case COND_IMM_PATH1:
    return 1 + 1;
  case COND_WAVESIZE:
    return te->CoreCfg()->Wavesize() + 1;
  default:
    assert(0); return 1024; 
  }
}

uint32_t ECondition::InputValue(uint64_t wi, BrigWidth width)
{
  if (width == BRIG_WIDTH_NONE) { width = this->width; }
  uint32_t ewidth = 0;
  switch (width) {
    case BRIG_WIDTH_1:  ewidth = 1; break;
    case BRIG_WIDTH_2:  ewidth = 2; break;
    case BRIG_WIDTH_4:  ewidth = 4; break;
    case BRIG_WIDTH_8:  ewidth = 8; break;
    case BRIG_WIDTH_16: ewidth = 16; break;
    case BRIG_WIDTH_32: ewidth = 32; break;
    case BRIG_WIDTH_64: ewidth = 64; break;
    case BRIG_WIDTH_128: ewidth = 128; break;
    case BRIG_WIDTH_256: ewidth = 256; break;
    case BRIG_WIDTH_WAVESIZE: ewidth = te->CoreCfg()->Wavesize(); break;
    case BRIG_WIDTH_ALL: ewidth = Geometry()->WorkgroupSize(); break;
    default: assert(0); break;
  }
  ewidth = (std::min)(ewidth, Geometry()->WorkgroupSize());
  switch (type) {
  case COND_BINARY:
    return (wi/ewidth) % 2;
  case COND_SWITCH:
    return (wi/ewidth) % SwitchBranchCount();
  default:
    assert(0); return 123;
  }
}

TestEmitter::TestEmitter()
  : be(new BrigEmitter()),
    initialContext(new Context()),
    scenario(new Scenario())
{
}

void TestEmitter::SetCoreConfig(CoreConfig* cc)
{
  this->coreConfig = cc;
  be->SetCoreConfig(cc);
}

Variable TestEmitter::NewVariable(const std::string& id, Brig::BrigSegment segment, Brig::BrigTypeX type, Location location, Brig::BrigAlignment align, uint64_t dim, bool isConst, bool output)
{
  return new(Ap()) EVariable(this, id, segment, type, location, align, dim, isConst, output);
}

Variable TestEmitter::NewVariable(const std::string& id, VariableSpec varSpec)
{
  return new(Ap()) EVariable(this, id, varSpec);
}

Variable TestEmitter::NewVariable(const std::string& id, VariableSpec varSpec, bool output)
{
  return new(Ap()) EVariable(this, id, varSpec, output);
}

Buffer TestEmitter::NewBuffer(const std::string& id, BufferType type, ValueType vtype, size_t count)
{
  return new(Ap()) EBuffer(this, id, type, vtype, count);
}

UserModeQueue TestEmitter::NewQueue(const std::string& id, UserModeQueueType type)
{
  return new(Ap()) EUserModeQueue(this, id, type);
}

Signal TestEmitter::NewSignal(const std::string& id, uint64_t initialValue)
{
  return new(Ap()) ESignal(this, id, initialValue);
}

Kernel TestEmitter::NewKernel(const std::string& id)
{
  return new(Ap()) EKernel(this, id);
}

Function TestEmitter::NewFunction(const std::string& id)
{
  return new(Ap()) EFunction(this, id);
}


}

Test* EmittedTestBase::Create()
{
  te->SetCoreConfig(hexl::emitter::CoreConfig::Get(context.get()));
  Test();
  return new ScenarioTest(TestName(), te->ReleaseContext());
}

EmittedTest::EmittedTest(emitter::Location codeLocation_, Grid geometry_)
 : cc(0),
   codeLocation(codeLocation_), geometry(geometry_),
   output(0), kernel(0), function(0), functionResult(0)
{
}

std::string EmittedTest::CodeLocationString() const
{
  return LocationString(codeLocation);
}

void EmittedTest::Test()
{
  Init();
  Programs();
  Scenario();
  Finish();
}

void EmittedTest::Init()
{
  cc = hexl::emitter::CoreConfig::Get(context.get());
  GeometryInit();
  te->InitialContext()->Put("geometry", const_cast<GridGeometry*>(geometry));
  kernel = te->NewKernel("test_kernel");
  output = kernel->NewBuffer("output", emitter::HOST_RESULT_BUFFER, ResultValueType(), OutputBufferSize());
  if (codeLocation == emitter::FUNCTION) {
    function = te->NewFunction("test_function");
    functionResult = function->NewVariable("result", BRIG_SEGMENT_ARG, ResultType(), emitter::FUNCTION, BRIG_ALIGNMENT_NONE, ResultDim(), false, true);
  }
}

void EmittedTest::GeometryInit()
{
  if (!geometry) { geometry = cc->Grids().DefaultGeometry(); }
}

void EmittedTest::Programs()
{
  Program();
}

void EmittedTest::Program()
{
  StartProgram();
  Modules();
  EndProgram();
  DispatchSetup* dsetup = new DispatchSetup();
  SetupDispatch(dsetup);
  te->InitialContext()->Put("dispatchSetup", dsetup);
}

void EmittedTest::StartProgram()
{
}

void EmittedTest::EndProgram()
{
}

void EmittedTest::Modules()
{
  Module();
}

void EmittedTest::Module()
{
  StartModule();
  ModuleDirectives();
  ModuleVariables();
  Executables();
  EndModule();
}

void EmittedTest::StartModule()
{
  te->Brig()->Start();
}

void EmittedTest::EndModule()
{
  te->Brig()->End();
  te->InitialContext()->Put("brig", te->Brig()->Brig());
}

void EmittedTest::ModuleDirectives()
{
}

void EmittedTest::ModuleVariables()
{
}

void EmittedTest::Executables()
{
  if (codeLocation == emitter::FUNCTION) { Function(); }
  Kernel();
}

void EmittedTest::Kernel()
{
  StartKernel();
  KernelArguments();
  StartKernelBody();
  KernelDirectives();
  KernelVariables();
  KernelInit();
  KernelCode();
  EndKernel();
}

void EmittedTest::StartKernel()
{
  kernel->StartKernel();
}

void EmittedTest::KernelArguments()
{
  kernel->KernelArguments();
}

void EmittedTest::StartKernelBody()
{
  kernel->StartKernelBody();
}

void EmittedTest::KernelDirectives()
{
  kernel->KernelDirectives();
}

void EmittedTest::KernelVariables()
{
  kernel->KernelVariables();
}

void EmittedTest::KernelInit()
{
  kernel->KernelInit();
}

emitter::TypedReg EmittedTest::KernelResult()
{
  switch (codeLocation) {
  case emitter::KERNEL:
      return Result();

  case emitter::FUNCTION:
    {
      emitter::TypedRegList kernArgInRegs = te->Brig()->AddTRegList(), kernArgOutRegs = te->Brig()->AddTRegList();
      functionResultReg = te->Brig()->AddTReg(ResultType(), ResultCount());
      ActualCallArguments(kernArgInRegs, kernArgOutRegs);
      te->Brig()->EmitCallSeq(function->Directive(), kernArgInRegs, kernArgOutRegs);
      return functionResultReg;
    }
  default:
    assert(0);
    return 0;
  }
}

void EmittedTest::KernelCode()
{
  emitter::TypedReg kernelResult = KernelResult();
  assert(kernelResult);
  output->EmitStoreData(kernelResult);
}

void EmittedTest::ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs)
{
  outputs->Add(functionResultReg);
}

void EmittedTest::EndKernel()
{
  kernel->EndKernel();
}

void EmittedTest::SetupDispatch(DispatchSetup* dispatch)
{
  dispatch->SetDimensions(geometry->Dimensions());
  dispatch->SetWorkgroupSize(geometry->WorkgroupSize(0), geometry->WorkgroupSize(1), geometry->WorkgroupSize(2));
  dispatch->SetGridSize(geometry->GridSize(0), geometry->GridSize(1), geometry->GridSize(2));
  output->SetData(ExpectedResults());
  kernel->SetupDispatch(dispatch);
}

void EmittedTest::Function()
{
  StartFunction();
  FunctionFormalOutputArguments();
  FunctionFormalInputArguments();
  StartFunctionBody();
  FunctionDirectives();
  FunctionVariables();
  FunctionInit();
  FunctionCode();
  EndFunction();
}

void EmittedTest::StartFunction()
{
  function->StartFunction();
}

void EmittedTest::FunctionFormalOutputArguments()
{
  function->FunctionFormalOutputArguments();
}

void EmittedTest::FunctionFormalInputArguments()
{
  function->FunctionFormalInputArguments();
}

void EmittedTest::StartFunctionBody()
{
  function->StartFunctionBody();
}

void EmittedTest::FunctionDirectives()
{
  function->FunctionDirectives();
}

void EmittedTest::FunctionVariables()
{
  function->FunctionVariables();
}

void EmittedTest::FunctionInit()
{
  function->FunctionInit();
}

void EmittedTest::FunctionCode()
{
  switch (codeLocation) {
  case emitter::FUNCTION:
    {
      emitter::TypedReg result = Result();
      te->Brig()->EmitStore(BRIG_SEGMENT_ARG, result, te->Brig()->Address(functionResult->Variable()));
      break;
    }
  default:
    assert(false);
  }
}

void EmittedTest::EndFunction()
{
  function->EndFunction();
}

size_t EmittedTest::OutputBufferSize() const
{
  return geometry->GridSize() * ResultCount();
}

Values* EmittedTest::ExpectedResults() const
{
  Values* result = new Values();
  ExpectedResults(result);
  return result;
}

void EmittedTest::ExpectedResults(Values* result) const
{
  for (size_t i = 0; i < geometry->GridSize(); ++i) {
    for (uint64_t j = 0; j < ResultCount(); ++j) {
      result->push_back(ExpectedResult(i, j));
    }
  }
}


void EmittedTest::Scenario() {
  ScenarioInit();
  ScenarioCodes();
  ScenarioDispatches();
  ScenarioValidation();
  ScenarioEnd();
}

void EmittedTest::ScenarioInit()
{
  kernel->ScenarioInit();
}

void EmittedTest::ScenarioCodes()
{
  CommandSequence& commands0 = te->TestScenario()->Commands();
  commands0.CreateProgram();
  commands0.AddBrigModule();
  commands0.ValidateProgram();
  commands0.Finalize(Defaults::CODE_ID, Defaults::PROGRAM_ID);
}

void EmittedTest::ScenarioDispatches()
{
  te->TestScenario()->Commands().Dispatch();
}

void EmittedTest::ScenarioValidation()
{
}

void EmittedTest::ScenarioEnd()
{
}

void EmittedTest::Finish() {
  te->InitialContext()->Put(Defaults::SCENARIO_ID, te->ReleaseScenario());
}


}
