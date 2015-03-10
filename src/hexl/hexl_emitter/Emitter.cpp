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

  hexl::Sequence<bool>* Value(bool val) {
    static OneValueSequence<bool> trueSequence(true);
    static OneValueSequence<bool> falseSequence(false);
    return val ? &trueSequence : &falseSequence;
  }
}

std::string Dir2Str(BrigControlDirective d)
{
  switch(d) {
  case BRIG_CONTROL_ENABLEBREAKEXCEPTIONS:
    return "EBEX";
  case BRIG_CONTROL_ENABLEDETECTEXCEPTIONS:
    return "EDEX";
  case BRIG_CONTROL_MAXDYNAMICGROUPSIZE:
    return "MDGS";
  case BRIG_CONTROL_MAXFLATGRIDSIZE:
    return "MFGS";
  case BRIG_CONTROL_MAXFLATWORKGROUPSIZE:
    return "MFWS";
  case BRIG_CONTROL_REQUIREDDIM:
    return "RD";
  case BRIG_CONTROL_REQUIREDGRIDSIZE:
    return "RGS";
  case BRIG_CONTROL_REQUIREDWORKGROUPSIZE:
    return "RWS";
  case BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS:
    return "RNPW";
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

FBarrier EmittableContainer::NewFBarrier(const std::string& id, Location location, bool output) 
{
  FBarrier fb = te->NewFBarrier(id, location, output);
  Add(fb);
  return fb;
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

Image EmittableContainer::NewImage(const std::string& id, ImageSpec spec)
{
  Image image = te->NewImage(id, spec);
  Add(image);
  return image;
}

Sampler EmittableContainer::NewSampler(const std::string& id, SamplerSpec spec) 
{
  Sampler sampler = te->NewSampler(id, spec);
  Add(sampler);
  return sampler;
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
    case BRIG_SEGMENT_MAX:
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

void EVariable::PushBack(hexl::Value val) {
  assert(hexl::Brig2ValueType(type) == val.Type());    
  data.push_back(val);
}

void EVariable::WriteData(hexl::Value val, size_t pos) {
  assert(pos < data.size());
  assert(hexl::Brig2ValueType(type) == val.Type());    
  data[pos] = val;
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
    if (data.size() != 0) {
      ArbitraryData arbData;
      for (const auto& val: data) {
        switch (type) {
        case BRIG_TYPE_S8:    arbData.push_back(val.S8());  break;
        case BRIG_TYPE_U8:    arbData.push_back(val.U8());  break;
        case BRIG_TYPE_S16:   arbData.push_back(val.S16()); break;
        case BRIG_TYPE_U16:   arbData.push_back(val.U16()); break;
        case BRIG_TYPE_S32:   arbData.push_back(val.S32()); break;
        case BRIG_TYPE_U32:   arbData.push_back(val.U32()); break;
        case BRIG_TYPE_S64:   arbData.push_back(val.S64()); break;
        case BRIG_TYPE_U64:   arbData.push_back(val.U64()); break;
        case BRIG_TYPE_F16:   arbData.push_back(val.F());   break;
        case BRIG_TYPE_F32:   arbData.push_back(val.F());   break;
        case BRIG_TYPE_F64:   arbData.push_back(val.D());   break;
        case BRIG_TYPE_U8X4:  arbData.push_back(val.U32()); break;
        case BRIG_TYPE_U8X8:  arbData.push_back(val.U64()); break;
        case BRIG_TYPE_S8X4:  arbData.push_back(val.U32()); break;
        case BRIG_TYPE_S8X8:  arbData.push_back(val.U64()); break;
        case BRIG_TYPE_U16X2: arbData.push_back(val.U32()); break;
        case BRIG_TYPE_U16X4: arbData.push_back(val.U64()); break;
        case BRIG_TYPE_S16X2: arbData.push_back(val.U32()); break;
        case BRIG_TYPE_S16X4: arbData.push_back(val.U64()); break;
        case BRIG_TYPE_U32X2: arbData.push_back(val.U64()); break;
        case BRIG_TYPE_S32X2: arbData.push_back(val.U64()); break;
        case BRIG_TYPE_F32X2: arbData.push_back(val.U64()); break;
    
        case BRIG_TYPE_U8X16: case BRIG_TYPE_U16X8: case BRIG_TYPE_U32X4: case BRIG_TYPE_U64X2: 
        case BRIG_TYPE_S8X16: case BRIG_TYPE_S16X8: case BRIG_TYPE_S32X4: case BRIG_TYPE_S64X2: 
        case BRIG_TYPE_F32X4: case BRIG_TYPE_F64X2: 
          arbData.push_back(val.U64());  break;

        case BRIG_TYPE_SIG32: arbData.push_back(val.U32());  break;
        case BRIG_TYPE_SIG64: arbData.push_back(val.U64());  break;

        default: assert(false);
        }
      }
      te->Brig()->EmitVariableInitializer(var, arbData.toSRef());
    }
  }
}

void EVariable::EmitLoadTo(TypedReg dst, bool useVectorInstructions)
{
  te->Brig()->EmitLoad(segment, dst, te->Brig()->Address(Variable()), useVectorInstructions);
}

void EVariable::EmitStoreFrom(TypedReg src, bool useVectorInstructions)
{
  te->Brig()->EmitStore(segment, src, te->Brig()->Address(Variable()), useVectorInstructions);
}

void EVariable::SetupDispatch(DispatchSetup* setup) {
  if (segment == BRIG_SEGMENT_KERNARG) {
    assert(var);
    uint32_t sizes[] = { Count(), 1, 1 };
    auto marg = new MBuffer(setup->MSetup().Count(), id + ".var", MEM_KERNARG, Brig2ValueType(type), 1, sizes);
    marg->Data() = data;
    setup->MSetup().Add(marg);
  }
}


EFBarrier::EFBarrier(TestEmitter* te_, const std::string& id_, Location location_, bool output_)
    : Emittable(te_), id(id_), location(location_), output(output_) 
{
  switch(location) {
  case MODULE:
  case KERNEL:
  case FUNCTION:
  case ARGSCOPE:
    return;
  default:
    assert(false);
  }
}

std::string EFBarrier::FBarrierName() const {
  if (location == Location::MODULE) {
    return "&" + id;
  } else {
    return "%" + id;
  }
}

void EFBarrier::Name(std::ostream& out) const {
  out << id << "_" << LocationString(location);
}

void EFBarrier::EmitDefinition() {
  assert(!fb);
  fb = te->Brig()->EmitFbarrierDefinition(id);
}

void EFBarrier::ModuleVariables() {
  if (location == Location::MODULE) {
    EmitDefinition();
  }
}

void EFBarrier::KernelVariables() {
  if (location == Location::KERNEL) {
    EmitDefinition();
  }
}

void EFBarrier::FunctionVariables() {
  if (location == Location::FUNCTION) {
    EmitDefinition();
  }
}

void EFBarrier::FunctionFormalOutputArguments() {
  if (location == Location::ARGSCOPE && output) {
    EmitDefinition();
  }
}

void EFBarrier::FunctionFormalInputArguments() {
  if (location == Location::ARGSCOPE && !output) {
    EmitDefinition();
  }
}

void EFBarrier::EmitInitfbar() {
  assert(fb);
  te->Brig()->EmitInitfbar(fb);
}

void EFBarrier::EmitInitfbarInFirstWI() {
  assert(fb);
  te->Brig()->EmitInitfbarInFirstWI(fb);
}

void EFBarrier::EmitJoinfbar() {
  assert(fb);
  te->Brig()->EmitJoinfbar(fb);
}

void EFBarrier::EmitWaitfbar() {
  assert(fb);
  te->Brig()->EmitWaitfbar(fb);
}

void EFBarrier::EmitArrivefbar() {
  assert(fb);
  te->Brig()->EmitArrivefbar(fb);
}

void EFBarrier::EmitLeavefbar() {
  assert(fb);
  te->Brig()->EmitLeavefbar(fb);
}

void EFBarrier::EmitReleasefbar() {
  assert(fb);
  te->Brig()->EmitReleasefbar(fb);
}

void EFBarrier::EmitReleasefbarInFirstWI() {
  assert(fb);
  te->Brig()->EmitReleasefbarInFirstWI(fb);
}

void EFBarrier::EmitLdf(TypedReg dest) {
  assert(fb);
  te->Brig()->EmitLdf(dest, fb);
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
  ///case DISPATCH_QUEUE:
  ///  assert(!address);
  ///  address = te->Brig()->AddAReg();
  ///  te->Brig()->EmitQueuePtr(address);
  ///  break;
  default:
    break;
  }
}

PointerReg EUserModeQueue::Address(Brig::BrigSegment segment)
{
  switch (segment) {
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_FLAT:
    assert(address);
    return address;
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

/*
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
*/

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
  PointerReg addr = Address(segment);
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_LDQUEUEREADINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(addr));
}

void EUserModeQueue::EmitLdQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest)
{
  PointerReg addr = Address(segment);
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_LDQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(addr));
}

