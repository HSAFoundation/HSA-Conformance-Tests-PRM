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

#include "Emitter.hpp"
#include "BrigEmitter.hpp"
#include "Scenario.hpp"
#include "Sequence.hpp"
#include "CoreConfig.hpp"
#include "hsa.h"
#include <cmath>

///\todo (Artem) Generalize (move to libHSAIL?)
#ifdef _WIN32
  bool isNanOrDenorm(float f)
  {
    switch(_fpclassf(f))
    {
    case _FPCLASS_SNAN:
    case _FPCLASS_QNAN:
    case _FPCLASS_ND:
    case _FPCLASS_PD:
      return true;
    default:
      return false;
      break;
    }
    return false;
  }

  bool isNanOrInf(double df)
  {
    switch (_fpclass(df))
    {
    case _FPCLASS_SNAN:
    case _FPCLASS_QNAN:
    case _FPCLASS_NINF:
    case _FPCLASS_PINF:
      return true;
    default:
      return false;
      break;
    }
    return false;
  }
#else //assume Linux
  #if __cplusplus >= 201103L
  //std::fpclassify() supported
  bool isNanOrDenorm(float f)
  {
    switch(std::fpclassify(f))
    {
    case FP_NAN:
    case FP_SUBNORMAL:
      return true;
      break;
    default:
      return false;
      break;
    }
    return false;
  }

  bool isNanOrInf(double df)
  {
    switch(std::fpclassify(df))
    {
    case FP_NAN:
    case FP_INFINITE:
      return true;
      break;
    default:
      return false;
      break;
    }
    return false;
  }
  #else
  #error "std::fpclassify() is not guaranteed to be supported. Check your compiler for c++11 support."
  #endif
#endif // _WIN32


using namespace HSAIL_ASM;
using namespace hexl::scenario;
using namespace hexl::runtime;

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