void EUserModeQueue::EmitStQueueReadIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg src)
{
  PointerReg addr = Address(segment);
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_STQUEUEREADINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(te->Brig()->Address(addr), src->Reg());
}

void EUserModeQueue::EmitStQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg src)
{
  PointerReg addr = Address(segment);
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_STQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(te->Brig()->Address(addr), src->Reg());
}

void EUserModeQueue::EmitAddQueueWriteIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg dest, Operand src)
{
  PointerReg addr = Address(segment);
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_ADDQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(addr), src);
}

void EUserModeQueue::EmitCasQueueWriteIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg dest, Operand src0, Operand src1)
{
  PointerReg addr = Address(segment);
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_CASQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(addr), src0, src1);
}

EImageSpec::EImageSpec(
  BrigSegment brigseg_, 
  BrigTypeX imageType_, 
  Location location_, 
  uint64_t dim_, 
  bool isConst_, 
  bool output_,
  BrigImageGeometry geometry_,
  BrigImageChannelOrder channel_order_, 
  BrigImageChannelType channel_type_,
  size_t width_, 
  size_t height_, 
  size_t depth_, 
  size_t array_size_
  ) : EVariableSpec(brigseg_, imageType_, location_, BRIG_ALIGNMENT_8, dim_, isConst_, output_),
  geometry(geometry_), channel_order(channel_order_), channel_type(channel_type_), width(width_), 
  height(height_), depth(depth_), array_size(array_size_)
{
  // Init rowPitch and slicePitch Params (depend of chanel_order and channel_type)
  //rowPitch = 
  //slicePitch = 
}

bool EImageSpec::IsValidSegment() const 
{
  switch (segment) {
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_KERNARG:
  case BRIG_SEGMENT_ARG:
    return true;
  default:
    return false;
  }
}

bool EImageSpec::IsValidType() const {
  if (type == BRIG_TYPE_ROIMG || type == BRIG_TYPE_WOIMG || type == BRIG_TYPE_RWIMG) {
    return true;
  } else {
    return false;
  }
}

bool EImageSpec::IsValid() const
{
  return EVariableSpec::IsValid() && IsValidSegment() && IsValidType();
}

Location EImage::RealLocation() const 
{
  if (location == AUTO) {
    switch (segment) {
    case BRIG_SEGMENT_GLOBAL:
    case BRIG_SEGMENT_READONLY:
      return MODULE;
    case BRIG_SEGMENT_KERNARG:
      return KERNEL;
    case BRIG_SEGMENT_ARG:
      return FUNCTION;
    default:
      return AUTO;
    }
  } else {
    return location;
  }
}

void EImage::SetupDispatch(DispatchSetup* dispatch) 
{
  if (segment == BRIG_SEGMENT_KERNARG) {  
    unsigned i = dispatch->MSetup().Count();
    image = new MImage(i++, id, segment, geometry, channel_order, channel_type, type, 
      width, height, depth, array_size);
    dispatch->MSetup().Add(image);
    dispatch->MSetup().Add(NewMValue(i, id + ".kernarg", MEM_KERNARG, MV_IMAGEREF, U64(image->Id())));

    if (data) { image->ContentData() = *data; }
    Value value = image->ContentData()[0];
    image->VType() = value.Type();
  }
}