std::string AnnotationLocationString(AnnotationLocation location) {
  switch (location)
  {
  case AnnotationLocation::BEFORE_VERSION: return "before_version";
  case AnnotationLocation::AFTER_VERSION: return "after_version";
  case AnnotationLocation::END_MODULE: return "end_module";
  case AnnotationLocation::BEFORE_MODULE_VARIABLE: return "before_module_variable";
  case AnnotationLocation::AFTER_MODULE_VARIABLE: return "after_module_variable";
  case AnnotationLocation::START_KERNEL: return "start_kernel";
  case AnnotationLocation::END_KERNEL: return "end_kernel";
  case AnnotationLocation::MIDDLE_KERNEL: return "middle_of_kernel";
  case AnnotationLocation::START_FUNCTION: return "start_function";
  case AnnotationLocation::END_FUNCTION: return "end_function";
  case AnnotationLocation::MIDDLE_FUNCTION: return "middle_of_function";
  case AnnotationLocation::START_ARG_BLOCK: return "start_arg_block";
  case AnnotationLocation::END_ARG_BLOCK: return "end_arg_block";
  case AnnotationLocation::MIDDLE_ARG_BLOCK: return "middle_of_arg_block";
  default: assert(false); return "";
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

BrigLinkage Location2Linkage(Location loc) {
  switch (loc) {
  case MODULE: 
    return BRIG_LINKAGE_PROGRAM;
  case KERNEL: 
  case FUNCTION:
    return BRIG_LINKAGE_FUNCTION;
  case ARGSCOPE:
    return BRIG_LINKAGE_ARG;
  default:
    assert(false); 
    return BRIG_LINKAGE_NONE;
  }
}

Grid Emittable::Geometry() { return te->EmitContext()->Get<GridGeometry>("geometry"); }

EmittableContainer::EmittableContainer(TestEmitter* te)
  : Emittable(te),
    list(te->Ap())
{
}

void EmittableContainer::Name(std::ostream& out) const
{
  for (size_t i = 0; i < list.size(); ++i) {
    list[i]->Name(out);
    if (i != list.size() - 1) { out << "_"; }
  }
}

Variable EmittableContainer::NewVariable(const std::string& id, BrigSegment segment, BrigType type, Location location, BrigAlignment align, uint64_t dim, bool isConst, bool output)
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

Variable EmittableContainer::NewFlexArray(const std::string& id, BrigType type, BrigAlignment align)
{
  Variable var = te->NewFlexArray(id, type, align);
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

Signal EmittableContainer::NewSignal(const std::string& id, uint64_t initialValue)
{
  Signal signal = te->NewSignal(id, initialValue);
  Add(signal);
  return signal;
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

Image EmittableContainer::NewImage(const std::string& id, ImageType type, ImageSpec spec, bool optionalFormats)
{
  Image image = te->NewImage(id, type, spec, optionalFormats);
  Add(image);
  return image;
}

Sampler EmittableContainer::NewSampler(const std::string& id, SamplerSpec spec) 
{
  Sampler sampler = te->NewSampler(id, spec);
  Add(sampler);
  return sampler;
}

Module EmittableContainer::NewModule(const std::string& id)
{
  Module module = te->NewModule(id);
  Add(module);
  return module;
}

Dispatch EmittableContainer::NewDispatch(const std::string& id, const std::string& executableId, const std::string& kernelName, Grid geometry)
{
  Dispatch dispatch = te->NewDispatch(id, executableId, kernelName, geometry);
  Add(dispatch);
  return dispatch;
}

OperandOperandList ETypedReg::CreateOperandList(HSAIL_ASM::Brigantine& b)
{
  ItemList items;
  for (size_t i = 0; i < count; ++i) { items.push_back(regs[i]); }
  return b.createOperandList(items);
}

ETypedRegList::ETypedRegList(Arena* ap)
  : tregs(ap)
{

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
  out << segment2str(segment) << "_" << type2str(type);
  if (dim > 0) { out << "[" << dim << "]"; }
  out << "_align(" << align2num(align) << ")";
  if (location != AUTO) { out << "@" << LocationString(location); }
}

EVariable::EVariable(TestEmitter* te_, const std::string& id_,
  BrigSegment segment_, BrigType type_, Location location_, BrigAlignment align_, uint64_t dim_, bool isConst_, bool output_)
  : EVariableSpec(segment_, type_, location_, align_, dim_, isConst_, output_), 
  id(id_.c_str(), te_->Ap()), hasInitializer(false) { te = te_;  }
EVariable::EVariable(TestEmitter* te_, const std::string& id_, const EVariableSpec* spec_)
  : EVariableSpec(*spec_), id(id_.c_str(), te_->Ap()), hasInitializer(false) { te = te_; }
EVariable::EVariable(TestEmitter* te_, const std::string& id_, const EVariableSpec* spec_, bool output)
  : EVariableSpec(*spec_, output), id(id_.c_str(), te_->Ap()), hasInitializer(false) { te = te_; }

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

void EVariable::AddData(hexl::Value val)
{
  assert(hexl::Brig2ValueType(type) == val.Type());
  if (!data.get()) { data.reset(new Values()); }
  data->push_back(val);
  if (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_READONLY) {
    hasInitializer = true;
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
    return std::string("&") + id.c_str();
  default:
    return std::string("%") + id.c_str();
  }
}

void EVariable::Name(std::ostream& out) const
{
  if (isConst) { out << "const_"; }
  out << segment2str(segment) << "_" << type2str(type);
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
  var = te->Brig()->EmitVariableDefinition(VariableName(), segment, type, align, dim, isConst, output, true);
  EmitInitializer();
}

void EVariable::EmitDeclaration()
{
  var = te->Brig()->EmitVariableDefinition(VariableName(), segment, type, align, dim, isConst, output, false);
}

void EVariable::EmitInitializer()
{
  assert(var);
  if (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_READONLY) { 
    if (Values* data = this->data.release()) {
      hasInitializer = true;
      ArbitraryData arbData;
      for (const auto& val: *data) {
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
      delete data;
    }
  }
}

void EVariable::EmitMemorySync()
{
  if (segment == BRIG_SEGMENT_GLOBAL && hasInitializer) {
    te->Brig()->EmitMemfence(
      BRIG_MEMORY_ORDER_SC_ACQUIRE,
      BRIG_MEMORY_SCOPE_SYSTEM,
      BRIG_MEMORY_SCOPE_SYSTEM,
      BRIG_MEMORY_SCOPE_NONE);
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

void EVariable::SetupDispatch(const std::string& dispatchId) {
  if (segment == BRIG_SEGMENT_KERNARG) {
    Values* data = this->data.release();
    if (data) {
      CommandsBuilder* commands = te->TestScenario()->Commands();
      commands->DispatchValuesArg(dispatchId, data);
    }
  }
}

EmittableWithId::EmittableWithId(TestEmitter* te, const std::string& id_)
  : Emittable(te), id(id_.c_str(), te->Ap())
{
}

EmittableContainerWithId::EmittableContainerWithId(TestEmitter* te, const std::string& id_)
  : EmittableContainer(te), id(id_.c_str(), te->Ap())
{
}

EFBarrier::EFBarrier(TestEmitter* te_, const std::string& id_, Location location_, bool output_)
    : EmittableWithId(te_, id_), location(location_), output(output_) 
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
    return std::string("&") + id.c_str();
  } else {
    return std::string("%") + id.c_str();
  }
}

void EFBarrier::Name(std::ostream& out) const {
  out << Id() << "_" << LocationString(location);
}

void EFBarrier::EmitDefinition() {
  fb = te->Brig()->EmitFbarrierDefinition(Id(), true);
}

void EFBarrier::EmitDeclaration() {
  fb = te->Brig()->EmitFbarrierDefinition(Id(), false);
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
  EmitAction emitAction(te, te->EmitContext()->Get<GridGeometry>("geometry"));
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
  return te->Brig()->EmitVariableDefinition(Id(), segment, te->Brig()->PointerType());
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

void EBuffer::ScenarioInit()
{
  CommandsBuilder* commands = te->TestScenario()->Commands();
  if (Values* values = data.release()) {
    te->InitialContext()->Move(IdData(), values);
  }
  commands->BufferCreate(Id(), Size(), (type == HOST_INPUT_BUFFER) ? IdData() : "");
}

void EBuffer::SetupDispatch(const std::string& dispatchId)
{
  CommandsBuilder* commands = te->TestScenario()->Commands();
  commands->DispatchArg(dispatchId, DARG_BUFFER, Id());
}

void EBuffer::ScenarioValidation()
{
  CommandsBuilder* commands = te->TestScenario()->Commands();
  if (type == HOST_RESULT_BUFFER) {
    commands->BufferValidate(Id(), IdData(), vtype, comparisonMethod);
  }
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

PointerReg EBuffer::DataAddressReg(TypedReg index, uint64_t offset, bool flat, uint64_t factor)
{
  PointerReg address = Address(flat);
  if (!index && !offset) {
    return address;
  }
  PointerReg fullAddress = AddAReg(flat);
  if (index && offset) {
    te->Brig()->EmitArith(BRIG_OPCODE_MAD, fullAddress, index, te->Brig()->Immed(address->Type(), TypeSize() * factor), address);
    te->Brig()->EmitArith(BRIG_OPCODE_ADD, fullAddress, fullAddress, te->Brig()->Immed(address->Type(), offset));
  } else if (index) {
    te->Brig()->EmitArith(BRIG_OPCODE_MAD, fullAddress, index, te->Brig()->Immed(address->Type(), TypeSize() * factor), address);
  } else if (offset) {
    te->Brig()->EmitArith(BRIG_OPCODE_ADD, fullAddress, address, te->Brig()->Immed(address->Type(), offset));
  } else {
    assert(false);
  }
  return fullAddress;
}

OperandAddress EBuffer::DataAddress(TypedReg index, uint64_t offset, bool flat, uint64_t factor)
{
  return te->Brig()->Address(DataAddressReg(index, offset, flat, factor));
}


void EBuffer::EmitLoadData(TypedReg dest, TypedReg index, uint64_t offset, bool flat, uint64_t factor)
{
  switch (type) {
  case HOST_INPUT_BUFFER:
    te->Brig()->EmitLoad(flat ? BRIG_SEGMENT_FLAT : BRIG_SEGMENT_GLOBAL, dest, DataAddress(index, offset, flat, dest->Count() * factor));
    break;
  default:
    assert(0);
    break;
  }
}

void EBuffer::EmitLoadData(TypedReg dest, bool flat)
{
  TypedReg index = te->Brig()->WorkitemFlatAbsId(Address(flat)->IsLarge());
  EmitLoadData(dest, index, 0, flat);
}

void EBuffer::EmitStoreData(TypedReg src, TypedReg index, uint64_t offset, bool flat, uint64_t factor)
{
  switch (type) {
  case HOST_RESULT_BUFFER:
    te->Brig()->EmitStore(flat ? BRIG_SEGMENT_FLAT : BRIG_SEGMENT_GLOBAL, src, DataAddress(index, offset, flat, src->Count() * factor));
    break;
  default:
    assert(0);
    break;
  }
}

void EBuffer::EmitStoreData(TypedReg src, bool flat)
{
  TypedReg index = te->Brig()->WorkitemFlatAbsId(Address(flat)->IsLarge());
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
    te->TestScenario()->Commands()->QueueCreate(Id());
    break;
  default:
    break;
  }
}

void EUserModeQueue::SetupDispatch(const std::string& dispatchId)
{
  Emittable::SetupDispatch(dispatchId);
  switch (type) {
  case SEPARATE_QUEUE:
    te->TestScenario()->Commands()->DispatchArg(dispatchId, DARG_QUEUE, Id());
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

PointerReg EUserModeQueue::Address(BrigSegment segment)
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

void EUserModeQueue::EmitAddQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest, Operand src)
{
  PointerReg addr = Address(segment);
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_ADDQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(addr), src);
}

void EUserModeQueue::EmitCasQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest, Operand src0, Operand src1)
{
  PointerReg addr = Address(segment);
  InstQueue inst = te->Brig()->Brigantine().addInst<InstQueue>(BRIG_OPCODE_CASQUEUEWRITEINDEX, BRIG_TYPE_U64);
  inst.segment() = segment;
  inst.memoryOrder() = memoryOrder;
  inst.operands() = te->Brig()->Operands(dest->Reg(), te->Brig()->Address(addr), src0, src1);
}

EImageSpec::EImageSpec(
  BrigSegment brigseg_,
  BrigType type_,
  Location location_,
  uint64_t dim_,
  bool isConst_,
  bool output_,
  BrigImageGeometry geometry_,
  BrigImageChannelOrder channelOrder_,
  BrigImageChannelType channelType_,
  size_t width_,
  size_t height_,
  size_t depth_,
  size_t arraySize_
  )
  : EVariableSpec(brigseg_, type_, location_, BRIG_ALIGNMENT_8, dim_, isConst_, output_),
    ImageParams(type_, geometry_, channelOrder_, channelType_, width_, height_, depth_, arraySize_) {
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

EImage::EImage(TestEmitter* te_, const std::string& id_, ImageType imageType_, const EImageSpec* spec, bool optionalFormats_)
  : EImageSpec(*spec), id(id_, te_->Ap()), imageType(imageType_), initialData(nullptr), writeCount(0), optionalFormats(optionalFormats_)
{
  te = te_;
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

Value EImage::GenMemValue(Value v)
{
  Value result;
  switch (ChannelOrder())
  {
  case BRIG_CHANNEL_ORDER_A:
  case BRIG_CHANNEL_ORDER_R:
  case BRIG_CHANNEL_ORDER_RX:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      result = Value(MV_UINT8, v.U8());
      break;
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      result = Value(MV_UINT16, v.U16());
      break;
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_FLOAT:
      result = Value(MV_UINT32, v.U32());
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_RG:
  case BRIG_CHANNEL_ORDER_RGX:
  case BRIG_CHANNEL_ORDER_RA:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      result = Value(MV_UINT16, v.U16());
      break;
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      result = Value(MV_UINT32, v.U32());
      break;
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_FLOAT:
      result = Value(MV_UINT64, v.U64());
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_RGB:
  case BRIG_CHANNEL_ORDER_RGBX:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      result = Value(MV_UINT16, v.U16());
      break;
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
      result = Value(MV_UINT32, v.U32());
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_RGBA:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      result = Value(MV_UINT32,  (v.U32() & 0xFFFF0000) + (v.U32() >> 16));
      break;
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      result = Value(MV_UINT64, v.U64());
      break;
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_FLOAT:
      result = Value(v.U128());
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_BGRA:
  case BRIG_CHANNEL_ORDER_ARGB:
  case BRIG_CHANNEL_ORDER_ABGR:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      //fix write sequence
      result = Value(MV_UINT32,  (v.U32() & 0xFFFF0000) + (v.U32() >> 16));
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_SRGB:
  case BRIG_CHANNEL_ORDER_SRGBX:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      result = Value(MV_UINT32, v.U32()); //TODO 24bpp
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_SRGBA:
  case BRIG_CHANNEL_ORDER_SBGRA:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      result = Value(MV_UINT32, v.U32());
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_INTENSITY:
  case BRIG_CHANNEL_ORDER_LUMINANCE:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      result = Value(MV_UINT8, v.U8());
      break;
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      result = Value(MV_UINT16, v.U16());
      break;
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
    case BRIG_CHANNEL_TYPE_FLOAT:
      result = Value(MV_UINT32, v.U32());
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_DEPTH:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT16:
    case BRIG_CHANNEL_TYPE_UNORM_INT16:
    case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      result = Value(MV_UINT16, v.U16());
      break;
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_FLOAT:
      result = Value(MV_UINT32, v.U32());
      break;
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      result = Value(MV_UINT32, v.U32()); //TODO 24bpp
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_DEPTH_STENCIL:
    switch (ChannelType())
    {
    case BRIG_CHANNEL_TYPE_SNORM_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT8:
    case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
    case BRIG_CHANNEL_TYPE_UNORM_INT24:
      result = Value(MV_UINT8, v.U32());
      break;
    case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    case BRIG_CHANNEL_TYPE_FLOAT:
      result = Value(MV_UINT64, v.U64());
      break;
    default:
      assert(0); //illegal channel
      break;
    }
    break;
  default:
    assert(0); //illegal channel order
    break;
  }
  //store value
  return result;
}

void EImage::SetInitialData(Value initial) {
  if (initialData.get() == nullptr) {
    initialData = std::unique_ptr<Value>(new Value());
  }
  *initialData = initial;
}

void EImage::ScenarioInit()
{  
  te->InitialContext()->Move(IdParams(), new ImageParams(*this));
  switch (imageType) {
  case HOST_INPUT_IMAGE:
  case HOST_IMAGE:
    te->TestScenario()->Commands()->ImageCreate(Id(), IdParams(), optionalFormats);
    if (initialData.get()) {
      te->InitialContext()->Put(IdData(), *initialData);
      //te->InitialContext()->Move(IdData(), initialData.release());
      te->TestScenario()->Commands()->ImageInitialize(Id(), IdParams(), IdData());
    }
    break;
  case HOST_OUTPUT_IMAGE:
    te->TestScenario()->Commands()->ImageCreate(Id(), IdParams(), optionalFormats);
    break;
  default:
    break;
  }
}

void EImage::ScenarioImageWrite(std::unique_ptr<Values> writeData, const ImageRegion& region) {
  assert(te->InitialContext()->Has(IdParams()));
  te->InitialContext()->Move(IdWrite(), writeData.release());
  te->TestScenario()->Commands()->ImageWrite(Id(), IdWrite(), region);
  ++writeCount;
}

void EImage::SetupDispatch(const std::string& dispatchId)
{
  switch (imageType) {
  case HOST_INPUT_IMAGE:
  case HOST_OUTPUT_IMAGE:
    te->TestScenario()->Commands()->DispatchArg(dispatchId, DARG_IMAGE, Id());
    break;
  default:
    break;
  }
}

void EImage::EmitImageRd(TypedReg dest, TypedReg image, TypedReg sampler, TypedReg coord)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_RDIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coord->Type();
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = dest->Type();
  ItemList OptList;
  if (dest->Count() == 1) {
    OptList.push_back(dest->Reg(0));
  } else {
    OptList.push_back(dest->CreateOperandList(te->Brig()->Brigantine()));
  }
  OptList.push_back(image->Reg());
  OptList.push_back(sampler->Reg());
  if (coord->Count() == 1) {
    OptList.push_back(coord->Reg(0));
  } else {
    OptList.push_back(coord->CreateOperandList(te->Brig()->Brigantine()));
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

void EImage::EmitImageLd(TypedReg dest, TypedReg image, TypedReg coord)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_LDIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coord->Type();
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = dest->Type();
  ItemList OptList;
  if (dest->Count() == 1) {
    OptList.push_back(dest->Reg(0));
  } else {
    OptList.push_back(dest->CreateOperandList(te->Brig()->Brigantine()));
  }
  OptList.push_back(image->Reg());
  if (coord->Count() == 1) {
    OptList.push_back(coord->Reg(0));
  } else {
    OptList.push_back(coord->CreateOperandList(te->Brig()->Brigantine()));
  }
  inst.operands() = OptList;
}

void EImage::EmitImageSt(TypedReg src, TypedReg image, TypedReg coord)
{
  InstImage inst = te->Brig()->Brigantine().addInst<InstImage>(BRIG_OPCODE_STIMAGE);
  inst.imageType() = image->Type();
  inst.coordType() = coord->Type();
  inst.geometry() = geometry;
  inst.equivClass() = 0;//12;
  inst.type() = src->Type();
  ItemList OptList;
  if (src->Count() == 1) {
    OptList.push_back(src->Reg(0));
  } else {
    OptList.push_back(src->CreateOperandList(te->Brig()->Brigantine()));
  }
  OptList.push_back(image->Reg()); 
  if (coord->Count() == 1) {
    OptList.push_back(coord->Reg(0));
  } else {
    OptList.push_back(coord->CreateOperandList(te->Brig()->Brigantine()));
  }
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
  if (imageType == KERNEL_IMAGE && RealLocation() == FUNCTION && segment == BRIG_SEGMENT_ARG && output) {
    EmitDefinition();
  }
}

void EImage::FunctionFormalInputArguments()
{
  if (imageType == HOST_INPUT_IMAGE && RealLocation() == FUNCTION && segment == BRIG_SEGMENT_ARG && !output) {
    EmitDefinition();
  }
}

void EImage::KernelArguments()
{
  if ((imageType == HOST_INPUT_IMAGE || imageType == HOST_OUTPUT_IMAGE) && 
      segment == BRIG_SEGMENT_KERNARG && RealLocation() == Location::KERNEL) {
    EmitDefinition();
  }
}

void EImage::ModuleVariables()
{
  if (imageType == KERNEL_IMAGE && RealLocation() == Location::MODULE) {
    EmitDefinition();
  }
}

void EImage::FunctionVariables()
{
  if (imageType == KERNEL_IMAGE && RealLocation() == FUNCTION && segment != BRIG_SEGMENT_ARG) {
    EmitDefinition();
  }
}

void EImage::EmitDefinition() 
{
  assert(!var);
  switch (imageType) {
  case HOST_INPUT_IMAGE:
  case HOST_OUTPUT_IMAGE:
  case KERNEL_IMAGE:
    var = EmitAddressDefinition(segment);
    break;
  default:
    break;
  }
  switch (imageType) {
  case KERNEL_IMAGE:
    EmitInitializer();
    break;
  default:
    break;
  }
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
      init.array() = arraySize;
      init.geometry() = geometry;
      init.channelOrder() = channelOrder;
      init.channelType() = channelType;
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
  return te->Brig()->EmitVariableDefinition(Id(), segment, type);
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

ESampler::ESampler(TestEmitter* te_, const std::string& id_, const ESamplerSpec* spec_)
  : ESamplerSpec(*spec_), id(id_, te_->Ap())
{
  te = te_;
}

void ESampler::ScenarioInit()
{
  te->InitialContext()->Move(IdParams(), new SamplerParams(*this));
  te->TestScenario()->Commands()->SamplerCreate(Id(), IdParams());
}

void ESampler::SetupDispatch(const std::string& dispatchId) 
{
  if (segment == BRIG_SEGMENT_KERNARG) {
    te->TestScenario()->Commands()->DispatchArg(dispatchId, DARG_SAMPLER, Id());
    /*
    unsigned i = dispatch->MSetup().Count();
    sampler = new MSampler(i++, id, segment, coord, filter, addressing);
    dispatch->MSetup().Add(sampler);
    dispatch->MSetup().Add(NewMValue(i, id + ".kernarg", MEM_KERNARG, MV_SAMPLERREF, U64(sampler->Id())));
    */
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
  return te->Brig()->EmitVariableDefinition(Id(), segment, te->Brig()->SamplerType(), align, dim, isConst, output);
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
      init.addressing() = Addressing();
      init.coord() = Coord();
      init.filter() = Filter();
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

void EImageCalc::SetupDefaultColors()
{
  switch (imageChannelType)
  {
  case BRIG_CHANNEL_TYPE_SNORM_INT8:
  case BRIG_CHANNEL_TYPE_SNORM_INT16:
  case BRIG_CHANNEL_TYPE_UNORM_INT8:
  case BRIG_CHANNEL_TYPE_UNORM_INT16:
  case BRIG_CHANNEL_TYPE_UNORM_INT24:
  case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
  case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
  case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
  case BRIG_CHANNEL_TYPE_HALF_FLOAT:
  case BRIG_CHANNEL_TYPE_FLOAT:
    color_zero = Value(0.0f);
    color_one  = Value(1.0f);
    break;

  case BRIG_CHANNEL_TYPE_SIGNED_INT8:
  case BRIG_CHANNEL_TYPE_SIGNED_INT16:
  case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    color_zero = Value(MV_INT32, 0);
    color_one  = Value(MV_INT32, 1);
    break;

  case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    color_zero = Value(MV_UINT32, 0);
    color_one  = Value(MV_UINT32, 1);
    break;
  default:
    assert(0); //illegal channel type
    break;
  }
}

double EImageCalc::UnnormalizeCoord(Value* c, unsigned dimSize) const
{
  double df;

  switch(c->Type())
  {
  case MV_INT32:
    df = static_cast<double>(c->S32());
    break;
  case MV_FLOAT:
    df = static_cast<double>(c->F());
    break;
  default:
    assert(0); //only f32 and s32 are allowed for rdimage
    break;
  }
  
  if(samplerCoord == BRIG_COORD_NORMALIZED)
  {
    assert(c->Type() == MV_FLOAT); //only f32 is allowed in normilized mode
    df *= dimSize;
  }

  if(samplerFilter == BRIG_FILTER_LINEAR)
  {
    assert(c->Type() == MV_FLOAT); //only f32 is allowed with linear filter
    df -= 0.5;
  }

  //coordinates are undefined in case of NaN or INF (PRM 7.1.6.1)
  assert(!isNanOrInf(df));

  return df;
}

double EImageCalc::UnnormalizeArrayCoord(Value* c) const
{
  double df;
  switch (c->Type())
  {
  case MV_INT32:
    df = static_cast<double>(c->S32());
    break;

  case MV_FLOAT:
    df = static_cast<double>(c->F());
    //coordinates are undefined in case of NaN or INF (PRM 7.1.6.1)
    assert(!isNanOrInf(df));
    break;

  default:
    assert(0); //illegal coord type
    break;
  }
  
  return df;
}

int32_t EImageCalc::round_downi(float f) const ///\todo (Artem) 
{
  return static_cast<int32_t>(floorf(f)); //it seems to be correct
}

int32_t EImageCalc::round_neari(float f) const ///\todo (Artem) 
{
  return static_cast<int32_t>(f); //incorrect!!
}

int32_t EImageCalc::clamp_i(int32_t a, int32_t min, int32_t max) const
{
  return (a < min) ? min : ((a > max) ? max : a);
}

float EImageCalc::clamp_f(float a, float min, float max) const
{
  return (a < min) ? min : ((a > max) ? max : a);
}

uint32_t EImageCalc::GetTexelIndex(double df, uint32_t dimSize) const
{
  int32_t rounded_coord = round_downi(df);
  bool out_of_range = (df < 0.0) || ( df >= static_cast<double>(dimSize));
  if(!out_of_range){
    return rounded_coord;
  };
  int32_t tile;
  double mirrored_coord;

  switch(samplerAddressing){
  case BRIG_ADDRESSING_UNDEFINED:
  //we should never use undefined addressing
  //in case of out of range coordinates
  //because it, well, undefined
  assert(0);
  break;
  case BRIG_ADDRESSING_CLAMP_TO_EDGE:
    if (df < 0.0) return 0;
    if (df > static_cast<double>(dimSize - 1)) return (dimSize - 1);
    return clamp_i(rounded_coord, 0, dimSize - 1);
    break;
  case BRIG_ADDRESSING_CLAMP_TO_BORDER:
    if (out_of_range) return dimSize;
    return clamp_i(rounded_coord, 0, dimSize);
    break;
  case BRIG_ADDRESSING_REPEAT:
    tile = round_downi(df / dimSize);
    rounded_coord = round_downi(df - tile * static_cast<double>(dimSize)); 
    return clamp_i(rounded_coord, 0, dimSize - 1);
    break;
  case BRIG_ADDRESSING_MIRRORED_REPEAT:
    mirrored_coord = (df < 0) ? (-df - 1.0f) : df;
    tile = round_downi(mirrored_coord / dimSize);
    rounded_coord = round_downi(mirrored_coord - tile * static_cast<double>(dimSize));
    rounded_coord = clamp_i(rounded_coord, 0, dimSize - 1);
    if(tile & 1){
      rounded_coord = (dimSize - 1) - rounded_coord;
    }
    return rounded_coord;
    break;
  default:
    assert(0); //illegal addressing mode
    break;
  }
  return -1;
}

uint32_t EImageCalc::GetTexelArrayIndex(double df, uint32_t dimSize) const
{
  if (df < 0.0) return 0;
  if (df > static_cast<double>(dimSize-1)) return (dimSize - 1);
  return clamp_i( round_neari(df), 0, dimSize - 1 );
}

void EImageCalc::LoadBorderData(Value* _color) const
{
  bool alpha_is_one = false;
  switch(imageChannelOrder){
  case BRIG_CHANNEL_ORDER_R:
  case BRIG_CHANNEL_ORDER_RG:
  case BRIG_CHANNEL_ORDER_RGB:
  case BRIG_CHANNEL_ORDER_SRGB:
  case BRIG_CHANNEL_ORDER_LUMINANCE:
    alpha_is_one = true;
    break;
  case BRIG_CHANNEL_ORDER_DEPTH:
  case BRIG_CHANNEL_ORDER_DEPTH_STENCIL:
    assert(0); //border color for depth images is implementation define //should we test it???
    break;
  default:
    break;
  }

  _color[0] = color_zero;
  _color[1] = color_zero;
  _color[2] = color_zero;
  _color[3] = alpha_is_one ? color_one : color_zero;
  return;
}

uint32_t EImageCalc::GetRawPixelData(uint32_t x_ind, uint32_t y_ind, uint32_t z_ind) const
{
  return existVal.U32();
}

uint32_t EImageCalc::GetRawChannelData(uint32_t x_ind, uint32_t y_ind, uint32_t z_ind, uint32_t channel) const
{
  switch (imageChannelType)
  {
  case BRIG_CHANNEL_TYPE_SNORM_INT8:
  case BRIG_CHANNEL_TYPE_UNORM_INT8:
  case BRIG_CHANNEL_TYPE_SIGNED_INT8:
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
    return (existVal.U32() >> channel*8) & 0xFF;
    break;
  case BRIG_CHANNEL_TYPE_SNORM_INT16:
  case BRIG_CHANNEL_TYPE_UNORM_INT16:
  case BRIG_CHANNEL_TYPE_SIGNED_INT16:
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    return (existVal.U64() >> channel*16) & 0xFFFF;
    break;
  case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    return (existVal.U64() >> channel*16) & 0xFFFF;
    break;
  case BRIG_CHANNEL_TYPE_UNORM_INT24:
    return (existVal.U64() >> channel*24) & 0x00FFFFFF;
    break;
  case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
  case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
  case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    assert(0); //GetRawPixelData() should be used for this formats
    break;
  case BRIG_CHANNEL_TYPE_SIGNED_INT32:
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
  case BRIG_CHANNEL_TYPE_FLOAT:
    if (imageChannelOrder == BRIG_CHANNEL_ORDER_RGBA) //this is the only case of 128 bit texel
    {
      if (channel < 2)
      {
        return existVal.U128().U64H() >> channel*32;
      }
      else
      {
        channel-=2;
        return existVal.U128().U64L() >> channel*32;
      }
    }
    else
      return existVal.U64() >> channel*32;
    break;
  default:
    assert(0); //illegal channel
    break;
  }
  return 0;
}

int32_t EImageCalc::SignExtend(uint32_t c, unsigned int bit_size) const
{
  union {
    int32_t s32;
    uint32_t u32;
  } r;

  uint32_t s = c & (1U << (bit_size - 1)); //sign_bit

  c = c & ((1U << bit_size) - 1); //zeroing higher bits
  r.u32 = (c ^ s) - s; //sign extention

  return r.s32;
}

float EImageCalc::ConvertionLoadSignedNormalize(uint32_t c, unsigned int bit_size) const
{
  float f = static_cast<float>(SignExtend(c, bit_size));
  f = f / ((1U << (bit_size-1)) - 1);

  return clamp_f(f, -1.0f, 1.0f); // values (-scale) and (-scale-1) should return -1.0f;
}

float EImageCalc::ConvertionLoadUnsignedNormalize(uint32_t c, unsigned int bit_size) const
{
  c = c & ((1U << bit_size) - 1); //zeroing higher bits
    
  float f = static_cast<float>(c) / ((1U << bit_size) - 1);

  return clamp_f(f, 0.0f, 1.0f);
}

float EImageCalc::SRGBtoLinearRGB(float f) const
{
  double df = f;

  //all magic numbers are from PRM section 7.1.4.1.2
  if (df < 0.04045)
    return static_cast<float>(df / 12.92);

  df = (df + 0.055) / 1.055;
  df = pow(df, 2.4);

  if(df > 1.0f)
    return 1.0f;

  return static_cast<float>(df);
}

float EImageCalc::LinearRGBtoSRGB(float f) const
{
  if(f != f) return 0.0f; //check for NaN
  if(f < 0.0f) return 0.0f;
  if(f > 1.0f) return 1.0f;

  //all magic numbers are from PRM section 7.1.4.1.2
  double df = f;
  if(df < 0.0031308)
  {
    df *= 12.92;
    return static_cast<float>(df);
  }

  df = 1.055 * pow(df, 1.0f/2.4);
  df -= 0.055;

  return static_cast<float>(df);
}

int32_t EImageCalc::ConvertionLoadSignedClamp(uint32_t c, unsigned int bit_size) const
{
  return SignExtend(c, bit_size);
}

uint32_t EImageCalc::ConvertionLoadUnsignedClamp(uint32_t c, unsigned int bit_size) const
{
  return c & ((1U << bit_size) - 1); //zeroing higher bits
}

float EImageCalc::ConvertionLoadHalfFloat(uint32_t data) const
{
  ValueData bits;
  bits.u32 = data;
  half h = half::make(bits.h_bits);

  float f = h.f32();
  assert(f==f); //handling NaNs is implementation defined
  return f;
}

float EImageCalc::ConvertionLoadFloat(uint32_t data) const
{
  ValueData f;
  f.u32 = data;
  //handling NaNs and subnormals is implementation defined
  //therefore we should avoid such tests
  assert(!isNanOrDenorm(f.f));
  return f.f;
}

uint32_t EImageCalc::ConvertionStoreSignedNormalize(float f, unsigned int bit_size) const
{
  int32_t scale = (1 << (bit_size-1)) - 1;
  if (f >=  1.0f) return scale;
  if (f <= -1.0f) return -scale - 1;
  if (f != f) return 0;
  int32_t val = round_neari(f * scale);
  val = clamp_i(val, -scale - 1, scale);

  return val;
}

uint32_t EImageCalc::ConvertionStoreUnsignedNormalize(float f, unsigned int bit_size) const
{
  int32_t scale = (1 << bit_size) - 1;
  if (f >= 1.0f) return scale;
  if (f <= 0.0f) return 0;
  if (f != f) return 0;
  int32_t val = round_neari(f * scale);
  val = clamp_i(val, 0, scale);

  return val;
}

uint32_t EImageCalc::ConvertionStoreSignedClamp(int32_t c, unsigned int bit_size) const
{
  int32_t max = (1 << (bit_size-1)) - 1;
  int32_t min = -max - 1;
  uint32_t val = clamp_i(c, min, max);

  return val;
}

uint32_t EImageCalc::ConvertionStoreUnsignedClamp(uint32_t c, unsigned int bit_size) const
{
  uint32_t max = (1 << bit_size) - 1;
  return (c < max) ? c : max;
}

uint32_t EImageCalc::ConvertionStoreHalfFloat(float f) const
{
  half h(f); //Achtung! Rounding is implementation defined!
  assert(f==f); //handling NaNs is implementation difined
  //todo test for fp16 denorm
  return h.getBits();
}

uint32_t EImageCalc::ConvertionStoreFloat(float f) const
{
  ValueData v;
  v.f = f;
  assert(!isNanOrDenorm(f));
  return v.u32;
}

Value EImageCalc::ConvertRawData(uint32_t data) const
{
  Value c;
  switch (imageChannelType)
  {
  case BRIG_CHANNEL_TYPE_SNORM_INT8:
    c = Value(ConvertionLoadSignedNormalize(data, 8));
    break;
  case BRIG_CHANNEL_TYPE_SNORM_INT16:
    c = Value(ConvertionLoadSignedNormalize(data, 16));
    break;

  case BRIG_CHANNEL_TYPE_UNORM_INT8:
    c = Value(ConvertionLoadUnsignedNormalize(data, 8));
    break;
  case BRIG_CHANNEL_TYPE_UNORM_INT16:
    c = Value(ConvertionLoadUnsignedNormalize(data, 16));
    break;
  case BRIG_CHANNEL_TYPE_UNORM_INT24:
    c = Value(ConvertionLoadUnsignedNormalize(data, 24));
    break;
  case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
  case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
  case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    //should never happen, because rgb and rgbx channel orders
    //are handled directly in LoadColorData()
    assert(0);
    break;
  case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    c = Value(MV_INT32, S32(ConvertionLoadSignedClamp(data, 8)));
    break;
  case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    c = Value(MV_INT32, S32(ConvertionLoadSignedClamp(data, 16)));
    break;
  case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    c = Value(MV_INT32, S32(data));
    break;
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
    c = Value(MV_UINT32, U32(ConvertionLoadUnsignedClamp(data, 8)));
    break;
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    c = Value(MV_UINT32, U32(ConvertionLoadUnsignedClamp(data, 16)));
    break;
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    c = Value(MV_UINT32, U32(data));
    break;
  case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    c = Value(ConvertionLoadHalfFloat(data));
    break;
  case BRIG_CHANNEL_TYPE_FLOAT:
    c = Value(ConvertionLoadFloat(data));
    break;
  default:
    assert(0);
    break;
  }

  return c;
}

void EImageCalc::LoadColorData(uint32_t x_ind, uint32_t y_ind, uint32_t z_ind, Value* _color) const
{
  Value c;
  uint32_t packed_color;
  uint32_t unpacked_r, unpacked_g, unpacked_b;
  float fr, fg, fb, fa;

  switch (imageChannelOrder)
  {
  case BRIG_CHANNEL_ORDER_A:
    _color[0] = color_zero;
    _color[1] = color_zero;
    _color[2] = color_zero;
    _color[3] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    break;
  case BRIG_CHANNEL_ORDER_R:
  case BRIG_CHANNEL_ORDER_RX:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    _color[1] = color_zero;
    _color[2] = color_zero;
    _color[3] = color_one;
    break;
  case BRIG_CHANNEL_ORDER_RG:
  case BRIG_CHANNEL_ORDER_RGX:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    _color[1] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 1));
    _color[2] = color_zero;
    _color[3] = color_one;
    break;
  case BRIG_CHANNEL_ORDER_RA:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    _color[1] = color_zero;
    _color[2] = color_zero;
    _color[3] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 1));
    break;
  case BRIG_CHANNEL_ORDER_RGB:
  case BRIG_CHANNEL_ORDER_RGBX:
    packed_color = GetRawPixelData(x_ind, y_ind, z_ind);
    switch (imageChannelType)
    {
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
      unpacked_b = packed_color & 0x1F;
      unpacked_g = (packed_color >> 5) & 0x1F;
      unpacked_r = (packed_color >> 10) & 0x1F;
      _color[0] = Value(ConvertionLoadUnsignedNormalize(unpacked_r, 5));
      _color[1] = Value(ConvertionLoadUnsignedNormalize(unpacked_g, 5));
      _color[2] = Value(ConvertionLoadUnsignedNormalize(unpacked_b, 5));
      break;
    case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      unpacked_b = packed_color & 0x1F;
      unpacked_g = (packed_color >> 5) & 0x3F;
      unpacked_r = (packed_color >> 11) & 0x1F;
      _color[0] = Value(ConvertionLoadUnsignedNormalize(unpacked_r, 5));
      _color[1] = Value(ConvertionLoadUnsignedNormalize(unpacked_g, 6));
      _color[2] = Value(ConvertionLoadUnsignedNormalize(unpacked_b, 5));
      break;
    case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
      unpacked_b = packed_color & 0x03FF;
      unpacked_g = (packed_color >> 10) & 0x03FF;
      unpacked_r = (packed_color >> 20) & 0x03FF;
      _color[0] = Value(ConvertionLoadUnsignedNormalize(unpacked_r, 10));
      _color[1] = Value(ConvertionLoadUnsignedNormalize(unpacked_g, 10));
      _color[2] = Value(ConvertionLoadUnsignedNormalize(unpacked_b, 10));
      break;
    default:
      assert(0); //rgb and rgbx channel orders are legal to use only with 555, 565 or 101010 channel type
      break;
    }
    _color[3] = color_one;
    break;
  case BRIG_CHANNEL_ORDER_RGBA:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    _color[1] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 1));
    _color[2] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 2));
    _color[3] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 3));
    break;
  case BRIG_CHANNEL_ORDER_BGRA:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 2));
    _color[1] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 1));
    _color[2] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    _color[3] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 3));
    break;
  case BRIG_CHANNEL_ORDER_ARGB:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 1));
    _color[1] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 2));
    _color[2] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 3));
    _color[3] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    break;
  case BRIG_CHANNEL_ORDER_ABGR:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 3));
    _color[1] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 2));
    _color[2] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 1));
    _color[3] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    break;
  case BRIG_CHANNEL_ORDER_SRGB:
  case BRIG_CHANNEL_ORDER_SRGBX:
    assert(imageChannelType == BRIG_CHANNEL_TYPE_UNORM_INT8); //only unorm_int8 is supported for s-Form
    fr = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 0), 8);
    fg = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 1), 8);
    fb = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 2), 8);
    _color[0] = Value(SRGBtoLinearRGB(fr));
    _color[1] = Value(SRGBtoLinearRGB(fg));
    _color[2] = Value(SRGBtoLinearRGB(fb));
    _color[3] = color_one;
    break;
  case BRIG_CHANNEL_ORDER_SRGBA:
    assert(imageChannelType == BRIG_CHANNEL_TYPE_UNORM_INT8); //only unorm_int8 is supported for s-Form
    fr = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 0), 8);
    fg = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 1), 8);
    fb = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 2), 8);
    fa = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 3), 8);
    _color[0] = Value(SRGBtoLinearRGB(fr));
    _color[1] = Value(SRGBtoLinearRGB(fg));
    _color[2] = Value(SRGBtoLinearRGB(fb));
    _color[3] = Value(fa);
    break;
  case BRIG_CHANNEL_ORDER_SBGRA:
    assert(imageChannelType == BRIG_CHANNEL_TYPE_UNORM_INT8); //only unorm_int8 is supported for s-Form
    fr = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 2), 8);
    fg = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 1), 8);
    fb = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 0), 8);
    fa = ConvertionLoadUnsignedNormalize(GetRawChannelData(x_ind, y_ind, z_ind, 3), 8);
    _color[0] = Value(SRGBtoLinearRGB(fr));
    _color[1] = Value(SRGBtoLinearRGB(fg));
    _color[2] = Value(SRGBtoLinearRGB(fb));
    _color[3] = Value(fa);
    break;
  case BRIG_CHANNEL_ORDER_INTENSITY:
    c = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    _color[0] = c;
    _color[1] = c;
    _color[2] = c;
    _color[3] = c;
    break;
  case BRIG_CHANNEL_ORDER_LUMINANCE:
    c = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    _color[0] = c;
    _color[1] = c;
    _color[2] = c;
    _color[3] = color_one;
    break;
  case BRIG_CHANNEL_ORDER_DEPTH:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    break;
  case BRIG_CHANNEL_ORDER_DEPTH_STENCIL:
    _color[0] = ConvertRawData(GetRawChannelData(x_ind, y_ind, z_ind, 0));
    break;
  default:
    assert(0); //illegal channel order
    break;
  }
}

void EImageCalc::LoadTexel(uint32_t x_ind, uint32_t y_ind, uint32_t z_ind, Value* _color) const
{
  uint32_t dimSizeX = imageGeometry.ImageSize(0);
  uint32_t dimSizeY = imageGeometry.ImageSize(1);
  uint32_t dimSizeZ = imageGeometry.ImageSize(2);
  bool out_of_range = (x_ind >= dimSizeX) || (y_ind >= dimSizeY) || (z_ind >= dimSizeZ);
  if(out_of_range){
    //could only happen in case of ADDRESSING_CLAMP_TO_BORDER
    assert(samplerAddressing == BRIG_ADDRESSING_CLAMP_TO_BORDER);
    //currently border color for depth images is implementation defined (PRM table 7-2 Channel Order Properties)
    assert(!isDepth);
    LoadBorderData(_color);
  } else {
    LoadColorData(x_ind, y_ind, z_ind, _color);
  }
}

void EImageCalc::LoadFloatTexel(uint32_t x, uint32_t y, uint32_t z, double* const df) const
{
  Value color[4];
  LoadTexel(x, y, z, color);
  switch (color[0].Type())
  {
  case MV_FLOAT:
    df[0] = color[0].F();
    df[1] = color[1].F();
    df[2] = color[2].F();
    df[3] = color[3].F();
    break;

  case MV_INT32:
    df[0] = color[0].S32();
    df[1] = color[1].S32();
    df[2] = color[2].S32();
    df[3] = color[3].S32();
    break;

  case MV_UINT32:
    df[0] = color[0].U32();
    df[1] = color[1].U32();
    df[2] = color[2].U32();
    df[3] = color[3].U32();
    break;

  default:
    assert(0);
    break;
  }
}