void EImage::EmitImageRd(OperandOperandList dest, BrigTypeX destType, TypedReg image, TypedReg sampler, TypedReg coord)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_RDIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coord->Type();
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = destType;
  ItemList OptList;
  if (dest.elementCount() == 1) {
    OptList.push_back(dest.elements(0));
  } else {
    OptList.push_back(dest);
  }
  OptList.push_back(image->Reg());
  OptList.push_back(sampler->Reg());
  OptList.push_back(coord->Reg());
  inst.operands() = OptList;
}
void EImage::EmitImageRd(HSAIL_ASM::OperandOperandList dest, BrigTypeX destType, TypedReg image, TypedReg sampler, OperandOperandList coord, BrigTypeX coordType)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_RDIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coordType;
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = destType;
  ItemList OptList;
  if (dest.elementCount() == 1) {
    OptList.push_back(dest.elements(0));
  } else {
    OptList.push_back(dest);
  }
  OptList.push_back(image->Reg());
  OptList.push_back(sampler->Reg());
  if (coord.elementCount() == 1) {
    OptList.push_back(coord.elements(0));
  } else {
    OptList.push_back(coord);
  }
  inst.operands() = OptList;
}

void EImage::EmitImageRd(TypedReg dest, TypedReg image, TypedReg sampler, OperandOperandList coord, BrigTypeX coordType)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_RDIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coordType;
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = dest->Type();
  ItemList OptList;
  OptList.push_back(dest->Reg());
  OptList.push_back(image->Reg());
  OptList.push_back(sampler->Reg());
  if (coord.elementCount() == 1) {
    OptList.push_back(coord.elements(0));
  } else {
    OptList.push_back(coord);
  }
  inst.operands() = OptList;
}

void  EImage::EmitImageQuery(TypedReg dest, TypedReg image, BrigImageQuery query)
{
  InstQueryImage inst = te->Brig()->Brigantine().addInst<InstQueryImage>(BRIG_OPCODE_QUERYIMAGE);
  inst.imageType() = image->Type();
  inst.geometry() = geometry;
  inst.imageQuery() = query;
  inst.type() = dest->Type();
  ItemList OptList;
  OptList.push_back(dest->Reg());
  OptList.push_back(image->Reg());
  inst.operands() = OptList;
}

void EImage::EmitImageLd(OperandOperandList dest, BrigTypeX destType, TypedReg image, TypedReg coord)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_LDIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coord->Type();
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = destType;
  ItemList OptList;
  if (dest.elementCount() == 1) {
    OptList.push_back(dest.elements(0));
  } else {
    OptList.push_back(dest);
  }
  OptList.push_back(image->Reg());
  OptList.push_back(coord->Reg());
  inst.operands() = OptList;
}

void EImage::EmitImageLd(TypedReg dest, TypedReg image, OperandOperandList coord, BrigTypeX coordType)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_LDIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coordType;
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = dest->Type();
  ItemList OptList;
  OptList.push_back(dest->Reg());
  OptList.push_back(image->Reg());
  if (coord.elementCount() == 1) {
    OptList.push_back(coord.elements(0));
  } else {
    OptList.push_back(coord);
  }
  inst.operands() = OptList;
}

void EImage::EmitImageLd(HSAIL_ASM::OperandOperandList dest, BrigTypeX destType, TypedReg image, OperandOperandList coord, BrigTypeX coordType)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_LDIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coordType;
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = destType;
  ItemList OptList;
  if (dest.elementCount() == 1) {
    OptList.push_back(dest.elements(0));
  } else {
    OptList.push_back(dest);
  }
  OptList.push_back(image->Reg());
  if (coord.elementCount() == 1) {
    OptList.push_back(coord.elements(0));
  } else {
    OptList.push_back(coord);
  }
  inst.operands() = OptList;
}

void EImage::EmitImageSt(OperandOperandList src, BrigTypeX srcType, TypedReg image, TypedReg coord)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_STIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coord->Type();
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = srcType;
  ItemList OptList;
  if (src.elementCount() == 1) {
    OptList.push_back(src.elements(0));
  } else {
    OptList.push_back(src);
  }
  OptList.push_back(image->Reg());
  OptList.push_back(coord->Reg());
  inst.operands() = OptList;
}