void EImageCalc::EmulateImageRd(Value* _coords, Value* _color) const
{
  //PRM states that in some modes texel index must be computed with
  //no loss of precision (7.1.6.3 Image Filters).
  //Therefore we are using doubles for coordinates.
  double u, v, w; //unnormalized coordinates
  uint32_t ind[3]; //element location

  Value* x = &_coords[0];
  Value* y = &_coords[1];
  Value* z = &_coords[2];

  uint32_t uSize = imageGeometry.ImageSize(0);
  uint32_t vSize = imageGeometry.ImageSize(1);
  uint32_t wSize = imageGeometry.ImageSize(2);

  //1DB images are not supported by rdimage instruction (PRM 7.1.6)
  assert(imageGeometryProp != BRIG_GEOMETRY_1DB);

  //unnormalize and apply addresing mode
  u = UnnormalizeCoord(x, uSize);
  v = w = 0.0f;
  ind[0] = GetTexelIndex(u, uSize);
  ind[1] = ind[2] = 0;
  switch(imageGeometryProp)
  {
  case BRIG_GEOMETRY_1D:
  case BRIG_GEOMETRY_1DB:
    break;
  case BRIG_GEOMETRY_1DA:
    v = UnnormalizeArrayCoord(y);
    ind[1] = GetTexelArrayIndex(v, vSize);
    break;
  case BRIG_GEOMETRY_2D:
  case BRIG_GEOMETRY_2DDEPTH:
    v = UnnormalizeCoord(y, vSize);
    ind[1] = GetTexelIndex(v, vSize);
    break;
  case BRIG_GEOMETRY_2DA:
  case BRIG_GEOMETRY_2DADEPTH:
    v = UnnormalizeCoord(y, vSize);
    w = UnnormalizeArrayCoord(z);
    ind[1] = GetTexelIndex(v, vSize);
    ind[2] = GetTexelArrayIndex(w, wSize);
    break;
  case BRIG_GEOMETRY_3D:
    v = UnnormalizeCoord(y, vSize);
    w = UnnormalizeCoord(z, wSize);
    ind[1] = GetTexelIndex(v, vSize);
    ind[2] = GetTexelIndex(w, wSize);
    break;
  default:
    break;
  }

  //apply filtering
  if(samplerFilter == BRIG_FILTER_NEAREST)
  {
    LoadTexel(ind[0], ind[1], ind[2], _color);
    return;
  }

  //linear filtering
  assert(samplerFilter == BRIG_FILTER_LINEAR); //we are supporting only nearest and linear filters
  uint32_t x0_index = ind[0];
  uint32_t x1_index = GetTexelIndex(u + 1.0f, imageGeometry.ImageSize(0));
  float x_frac = u - floorf(u);

  uint32_t y0_index = ind[1];
  uint32_t y1_index = ind[1];
  float y_frac = v - floorf(v);

  uint32_t z0_index = ind[2];
  uint32_t z1_index = ind[2];
  float z_frac = w - floorf(w);

  double filtered_color[4];
  double colors[8][4];

  switch (imageGeometryProp)
  {
  case BRIG_GEOMETRY_1D:
  case BRIG_GEOMETRY_1DA:
    LoadFloatTexel(x0_index, y0_index, z0_index, colors[0]);
    LoadFloatTexel(x1_index, y0_index, z0_index, colors[1]);
    for(int i=0; i<4; i++){
      filtered_color[i] = (1 - x_frac) * colors[0][i] + x_frac * colors[1][i];
    }
    break;
  case BRIG_GEOMETRY_2D:
  case BRIG_GEOMETRY_2DA:
  case BRIG_GEOMETRY_2DDEPTH:
  case BRIG_GEOMETRY_2DADEPTH:
    y1_index = GetTexelIndex(v + 1.0f, imageGeometry.ImageSize(1));
    LoadFloatTexel(x0_index, y0_index, z0_index, colors[0]);
    LoadFloatTexel(x1_index, y0_index, z0_index, colors[1]);
    LoadFloatTexel(x0_index, y1_index, z0_index, colors[2]);
    LoadFloatTexel(x1_index, y1_index, z0_index, colors[3]);
    for(int i=0; i< (isDepth ? 1 : 4); i++){
      filtered_color[i] = (1 - x_frac)  * (1 - y_frac)  * colors[0][i]
              + x_frac    * (1 - y_frac)  * colors[1][i]
              + (1 - x_frac)  * y_frac    * colors[2][i]
              + x_frac    * y_frac    * colors[3][i];
    }
    break;
  case BRIG_GEOMETRY_3D:
    y1_index = GetTexelIndex(v + 1.0f, imageGeometry.ImageSize(1));
    z1_index = GetTexelIndex(w + 1.0f, imageGeometry.ImageSize(2));
    LoadFloatTexel(x0_index, y0_index, z0_index, colors[0]);
    LoadFloatTexel(x1_index, y0_index, z0_index, colors[1]);
    LoadFloatTexel(x0_index, y1_index, z0_index, colors[2]);
    LoadFloatTexel(x1_index, y1_index, z0_index, colors[3]);
    LoadFloatTexel(x0_index, y0_index, z1_index, colors[4]);
    LoadFloatTexel(x1_index, y0_index, z1_index, colors[5]);
    LoadFloatTexel(x0_index, y1_index, z1_index, colors[6]);
    LoadFloatTexel(x1_index, y1_index, z1_index, colors[7]);
    for(int i=0; i<4; i++){
      filtered_color[i] = (1 - x_frac)  * (1 - y_frac)  * (1 - z_frac)  * colors[0][i]
              + x_frac    * (1 - y_frac)  * (1 - z_frac)  * colors[1][i]
              + (1 - x_frac)  * y_frac    * (1 - z_frac)  * colors[2][i]
              + x_frac    * y_frac    * (1 - z_frac)  * colors[3][i]
              + (1 - x_frac)  * (1 - y_frac)  * z_frac    * colors[4][i]
              + x_frac    * (1 - y_frac)  * z_frac    * colors[5][i]
              + (1 - x_frac)  * y_frac    * z_frac    * colors[6][i]
              + x_frac    * y_frac    * z_frac    * colors[7][i];
    }
    break;
  
  default:
    assert(0);
    break;
  }

  switch (imageChannelType)
  {
  case BRIG_CHANNEL_TYPE_SNORM_INT8:
  case BRIG_CHANNEL_TYPE_SNORM_INT16:
  case BRIG_CHANNEL_TYPE_UNORM_INT8:
  case BRIG_CHANNEL_TYPE_UNORM_INT16:
  case BRIG_CHANNEL_TYPE_UNORM_INT24:
  case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
  case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
  case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
  case BRIG_CHANNEL_TYPE_HALF_FLOAT:
  case BRIG_CHANNEL_TYPE_FLOAT:
    for(int i=0; i<(isDepth ? 1 : 4); i++)
      _color[i] = Value(static_cast<float>(filtered_color[i]));
    break;
  case BRIG_CHANNEL_TYPE_SIGNED_INT8:
  case BRIG_CHANNEL_TYPE_SIGNED_INT16:
  case BRIG_CHANNEL_TYPE_SIGNED_INT32:
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
  case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    assert(0); //linear filter is defined only for channel types with f32 access type (PRM table 7-6)
    break;
  default:
    assert(0);
    break;
  }
}

void EImageCalc::Init(EImage * eimage, ESampler* esampler)
{
  assert(eimage);
  imageGeometry = eimage->ImageGeometry();
  imageGeometryProp = eimage->Geometry();
  imageChannelOrder = eimage->ChannelOrder();
  imageChannelType = eimage->ChannelType();
  if (esampler != NULL) {
    samplerCoord = esampler->Coord();
    samplerFilter = esampler->Filter();
    samplerAddressing = esampler->Addressing();    
    assert(IsSamplerLegal(samplerCoord, samplerFilter, samplerAddressing));
  }
  else {
    samplerCoord = BRIG_COORD_UNNORMALIZED;
    samplerFilter = BRIG_FILTER_NEAREST;
    samplerAddressing = BRIG_ADDRESSING_UNDEFINED;
  }
  SetupDefaultColors();

  assert(IsImageLegal(imageGeometryProp, imageChannelOrder, imageChannelType));

  switch (imageChannelOrder)
  {
  case BRIG_CHANNEL_ORDER_SRGB:
  case BRIG_CHANNEL_ORDER_SRGBX:
  case BRIG_CHANNEL_ORDER_SRGBA:
  case BRIG_CHANNEL_ORDER_SBGRA:
    isSRGB = true;
    break;
  default:
    isSRGB = false;
    break;
  }

  isDepth = IsImageDepth(imageGeometryProp);

  //set default
  existVal = eimage->GetInitialData();
}