void EImage::EmitImageSt(HSAIL_ASM::OperandOperandList src, Brig::BrigTypeX srcType, TypedReg image, HSAIL_ASM::OperandOperandList coord, Brig::BrigTypeX coordType)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_STIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coordType;
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = srcType;
  ItemList OptList;
  OptList.push_back(src);
  OptList.push_back(image->Reg());
  OptList.push_back(coord);
  inst.operands() = OptList;
}

void EImage::EmitImageSt(TypedReg src, TypedReg image, HSAIL_ASM::OperandOperandList coord, Brig::BrigTypeX coordType)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_STIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coordType;
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = src->Type();
  ItemList OptList;
  OptList.push_back(src->Reg());
  OptList.push_back(image->Reg());
  OptList.push_back(coord);
  inst.operands() = OptList;
}

void EImage::KernelVariables()
{
  if (RealLocation() == KERNEL && segment != BRIG_SEGMENT_KERNARG) {
    EmitDefinition();
  }
}

void EImage::FunctionFormalOutputArguments()
{
  if (RealLocation() == FUNCTION && segment == BRIG_SEGMENT_ARG && output) {
    EmitDefinition();
  }
}

void EImage::FunctionFormalInputArguments()
{
  if (RealLocation() == FUNCTION && segment == BRIG_SEGMENT_ARG && !output) {
    EmitDefinition();
  }
}

void EImage::KernelArguments()
{
  if (segment == BRIG_SEGMENT_KERNARG && RealLocation() == Location::KERNEL) {
    EmitDefinition();
  }
}

void EImage::ModuleVariables()
{
  if (RealLocation() == Location::MODULE) {
    EmitDefinition();
  }
}

void EImage::FunctionVariables()
{
  if (RealLocation() == FUNCTION && segment != BRIG_SEGMENT_ARG) {
    EmitDefinition();
  }
}

void EImage::EmitDefinition() 
{
  assert(!var);
  var = EmitAddressDefinition(segment);
  EmitInitializer();
}

void EImage::EmitInitializer()
{
  assert(var);
  if (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_READONLY) {
    var.allocation() = BRIG_ALLOCATION_AGENT;
    ItemList list;
    for (uint64_t i = 0; i < std::max<uint64_t>(dim, (uint64_t)1); ++i) {
      auto init = te->Brig()->Brigantine().append<OperandConstantImage>();
      init.type() = type;
      init.width() = width;
      init.height() = height;
      init.depth() = depth;
      init.array() = array_size;
      init.geometry() = geometry;
      init.channelOrder() = channel_order;
      init.channelType() = channel_type;
    }
    if (dim == 0) {
      var.init() = list[0];
    } else {
      var.init() = te->Brig()->Brigantine().createOperandList(list);
    }
  }
}

HSAIL_ASM::DirectiveVariable EImage::EmitAddressDefinition(BrigSegment segment)
{
  return te->Brig()->EmitVariableDefinition(id, segment, type);
}

bool ESamplerSpec::IsValidSegment() const 
{
  switch (segment) {
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_KERNARG:
  case BRIG_SEGMENT_ARG:
    return true;
  default:
    return false;
  }
}

bool ESamplerSpec::IsValid() const
{
  return EVariableSpec::IsValid() && IsValidSegment();
}

void ESampler::SetupDispatch(DispatchSetup* dispatch) 
{
  if (segment == BRIG_SEGMENT_KERNARG) {
    unsigned i = dispatch->MSetup().Count();
    sampler = new MSampler(i++, id, segment, coord, filter, addressing);
    dispatch->MSetup().Add(sampler);
    dispatch->MSetup().Add(NewMValue(i, id + ".kernarg", MEM_KERNARG, MV_SAMPLERREF, U64(sampler->Id())));
  }
}

void ESampler::KernelVariables()
{
  if (RealLocation() == KERNEL && segment != BRIG_SEGMENT_KERNARG) {
    EmitDefinition();
  }
}

void ESampler::FunctionFormalOutputArguments()
{
  if (RealLocation() == FUNCTION && segment == BRIG_SEGMENT_ARG && output) {
    EmitDefinition();
  }
}