Value EImageCalc::PackChannelDataToMemoryFormat(Value* _color) const
{
  Value rgba[4];
  union RawData {
    uint8_t data8;
    uint16_t data16;
    uint32_t data32;
    uint64_t data64;
    uint128_t::Type data128;
    struct RawChData8 {
      uint8_t ch1;
      uint8_t ch2;
      uint8_t ch3;
      uint8_t ch4;
    } ch8;
    struct RawChData16 {
      uint16_t ch1;
      uint16_t ch2;
      uint16_t ch3;
      uint16_t ch4;
    } ch16;
    struct RawChData32 {
      uint32_t ch1;
      uint32_t ch2;
      uint32_t ch3;
      uint32_t ch4;
    } ch32;
  };
  RawData Raw;

  Value texel;
  int bits_per_channel = -1;
  uint32_t packed_rgb = 0;
  uint32_t unpacked_r, unpacked_g, unpacked_b;

  //convert channel data
  switch (imageChannelType)
  {
  case BRIG_CHANNEL_TYPE_SNORM_INT8:
    bits_per_channel = 8;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreSignedNormalize(_color[i].F(), bits_per_channel));
    break;

  case BRIG_CHANNEL_TYPE_SNORM_INT16:
    bits_per_channel = 16;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreSignedNormalize(_color[i].F(), bits_per_channel));
    break;

  case BRIG_CHANNEL_TYPE_UNORM_INT8:
    bits_per_channel = 8;
    for(int i=0; i<4; i++)
    {
      float f = _color[i].F();
      if (isSRGB) f = LinearRGBtoSRGB(f);
      rgba[i] = Value(MV_UINT32, ConvertionStoreUnsignedNormalize(f, bits_per_channel));
    }
    break;

  case BRIG_CHANNEL_TYPE_UNORM_INT16:
    bits_per_channel = 16;
    if(isDepth)
      return Value(MV_UINT16, ConvertionStoreUnsignedNormalize(_color[0].F(), bits_per_channel));
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreUnsignedNormalize(_color[i].F(), bits_per_channel));
    break;

  case BRIG_CHANNEL_TYPE_UNORM_INT24:
    bits_per_channel = 24;
    return Value(MV_UINT32, ConvertionStoreUnsignedNormalize(_color[0].F(), bits_per_channel));
    break;

  case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
    unpacked_r = ConvertionStoreUnsignedNormalize(_color[0].F(), 5);
    unpacked_g = ConvertionStoreUnsignedNormalize(_color[1].F(), 5);
    unpacked_b = ConvertionStoreUnsignedNormalize(_color[2].F(), 5);
    packed_rgb = (unpacked_r << 10) | (unpacked_g << 5) | unpacked_b;
    return Value(MV_UINT16, packed_rgb);
    break;

  case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
    unpacked_r = ConvertionStoreUnsignedNormalize(_color[0].F(), 5);
    unpacked_g = ConvertionStoreUnsignedNormalize(_color[1].F(), 6);
    unpacked_b = ConvertionStoreUnsignedNormalize(_color[2].F(), 5);
    packed_rgb = (unpacked_r << 11) | (unpacked_g << 6) | unpacked_b;
    return Value(MV_UINT16, packed_rgb);
    break;

  case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
    unpacked_r = ConvertionStoreUnsignedNormalize(_color[0].F(), 10);
    unpacked_g = ConvertionStoreUnsignedNormalize(_color[1].F(), 10);
    unpacked_b = ConvertionStoreUnsignedNormalize(_color[2].F(), 10);
    packed_rgb = (unpacked_r << 20) | (unpacked_g << 10) | unpacked_b;
    return Value(MV_UINT32, packed_rgb);
    break;

  case BRIG_CHANNEL_TYPE_SIGNED_INT8:
    bits_per_channel = 8;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreSignedClamp(_color[i].S32(), bits_per_channel));
    break;

  case BRIG_CHANNEL_TYPE_SIGNED_INT16:
    bits_per_channel = 16;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreSignedClamp(_color[i].S32(), bits_per_channel));
    break;

  case BRIG_CHANNEL_TYPE_SIGNED_INT32:
    bits_per_channel = 32;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, _color[i].U32());
    break;

  case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
    bits_per_channel = 8;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreUnsignedClamp(_color[i].U32(), bits_per_channel));
    break;

  case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
    bits_per_channel = 16;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreUnsignedClamp(_color[i].U32(), bits_per_channel));
    break;

  case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
    bits_per_channel = 32;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, _color[i].U32());
    break;

  case BRIG_CHANNEL_TYPE_HALF_FLOAT:
    bits_per_channel = 16;
    for(int i=0; i<4; i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreHalfFloat(_color[i].F()));
    break;

  case BRIG_CHANNEL_TYPE_FLOAT:
    bits_per_channel = 32;
    if(isDepth)
      return Value(MV_UINT32, ConvertionStoreFloat(_color[0].F()));
    for(int i=0; i < (isDepth ? 1 : 4); i++)
      rgba[i] = Value(MV_UINT32, ConvertionStoreFloat(_color[i].F()));
    break;

  default:
    assert(0); //illeral channel type
    break;
  }

  Raw.data128.h = 0;
  Raw.data128.l = 0;

  texel = Value(MV_UINT32, 0);

  //assemble channels
  switch (imageChannelOrder)
  {
  case BRIG_CHANNEL_ORDER_A:
    switch (bits_per_channel)
    {
    case 8:
      texel = Value(MV_UINT8, rgba[3].U8());
      break;
    case 16:
      texel = Value(MV_UINT16, rgba[3].U16());
      break;
    case 32:
      texel = Value(MV_UINT32, rgba[3].U32());
      break;
    default:
      assert(0);
      break;
    }
    break;
  
  case BRIG_CHANNEL_ORDER_R:
  case BRIG_CHANNEL_ORDER_RX:
  case BRIG_CHANNEL_ORDER_INTENSITY:
  case BRIG_CHANNEL_ORDER_LUMINANCE:
    switch (bits_per_channel)
    {
    case 8:
      texel = Value(MV_UINT8, rgba[0].U8());
      break;
    case 16:
      texel = Value(MV_UINT16, rgba[0].U16());
      break;
    case 32:
      texel = Value(MV_UINT32, rgba[0].U32());
      break;
    default:
      assert(0);
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_RG:
  case BRIG_CHANNEL_ORDER_RGX:
    switch (bits_per_channel)
    {
    case 8:
      Raw.ch8.ch1 = rgba[0].U8();
      Raw.ch8.ch2 = rgba[1].U8();
      texel = Value(MV_UINT16, Raw.data16);
      break;
    case 16:
      Raw.ch16.ch1 = rgba[0].U16();
      Raw.ch16.ch2 = rgba[1].U16();
      texel = Value(MV_UINT32, Raw.data32);
      break;
    case 32:
      Raw.ch32.ch1 = rgba[0].U32();
      Raw.ch32.ch2 = rgba[1].U32();
      texel = Value(MV_UINT64, Raw.data64);
      break;
    default:
      assert(0);
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_RA:
    switch (bits_per_channel)
    {
    case 8:
      Raw.ch8.ch1 = rgba[0].U8();
      Raw.ch8.ch2 = rgba[3].U8();
      texel = Value(MV_UINT16, Raw.data16);
      break;
    case 16:
      Raw.ch16.ch1 = rgba[0].U16();
      Raw.ch16.ch2 = rgba[3].U16();
      texel = Value(MV_UINT32, Raw.data32);
      break;
    case 32:
      Raw.ch32.ch1 = rgba[0].U32();
      Raw.ch32.ch2 = rgba[3].U32();
      texel = Value(MV_UINT64, Raw.data64);
      break;
    default:
      assert(0);
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_RGB:
  case BRIG_CHANNEL_ORDER_RGBX:
  case BRIG_CHANNEL_ORDER_DEPTH:
  case BRIG_CHANNEL_ORDER_DEPTH_STENCIL:
    assert(0); //should never happen, coz these formats are handled earlier
    break;
  case BRIG_CHANNEL_ORDER_RGBA:
  case BRIG_CHANNEL_ORDER_SRGBA:
    switch (bits_per_channel)
    {
    case 8:
      Raw.ch8.ch1 = rgba[0].U8();
      Raw.ch8.ch2 = rgba[1].U8();
      Raw.ch8.ch3 = rgba[2].U8();
      Raw.ch8.ch4 = rgba[3].U8();
      texel = Value(MV_UINT32, Raw.data32);
      break;
    case 16:
      Raw.ch16.ch1 = rgba[0].U16();
      Raw.ch16.ch2 = rgba[1].U16();
      Raw.ch16.ch3 = rgba[2].U16();
      Raw.ch16.ch4 = rgba[3].U16();
      texel = Value(MV_UINT64, Raw.data64);
      break;
    case 32:
      Raw.ch32.ch1 = rgba[0].U32();
      Raw.ch32.ch2 = rgba[1].U32();
      Raw.ch32.ch3 = rgba[2].U32();
      Raw.ch32.ch4 = rgba[3].U32();
      texel = Value(Raw.data128);
      break;
    default:
      assert(0);
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_BGRA:
  case BRIG_CHANNEL_ORDER_SBGRA:
    switch (bits_per_channel)
    {
    case 8:
      Raw.ch8.ch1 = rgba[2].U8();
      Raw.ch8.ch2 = rgba[1].U8();
      Raw.ch8.ch3 = rgba[0].U8();
      Raw.ch8.ch4 = rgba[3].U8();
      texel = Value(MV_UINT32, Raw.data32);
      break;
    case 16:
      Raw.ch16.ch1 = rgba[2].U16();
      Raw.ch16.ch2 = rgba[1].U16();
      Raw.ch16.ch3 = rgba[0].U16();
      Raw.ch16.ch4 = rgba[3].U16();
      texel = Value(MV_UINT64, Raw.data64);
      break;
    case 32:
      Raw.ch32.ch1 = rgba[2].U32();
      Raw.ch32.ch2 = rgba[1].U32();
      Raw.ch32.ch3 = rgba[0].U32();
      Raw.ch32.ch4 = rgba[3].U32();
      texel = Value(Raw.data128);
      break;
    default:
      assert(0);
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_ARGB:
    switch (bits_per_channel)
    {
    case 8:
      Raw.ch8.ch1 = rgba[3].U8();
      Raw.ch8.ch2 = rgba[0].U8();
      Raw.ch8.ch3 = rgba[1].U8();
      Raw.ch8.ch4 = rgba[2].U8();
      texel = Value(MV_UINT32, Raw.data32);
      break;
    case 16:
      Raw.ch16.ch1 = rgba[3].U16();
      Raw.ch16.ch2 = rgba[0].U16();
      Raw.ch16.ch3 = rgba[1].U16();
      Raw.ch16.ch4 = rgba[2].U16();
      texel = Value(MV_UINT64, Raw.data64);
      break;
    case 32:
      Raw.ch32.ch1 = rgba[3].U32();
      Raw.ch32.ch2 = rgba[0].U32();
      Raw.ch32.ch3 = rgba[1].U32();
      Raw.ch32.ch4 = rgba[2].U32();
      texel = Value(Raw.data128);
      break;
    default:
      assert(0);
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_ABGR:
    switch (bits_per_channel)
    {
    case 8:
      Raw.ch8.ch1 = rgba[3].U8();
      Raw.ch8.ch2 = rgba[2].U8();
      Raw.ch8.ch3 = rgba[1].U8();
      Raw.ch8.ch4 = rgba[0].U8();
      texel = Value(MV_UINT32, Raw.data32);
      break;
    case 16:
      Raw.ch16.ch1 = rgba[3].U16();
      Raw.ch16.ch2 = rgba[2].U16();
      Raw.ch16.ch3 = rgba[1].U16();
      Raw.ch16.ch4 = rgba[0].U16();
      texel = Value(MV_UINT64, Raw.data64);
      break;
    case 32:
      Raw.ch32.ch1 = rgba[3].U32();
      Raw.ch32.ch2 = rgba[2].U32();
      Raw.ch32.ch3 = rgba[1].U32();
      Raw.ch32.ch4 = rgba[0].U32();
      texel = Value(Raw.data128);
      break;
    default:
      assert(0);
      break;
    }
    break;
  case BRIG_CHANNEL_ORDER_SRGB:
  case BRIG_CHANNEL_ORDER_SRGBX:
    Raw.ch8.ch1 = rgba[0].U8();
    Raw.ch8.ch2 = rgba[1].U8();
    Raw.ch8.ch3 = rgba[2].U8();
    Raw.ch8.ch4 = 0;
    texel = Value(MV_UINT32, Raw.data32); //add uint24?
    break;

  default:
    assert(0); //illegal channel order
    break;
  }

  return texel;
}

Value EImageCalc::EmulateImageSt(Value* _coords, Value* _color) const
{
  if(_coords){
    //only u32 coordinate type is supported for stimage
    assert(_coords[0].Type() == MV_UINT32);

    uint32_t u, v, w;
    u = _coords[0].U32();
    v = _coords[1].U32();
    w = _coords[2].U32();
    
    uint32_t uSize = imageGeometry.ImageSize(0);
    uint32_t vSize = imageGeometry.ImageSize(1);
    uint32_t wSize = imageGeometry.ImageSize(2);

    //It is undefined if a coordinate is out of bounds
    assert((u < uSize) && (v < vSize) && (w < wSize));
  }

  return PackChannelDataToMemoryFormat(_color);
}

void EImageCalc::EmulateImageLd(Value* _coords, Value* _color) const
{
  //only u32 coordinate type is supported for ldimage
  assert(_coords[0].Type() == MV_UINT32);

  uint32_t u, v, w;
  u = _coords[0].U32();
  v = _coords[1].U32();
  w = _coords[2].U32();
    
  uint32_t uSize = imageGeometry.ImageSize(0);
  uint32_t vSize = imageGeometry.ImageSize(1);
  uint32_t wSize = imageGeometry.ImageSize(2);

  //It is undefined if a coordinate is out of bounds
  assert((u < uSize) && (v < vSize) && (w < wSize));

  LoadColorData(u, v, w, _color);
}

void ESignal::KernelArguments()
{
  kernelArg = te->Brig()->EmitVariableDefinition(Id(), BRIG_SEGMENT_KERNARG, te->Brig()->SignalType());
}

void ESignal::ScenarioInit()
{
  te->TestScenario()->Commands()->SignalCreate(Id(), initialValue);
}

TypedReg ESignal::AddReg()
{
  return te->Brig()->AddTReg(te->Brig()->SignalType());
}

TypedReg ESignal::AddValueReg()
{
  return te->Brig()->AddTReg(te->Brig()->SignalValueIntType());
}

void ESignal::SetupDispatch(const std::string& dispatchId)
{
  te->TestScenario()->Commands()->DispatchArg(dispatchId, DARG_SIGNAL, Id());
}

TypedReg ESignal::Handle()
{
  if (!handle) {
    assert(kernelArg != 0);
    handle = AddReg();
    te->Brig()->EmitLoad(BRIG_SEGMENT_KERNARG, handle, te->Brig()->Address(kernelArg));
  }
  return handle;
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

void EKernel::Declaration()
{
  kernel = te->Brig()->StartKernel(KernelName(), false);
  KernelArguments();
}

void EKernel::Definition()
{
  kernel = te->Brig()->StartKernel(KernelName(), true);
  KernelArguments();
}

void EFunction::StartFunction()
{
  function = te->Brig()->StartFunction(FunctionName());
}

void EFunction::EndFunction()
{
  te->Brig()->EndFunction();
}

void EFunction::Declaration()
{
  function = te->Brig()->StartFunction(FunctionName(), false);
  FunctionFormalOutputArguments();
  FunctionFormalInputArguments();
  te->Brig()->StartModuleScope();
}

void EFunction::Definition()
{
  function = te->Brig()->StartFunction(FunctionName(), true);
  FunctionFormalOutputArguments();
  FunctionFormalInputArguments();
}

void EFunction::StartFunctionBody()
{
  te->Brig()->StartBody();
}


void EModule::CreateModule()
{
  brigContainer = te->Brig()->Start();
}

void EModule::StartModule()
{
  module = te->Brig()->StartModule(ModuleName());
}

void EModule::EndModule()
{
  te->InitialContext()->Move(id.str() + ".brig", brigContainer.get());
  te->Brig()->End();
}

EModule::~EModule() {
  te->Brig()->DestroyBrigContainer(brigContainer);
}

void EModule::ScenarioProgram()
{
  EmittableContainer::ScenarioProgram();
  te->TestScenario()->Commands()->ModuleCreateFromBrig(Id(), id.str() + ".brig");
  te->TestScenario()->Commands()->ProgramAddModule("program", Id());
}

EDispatch::EDispatch(TestEmitter* te, const std::string& id_, const std::string& executableId_, 
  const std::string& kernelName_, Grid geometry_)
  : EmittableContainerWithId(te, id_),
  executableId(executableId_.c_str(), te->Ap()), 
  kernelName(kernelName_.c_str(), te->Ap()),
  geometry(geometry_)
{
}

void EDispatch::ScenarioDispatch()
{
  EmittableContainer::ScenarioDispatch();
  te->TestScenario()->Commands()->DispatchExecute(Id());
}

void EDispatch::SetupDispatch(const std::string& dispatchId)
{
  te->InitialContext()->Put(Id(), "dimensions", Value(MV_UINT16, geometry->Dimensions()));
  te->InitialContext()->Put(Id(), "workgroupSize[0]", Value(MV_UINT16, geometry->WorkgroupSize(0)));
  te->InitialContext()->Put(Id(), "workgroupSize[1]", Value(MV_UINT16, geometry->WorkgroupSize(1)));
  te->InitialContext()->Put(Id(), "workgroupSize[2]", Value(MV_UINT16, geometry->WorkgroupSize(2)));
  te->InitialContext()->Put(Id(), "gridSize[0]", Value(MV_UINT32, geometry->GridSize(0)));
  te->InitialContext()->Put(Id(), "gridSize[1]", Value(MV_UINT32, geometry->GridSize(1)));
  te->InitialContext()->Put(Id(), "gridSize[2]", Value(MV_UINT32, geometry->GridSize(2)));
  te->TestScenario()->Commands()->DispatchCreate(Id(), executableId.c_str(), kernelName.c_str());
  EmittableContainer::SetupDispatch(Id());
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
  case COND_REG:
    return "reg";
  default:
    assert(0); return "<invalid condition input>";
  }
}

void ECondition::Name(std::ostream& out) const
{
  out << ConditionType2Str(type) << "_" << ConditionInput2Str(input) << "_" << width2str(width) << "_" << type2str(itype);
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

void ECondition::SetupDispatch(const std::string& dispatchId)
{
  if (condBuffer) { condBuffer->SetupDispatch(dispatchId); }
}

void ECondition::ScenarioInit()
{
  if (condBuffer) { condBuffer->ScenarioInit(); }
}

void ECondition::ScenarioValidation()
{
  if (condBuffer) { condBuffer->ScenarioValidation(); }
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
  case COND_REG:
    return reg->Reg();
  default: assert(0); return Operand();
  }
}

Operand ECondition::EmitIfCond()
{
  Operand cond = CondOperand();
  if (input != ConditionInput::COND_REG || reg->Type() == BRIG_TYPE_B1) {
    return cond;
  } else {
    TypedReg cmp = te->Brig()->AddCTReg();
    te->Brig()->EmitCmp(cmp->Reg(), reg, te->Brig()->Immed(reg->Type(), 1), BRIG_COMPARE_EQ); 
    return cmp->Reg();
  }
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

void ECondition::EmitIfThenStartSand(Condition condition)
{
  lThen = te->Brig()->AddLabel();
  te->Brig()->EmitCbr(EmitIfCond(), lThen, width);
  te->Brig()->EmitBr(condition->EndLabel());
  te->Brig()->EmitLabel(lThen);
}

void ECondition::EmitIfThenStartSor()
{
  lThen = te->Brig()->AddLabel();
  te->Brig()->EmitCbr(EmitIfCond(), lThen, width);
}

void ECondition::EmitIfThenStartSor(Condition condition)
{
  lEnd = te->Brig()->AddLabel();
  te->Brig()->EmitCbr(EmitIfCond(), condition->ThenLabel(), width);
  te->Brig()->EmitBr(lEnd);
  te->Brig()->EmitLabel(condition->ThenLabel());
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
  case COND_REG: {
    if (reg->Type() == BRIG_TYPE_U32 || reg->Type() == BRIG_TYPE_U64) {
      return reg->Reg();
    } else {
      BrigType16_t cvtType = te->Brig()->LegalizeSourceType(BRIG_OPCODE_SBR, reg->Type());
      TypedReg cvt = te->Brig()->AddTReg(cvtType);
      return te->Brig()->EmitCvt(cvt, reg);
    }
  }
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
  labels.clear();
}

unsigned ECondition::SwitchBranchCount()
{
  switch (input) {
  case COND_HOST_INPUT: return 16;
  case COND_IMM_PATH0: return 2;
  case COND_IMM_PATH1: return 3;
  case COND_WAVESIZE: return te->CoreCfg()->Wavesize() + 1;
  case COND_REG: return branchCount;
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

TestEmitter::TestEmitter(Context* context_)
  : context(context_),
    be(new BrigEmitter()),
    initialContext(new Context()),
    scenario(new ScenarioBuilder(initialContext.get()))
{
}

void TestEmitter::SetCoreConfig(CoreConfig* cc)
{
  this->coreConfig = cc;
  be->SetCoreConfig(coreConfig);
}

Variable TestEmitter::NewVariable(const std::string& id, BrigSegment segment, BrigType type, Location location, BrigAlignment align, uint64_t dim, bool isConst, bool output)
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

Variable TestEmitter::NewFlexArray(const std::string& id, BrigType type, BrigAlignment align)
{
  BrigType atype = (BrigType) (type | BRIG_TYPE_ARRAY);
  return new(Ap()) EVariable(this, id, BRIG_SEGMENT_ARG, atype, FUNCTION, align, 0, false, false);
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

Image TestEmitter::NewImage(const std::string& id, ImageType type, ImageSpec spec, bool optionalFormats)
{
  return new(Ap()) EImage(this, id, type, spec, optionalFormats);
}

Sampler TestEmitter::NewSampler(const std::string& id, SamplerSpec spec) 
{
  return new(Ap()) ESampler(this, id, spec);
}

Module TestEmitter::NewModule(const std::string& id)
{
  return new(Ap()) EModule(this, id);
}

Dispatch TestEmitter::NewDispatch(const std::string& id, const std::string& executableId, const std::string& kernelName, Grid geometry)
{
  return new(Ap()) EDispatch(this, id, executableId, kernelName, geometry);
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
   executableId("executable", te->Ap()), output(0), kernel(0),
   function(0), functionResult(0), functionResultReg(0),
   dispatch(0)
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
  te->EmitContext()->Put("geometry", const_cast<GridGeometry*>(geometry));
  kernel = te->NewKernel("test_kernel");
  KernelArgumentsInit();
  if (codeLocation == emitter::FUNCTION) {
    function = te->NewFunction("test_function");
    FunctionArgumentsInit();
  }
  module = te->NewModule();
  dispatch = te->NewDispatch("dispatch", executableId.str(), kernel->Id(), geometry);
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
  CreateModule();
  StartModule();
  ModuleDirectives();
  ModuleVariables();
  Executables();
  EndModule();
}

void EmittedTest::CreateModule()
{
  module->CreateModule();
}

void EmittedTest::StartModule()
{
  module->StartModule();
}

void EmittedTest::EndModule()
{
  module->EndModule();
}

void EmittedTest::ModuleDirectives()
{
  module->ModuleDirectives();
}

void EmittedTest::ModuleVariables()
{
  module->ModuleVariables();
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
      te->Brig()->EmitCallSeq(function, kernArgInRegs, kernArgOutRegs);
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
  ScenarioProgram();
  ScenarioDispatches();
  ScenarioValidation();
  ScenarioEnd();
}

void EmittedTest::ScenarioInit()
{
  if (output) {
    output->SetData(ExpectedResults());
  }
  kernel->ScenarioInit();
  te->TestScenario()->Commands()->ProgramCreate();
}

void EmittedTest::ScenarioProgram()
{
  module->ScenarioProgram();
  te->TestScenario()->Commands()->ProgramFinalize();
  te->TestScenario()->Commands()->ExecutableCreate(executableId.c_str());
  te->TestScenario()->Commands()->ExecutableLoadCode();
  te->TestScenario()->Commands()->ExecutableFreeze();
  /*
  CommandSequence& commands0 = te->TestScenario()->Commands();
  commands0.CreateProgram();
  commands0.AddBrigModule();
  commands0.ValidateProgram();
  commands0.Finalize(Defaults::CODE_ID, Defaults::PROGRAM_ID);
  */
}

void EmittedTest::ScenarioDispatches()
{
  SetupDispatch(dispatch->Id());
  ScenarioDispatch();
}

void EmittedTest::SetupDispatch(const std::string& dispatchId)
{
  dispatch->SetupDispatch(dispatchId);
  module->SetupDispatch(dispatchId);
  kernel->SetupDispatch(dispatchId);
}

void EmittedTest::ScenarioDispatch()
{
  dispatch->ScenarioDispatch();
}

void EmittedTest::ScenarioValidation()
{
  kernel->ScenarioValidation();
  dispatch->ScenarioValidation();
}

void EmittedTest::ScenarioEnd()
{
}

void EmittedTest::Finish() {
  te->InitialContext()->Move("scenario", te->TestScenario()->ReleaseScenario());
}


}