void ESampler::FunctionFormalInputArguments()
{
  if (RealLocation() == FUNCTION && segment == BRIG_SEGMENT_ARG && !output) {
    EmitDefinition();
  }
}

void ESampler::KernelArguments()
{
  if (segment == BRIG_SEGMENT_KERNARG && RealLocation() == Location::KERNEL) {
    EmitDefinition();
  }
}

void ESampler::ModuleVariables()
{
  if (RealLocation() == Location::MODULE) {
    EmitDefinition();
  }
}

void ESampler::FunctionVariables()
{
  if (RealLocation() == FUNCTION && segment != BRIG_SEGMENT_ARG) {
    EmitDefinition();
  }
}

void ESampler::EmitDefinition() 
{
  assert(!var);
  var = EmitAddressDefinition(segment);
  EmitInitializer();
}

void ESampler::EmitSamplerQuery(TypedReg dest, TypedReg sampler, BrigSamplerQuery query)
{
  InstQuerySampler inst = te->Brig()->Brigantine().addInst<InstQuerySampler>(BRIG_OPCODE_QUERYSAMPLER);
  inst.samplerQuery() = query;
  inst.type() = dest->Type();
  ItemList OptList;
  OptList.push_back(dest->Reg());
  OptList.push_back(sampler->Reg());
  inst.operands() = OptList;
}

HSAIL_ASM::DirectiveVariable ESampler::EmitAddressDefinition(BrigSegment segment)
{
  return te->Brig()->EmitVariableDefinition(id, segment, te->Brig()->SamplerType(), align, dim, isConst, output);
}

void ESampler::EmitInitializer()
{
  assert(var);
  if (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_READONLY) {
    var.allocation() = BRIG_ALLOCATION_AGENT;
    ItemList list;
    for (uint64_t i = 0; i < std::max<uint64_t>(dim, (uint64_t)1); ++i) {
      auto init = te->Brig()->Brigantine().append<OperandConstantSampler>();
      init.type() = type;
      init.addressing() = addressing;
      init.coord() = coord;
      init.filter() = filter;
      list.push_back(init);
    }
    if (dim == 0) {
      var.init() = list[0];
    } else {
      var.init() = te->Brig()->Brigantine().createConstantOperandList(list, type);
    }
  }
}

bool ESampler::IsValidSegment() const 
{
  switch (segment) {
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_READONLY:
  case BRIG_SEGMENT_KERNARG:
  case BRIG_SEGMENT_ARG:
    return true;
  default:
    return false;
  }
}

Location ESampler::RealLocation() const 
{
  if (location == AUTO) {
    switch (segment) {
    case BRIG_SEGMENT_GLOBAL:
    case BRIG_SEGMENT_READONLY:
      return MODULE;
    case BRIG_SEGMENT_KERNARG:
      return KERNEL;
    case BRIG_SEGMENT_ARG:
      return FUNCTION;
    default:
      return AUTO;
    }
  } else {
    return location;
  }
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

FBarrier TestEmitter::NewFBarrier(const std::string& id, Location location, bool output) 
{
  return new(Ap()) EFBarrier(this, id, location, output);
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

Image TestEmitter::NewImage(const std::string& id, ImageSpec spec)
{
  return new(Ap()) EImage(this, id, spec);
}

Sampler TestEmitter::NewSampler(const std::string& id, SamplerSpec spec) 
{
  return new(Ap()) ESampler(this, id, spec);
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
  KernelArgumentsInit();
  if (codeLocation == emitter::FUNCTION) {
    function = te->NewFunction("test_function");
    FunctionArgumentsInit();
  }
}

void EmittedTest::KernelArgumentsInit()
{
  output = kernel->NewBuffer("output", emitter::HOST_RESULT_BUFFER, ResultValueType(), OutputBufferSize());
}

void EmittedTest::FunctionArgumentsInit()
{
  functionResult = function->NewVariable("result", BRIG_SEGMENT_ARG, ResultType(), emitter::FUNCTION, BRIG_ALIGNMENT_NONE, ResultDim(), false, true);
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
  if (output) {
  output->EmitStoreData(kernelResult);
}
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
  if (output) {
  output->SetData(ExpectedResults());
  }
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
