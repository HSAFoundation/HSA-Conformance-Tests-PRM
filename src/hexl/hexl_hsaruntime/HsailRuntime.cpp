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

#include "HexlTest.hpp"
#include "HsailRuntime.hpp"
#include "Scenario.hpp"
#include "Utils.hpp"
#include "DllApi.hpp"
#include <cstring>
#include <algorithm>
#include <sstream>
#include "time.h"

using namespace hexl;
using namespace Brig;

namespace hexl {

namespace hsail_runtime {

#define GET_FUNCTION(NAME) \
  api->NAME = GetFunction(#NAME, api->NAME); \
  if (!api->NAME) { delete api; return 0; }

const HsaApiTable* HsaApi::InitApiTable() {
  HsaApiTable* api = new HsaApiTable();
  GET_FUNCTION(hsa_init);
  GET_FUNCTION(hsa_shut_down);
  GET_FUNCTION(hsa_iterate_agents);
  GET_FUNCTION(hsa_agent_iterate_regions);
  GET_FUNCTION(hsa_region_get_info);
  GET_FUNCTION(hsa_agent_get_info);
  GET_FUNCTION(hsa_queue_create);
  GET_FUNCTION(hsa_queue_destroy);
  GET_FUNCTION(hsa_memory_allocate);
  GET_FUNCTION(hsa_memory_free);
  GET_FUNCTION(hsa_memory_register);
  GET_FUNCTION(hsa_memory_deregister);
  GET_FUNCTION(hsa_signal_create);
  GET_FUNCTION(hsa_signal_destroy);
  GET_FUNCTION(hsa_ext_program_create);
  GET_FUNCTION(hsa_ext_program_destroy);
  GET_FUNCTION(hsa_queue_load_write_index_relaxed);
  GET_FUNCTION(hsa_queue_store_write_index_relaxed);
  GET_FUNCTION(hsa_queue_add_write_index_relaxed);
  GET_FUNCTION(hsa_signal_store_relaxed);
  GET_FUNCTION(hsa_signal_store_release);
  GET_FUNCTION(hsa_signal_wait_acquire);
  GET_FUNCTION(hsa_ext_add_module);
  GET_FUNCTION(hsa_ext_finalize_program);
  GET_FUNCTION(hsa_ext_query_kernel_descriptor_address);
  return api;
}

class HsailProgram;
class HsailCode;
class HsailMemoryState;
class HsailDispatch;
class HsailRuntimeContextState;
class HsailRuntimeContext;

template <typename D, typename P> class IterateData {
public:
  IterateData(HsailRuntimeContext* runtime_, D* data_, P param_ = static_cast<P>(0))
    : runtime(runtime_), data(data_), param(param_) {
    this->data->handle = 0;
  }

  IterateData(void *data) {
    IterateData<D, P>* other = static_cast<IterateData<D, P>*>(data);
    this->runtime = other->runtime;
    this->data = other->data;
    this->param = other->param;
  }

  HsailRuntimeContext* Runtime() { return runtime; }
  D* Data() { return data; }
  bool IsSet() { return data->handle != 0; }
  P Param() { return param; }
private:
  HsailRuntimeContext* runtime;
  D* data;
  P param;
};

static hsa_status_t IterateAgentGetHsaDevice(hsa_agent_t agent, void *data);
static hsa_status_t IterateRegionsGet(hsa_region_t region, void* data);

bool RegionMatchAny(HsailRuntimeContext* runtime, hsa_region_t region) { return true; }

bool RegionMatchKernarg(HsailRuntimeContext* runtime, hsa_region_t region);

void HsaQueueErrorCallback(hsa_status_t status, hsa_queue_t *source, void *data)
{
  HsailRuntimeContext* runtime = static_cast<HsailRuntimeContext*>(data);
  runtime->HsaError("Queue error", status);
}

class HsailModule : public Module {
private:
  HsailRuntimeContextState* state;
  hsa_ext_brig_module_t* module;
  BrigCodeOffset32_t uniqueKernelOffset;

public:
  HsailModule(HsailRuntimeContextState* state_, hsa_ext_brig_module_t* module_, BrigCodeOffset32_t uniqueKernelOffset_)
    : state(state_), module(module_), uniqueKernelOffset(uniqueKernelOffset_) { }
  ~HsailModule();
  hsa_ext_brig_module_t* Module() { return module; }
  BrigCodeOffset32_t UniqueKernelOffset() const { return uniqueKernelOffset; }
};

class HsailProgram : public Program {
private:
  HsailRuntimeContextState* state;
  hsa_ext_program_handle_t handle;
  std::vector<hsa_ext_brig_module_handle_t> moduleHandles;
  BrigCodeOffset32_t uniqueKernelOffset;

public:
  HsailProgram(HsailRuntimeContextState* state_, hsa_ext_program_handle_t handle_)
    : state(state_), handle(handle_), uniqueKernelOffset(0) { }
  ~HsailProgram();
  void AddModule(Module* module);
  hsa_ext_program_handle_t Handle() { return handle; }
  BrigCodeOffset32_t UniqueKernelOffset() const { return uniqueKernelOffset; }
  hsa_ext_brig_module_handle_t ModuleHandle(unsigned i = 0) { assert(i < moduleHandles.size()); return moduleHandles[i]; }
  bool Validate(std::string& errMsg);
  HsailRuntimeContext* Runtime();
};

class HsailCode : public Code {
private:
  HsailRuntimeContextState* state;
  hsa_ext_code_descriptor_t* codeDescriptor;

public:
  HsailCode(HsailRuntimeContextState* state_, hsa_ext_code_descriptor_t* codeDescriptor_)
    : state(state_), codeDescriptor(codeDescriptor_) { }
  ~HsailCode() { }

  hsa_ext_code_descriptor_t* CodeDescriptor() { return codeDescriptor; }
};

typedef IObject<HsailMemoryState> HObject;

class HBuffer : public HObject {
private:
  MBuffer* mb;
  void *ptr;

public:
  HBuffer(HsailMemoryState* mstate_, MBuffer* mb_, void *ptr_)
    : HObject(mstate_), mb(mb_), ptr(ptr_) { }
  ~HBuffer();
  MBuffer* Mb() { return mb; }
  void *Ptr() { return ptr; }
  virtual bool Push();
  virtual bool Pull();
  virtual bool Validate();
  virtual void Print(std::ostream& out) const;
  virtual size_t Size() const;
};

class HRBuffer : public HObject {
private:
  MRBuffer* mr;
  HBuffer* hb;

public:
  HRBuffer(HsailMemoryState* mstate_, MRBuffer* mr_, HBuffer* hb_)
    : HObject(mstate_), mr(mr_), hb(hb_) { }
  virtual bool Push();
  virtual bool Pull();
  virtual bool Validate();
  virtual void Print(std::ostream& out) const;
  virtual size_t Size() const;
};

class HsailMemoryState : public MemoryStateBase<HsailMemoryState> {
private:
  HsailRuntimeContextState* state;
  size_t kernArgSize;
  size_t kernArgPos;
  void *kernArg;
  size_t dynamicGroupSize;

protected:
  bool PreAllocate();
  virtual bool PreAllocateBuffer(MBuffer* mo);
  virtual HObject* AllocateBuffer(MBuffer* mo);
  virtual HObject* AllocateRBuffer(MRBuffer* mo);
  virtual HObject* AllocateImage(MImage* mi);
  virtual HObject* AllocateRImage(MRImage* mi);

public:
  explicit HsailMemoryState(HsailRuntimeContextState* state_);
  ~HsailMemoryState();
  HsailRuntimeContext* Runtime();
  size_t GetValueSize(ValueType type);
  Value GetValue(Value v);
  void *KernArg() { return kernArg; }
  size_t GetDynamicGroupSize() { return dynamicGroupSize; }
};

class HsailDispatch : public Dispatch {
private:
  HsailRuntimeContextState* state;
  hsa_ext_code_descriptor_t* codeDescriptor;
  HsailMemoryState mstate;
  uint32_t dimensions;
  uint32_t gridSize[3];
  uint16_t workgroupSize[3];
  uint64_t timeout;

public:
  HsailDispatch(HsailRuntimeContextState* state_, hsa_ext_code_descriptor_t* codeDescriptor_)
    : state(state_), codeDescriptor(codeDescriptor_), mstate(state),
      dimensions(0), timeout(60 * CLOCKS_PER_SEC) { }

  MemoryState* MState() { return &mstate; }
  HsailRuntimeContext* Runtime() { return mstate.Runtime(); }
  void SetDimensions(uint32_t dimensions) { this->dimensions = dimensions; }
  void SetGridSize(uint32_t x, uint32_t y = 1, uint32_t z = 1) { gridSize[0] = x; gridSize[1] = y; gridSize[2] = z; }
  void SetWorkgroupSize(uint16_t x, uint16_t y = 1, uint16_t z = 1) { workgroupSize[0] = x; workgroupSize[1] = y; workgroupSize[2] = z; }
  bool Execute();
};

class HsailRuntimeContextState : public RuntimeContextState {
private:
  HsailRuntimeContext* runtime;

public:
  HsailRuntimeContextState(HsailRuntimeContext* runtime_, Context* context)
    : RuntimeContextState(context), runtime(runtime_) { }
  Program* NewProgram();
  Module* NewModuleFromBrig(brig_container_t brig);
  Module* NewModuleFromHsailText(const std::string& hsailText);
  Code* Finalize(Program* program, BrigCodeOffset32_t kernelOffset, DispatchSetup* dsetup);
  Dispatch* NewDispatch(Code* code);
  HsailRuntimeContext* Runtime() { return runtime; }
};

#define HSARUNTIMECORENAME (sizeof(void*) == 4) ? "hsa-runtime" : "hsa-runtime64"

HsailRuntimeContext::HsailRuntimeContext(Context* context)
  : RuntimeContext(context),
    hsaApi(context->Env(), context->Opts(), HSARUNTIMECORENAME),
    queue(0), queueSize(0), error(false)
{
}

RuntimeContextState* HsailRuntimeContext::NewState(Context* context)
{
  return new HsailRuntimeContextState(this, context);
}

HsailModule::~HsailModule()
{
  for (uint32_t i = 0; i < module->section_count; ++i) {
    free(module->section[i]);
  }
  free(module);
}

HsailProgram::~HsailProgram()
{
  hsa_status_t status =
    Runtime()->Hsa()->hsa_ext_program_destroy(handle);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_program_destroy failed", status); }
}

HsailRuntimeContext* HsailProgram::Runtime() {
  return state->Runtime();
}

void HsailProgram::AddModule(Module* module)
{
  HsailModule* hsailModule = static_cast<HsailModule*>(module);

  hsa_ext_brig_module_handle_t moduleHandle;
  hsa_status_t status = Runtime()->Hsa()->hsa_ext_add_module(handle, hsailModule->Module(), &moduleHandle);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_add_module failed", status); return; }
  moduleHandles.push_back(moduleHandle);
  assert(!uniqueKernelOffset);
  uniqueKernelOffset = hsailModule->UniqueKernelOffset();
}

bool HsailProgram::Validate(std::string& errMsg)
{
/*
  hsa_status_t status = hsa_ext_validate_program(handle, NULL);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_validate_program failed", status); return false; }
*/
  return true;
}

bool HsailDispatch::Execute()
{
  if (!mstate.Allocate()) { Runtime()->HsaError("Failed to allocate memory state"); return false; }
  mstate.Push();

  hsa_queue_t* queue = Runtime()->Queue();
  uint64_t packetId = Runtime()->Hsa()->hsa_queue_add_write_index_relaxed(queue, 1);
  // Prepare dispatch packet.
  hsa_kernel_dispatch_packet_s* dispatch = (hsa_kernel_dispatch_packet_s*) queue->base_address + packetId;
  memset(dispatch, 0, sizeof(*dispatch));
  dispatch->setup = (dimensions << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS);
  dispatch->workgroup_size_x = workgroupSize[0];
  dispatch->workgroup_size_y = workgroupSize[1];
  dispatch->workgroup_size_z = workgroupSize[2];
  dispatch->grid_size_x = gridSize[0];
  dispatch->grid_size_y = gridSize[1];
  dispatch->grid_size_z = gridSize[2];
  dispatch->kernel_object = codeDescriptor->code.handle;
  dispatch->kernarg_address = mstate.KernArg();
  dispatch->private_segment_size = codeDescriptor->workitem_private_segment_byte_size;
  dispatch->group_segment_size = codeDescriptor->workgroup_group_segment_byte_size + (uint32_t)mstate.GetDynamicGroupSize();
  // Create a signal with an initial value of one to monitor the task completion.
  hsa_signal_t signal;
  Runtime()->Hsa()->hsa_signal_create(1, 0, 0, &signal);
  dispatch->completion_signal = signal;
  Runtime()->GetContext()->Put("packetcompletionsig", Value(MV_UINT64, signal.handle));
  
  // Notify.
  dispatch->header =
    (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE) |
    (1 << HSA_PACKET_HEADER_BARRIER) |
    (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
    (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE);
  Runtime()->Hsa()->hsa_signal_store_release(queue->doorbell_signal, packetId);

  // Wait for kernel completion.
  hsa_signal_value_t result;
  clock_t beg = clock();
  do {
    result = Runtime()->Hsa()->hsa_signal_wait_acquire(signal, HSA_SIGNAL_CONDITION_EQ, 0, timeout, HSA_WAIT_STATE_ACTIVE);
    clock_t clocks = clock() - beg;
    if (clocks > (clock_t) timeout && result != 0) {
      Runtime()->Env()->Error("Kernel execution timed out, elapsed time: %ld clocks (clocks per second %ld)", (long) clocks, (long) CLOCKS_PER_SEC);
      Runtime()->Hsa()->hsa_signal_destroy(signal);
      return false;
    }
  } while (result!= 0);
  // Destroy completion signal
  Runtime()->Hsa()->hsa_signal_destroy(signal);
  return !Runtime()->IsError();
}

HBuffer::~HBuffer()
{
  if (mb->MType() != MEM_KERNARG) {
    if (ptr) {
      State()->Runtime()->Hsa()->hsa_memory_deregister(ptr);
      free(ptr);
    }
/*
    hsa_status_t status = State()->Runtime()->Hsa()->hsa_memory_free(ptr);
    if (status != HSA_STATUS_SUCCESS) { State()->Runtime()->HsaError("hsa_memory_free failed", status); }
*/
  }
}

bool HBuffer::Push()
{
  assert(mb && state);
  char *p = (char *) ptr;
  for (size_t i = 0; i < mb->Data().size(); ++i) {
    Value value = state->GetValue(mb->Data()[i]);
    value.WriteTo(p);
    p += value.Size();
  }
  return true;
}

bool HBuffer::Pull()
{
  // Do nothing.
  return true;
}

bool HBuffer::Validate()
{
  // Do nothing.
  return true;
}

void HBuffer::Print(std::ostream& out) const
{
  out << "buffer at " << ptr << ", size " << Size();
  if (ptr && mb->Data().size() == 1) {
    Value v = state->GetValue(mb->Data()[0]);
    out << ", value " << v;
  }
}

size_t HBufferSize(MBuffer* mb, Context* context)
{
  switch (mb->VType()) {
  case MV_REF:
    return mb->Count() * sizeof(void *);
  default:
    return mb->Size(context);
  }
}

size_t HBuffer::Size() const
{
  return HBufferSize(mb, state->GetContext());
}

bool HRBuffer::Push()
{
  // Do nothing.
  return true;
}

bool HRBuffer::Pull()
{
  // Do nothing for now.
  return true;
}

bool HRBuffer::Validate()
{
  Values actual;
  MBuffer* mb = hb->Mb();
  assert(mb);
  ReadFrom(hb->Ptr(), mr->VType(), mr->Data().size(), actual);
  state->GetContext()->Info() << "Validating: "; hb->Print(state->GetContext()->Info());
  state->GetContext()->Info() << ", " << mr->GetComparison() << std::endl;
  return state->ValidateRBuffer(mb, mr, actual, state->Runtime()->Opts());
}

void HRBuffer::Print(std::ostream& out) const
{
  MBuffer* mb = hb->Mb();
  out << "buffer check at " << hb->Ptr() << ", size " << Size();
  if (mb->Count() == 1) {
    Value v = state->GetValue(mr->Data()[0]);
    out << ", expected value " << v;
  }
}

size_t HRBuffer::Size() const
{
  return hb->Size();
}

HsailMemoryState::HsailMemoryState(HsailRuntimeContextState* state_)
    : MemoryStateBase(state_->GetContext()),
      state(state_), kernArgSize(0), kernArgPos(0), kernArg(0), dynamicGroupSize(0)
{
}

HsailMemoryState::~HsailMemoryState()
{
  if (kernArg) {
    hsa_status_t status;
    status = Runtime()->Hsa()->hsa_memory_deregister(kernArg);
    if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_deregister failed", status); }
    free(kernArg);
  }
}

size_t HsailMemoryState::GetValueSize(ValueType type) {
  switch (type) {
  case MV_REF:
    return sizeof(void *);
  case MV_IMAGEREF:
    return 8;
  default:
    return ValueTypeSize(type);
  }
}

Value HsailMemoryState::GetValue(Value v)
{
  switch (v.Type()) {
  case MV_REF: {
    HBuffer* ho = Get<HBuffer>(v.U32());
    assert(ho);
    return Value(MV_POINTER, P(ho->Ptr()));
  }
  case MV_IMAGEREF: {
    assert(false);
    return Value(MV_UINT64, (uint64_t) 0);
  }
  case MV_EXPR: {
    return GetContext()->GetValue(v.S());
  }

  default: return v;
  }
}

bool HsailMemoryState::PreAllocate()
{
  kernArgSize = 0;
  kernArgPos = 0;
  if (!MemoryStateBase<HsailMemoryState>::PreAllocate()) { return false; }
  if (kernArgSize > 0) {
    hsa_status_t status;
/*  TODO: fix when regions work properly with kernarg
    status = Runtime()->Hsa()->hsa_memory_allocate(Runtime()->GetRegion(RegionMatchKernarg), kernArgSize, &kernArg);
    if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_allocate failed", status); return false; }
*/
    kernArgSize = (std::max)(kernArgSize, (size_t) 4096);
    kernArg = malloc(kernArgSize);
    status = Runtime()->Hsa()->hsa_memory_register(kernArg, kernArgSize);
    if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_register failed", status); free(kernArg); return false; }
  }
  return true;

}

bool HsailMemoryState::PreAllocateBuffer(MBuffer* mb)
{
  assert(mb);
  if (mb->MType() == MEM_KERNARG) {
    kernArgSize += HBufferSize(mb, GetContext());
  } else if (mb->MType() == MEM_GROUP) {
    dynamicGroupSize += HBufferSize(mb, GetContext());
  }
  return true;
}

HObject* HsailMemoryState::AllocateBuffer(MBuffer* mb)
{
  hsa_status_t status;
  size_t size = HBufferSize(mb, GetContext());
  void *ptr;
  if (mb->MType() == MEM_KERNARG) {
    kernArgPos = (kernArgPos + size - 1) & ~(size - 1); // Align
    assert(kernArgPos < kernArgSize);
    ptr = (char *) kernArg + kernArgPos;
    kernArgPos += size;
    return new HBuffer(this, mb, ptr);
  } else {
    ptr = malloc(size);
    status = Runtime()->Hsa()->hsa_memory_register(ptr, size);
    if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_register failed", status); free(ptr); return 0; }
/*
    status = Runtime()->Hsa()->hsa_memory_allocate(Runtime()->GetRegion(), mb->Size(), &ptr);
    if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_allocate failed", status); return 0; }
*/
    return new HBuffer(this, mb, ptr);
  }
}

HObject* HsailMemoryState::AllocateRBuffer(MRBuffer* mr)
{
  assert(mr);
  HBuffer* hb = Get<HBuffer>(mr->RefId());
  return new HRBuffer(this, mr, hb);
}

HObject* HsailMemoryState::AllocateImage(MImage* mr)
{
  assert(false);
  return 0;
}

HObject* HsailMemoryState::AllocateRImage(MRImage* mr)
{
  assert(false);
  return 0;
}

HsailRuntimeContext* HsailMemoryState::Runtime()
{
  return state->Runtime();
}

Program* HsailRuntimeContextState::NewProgram()
{
  hsa_ext_program_handle_t handle;
  hsa_ext_brig_machine_model8_t machineModel =
    sizeof(void *) == 4 ? HSA_EXT_BRIG_MACHINE_SMALL : HSA_EXT_BRIG_MACHINE_LARGE;
  hsa_ext_brig_profile_t profile = HSA_EXT_BRIG_PROFILE_FULL;
  hsa_status_t status =
    Runtime()->Hsa()->hsa_ext_program_create(runtime->Agents(), runtime->AgentCount(),
      machineModel, profile, &handle);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_program_create failed", status); return 0; }
  return new HsailProgram(this, handle);
}


Module* HsailRuntimeContextState::NewModuleFromBrig(brig_container_t brig)
{
  // Temporarily validate BRIG here until hsa_ext_validate_program is implemented in Runtime.
  int status = brig_container_validate(brig);
  if (status != 0) {
    Runtime()->hsailcError("brig_container_validate failed", brig, status);
    return 0;
  }

  hsa_ext_brig_module_t* brig_module;
  uint32_t number_of_sections = brig_container_get_section_count(brig);
  brig_module = (hsa_ext_brig_module_t*) (malloc (sizeof(hsa_ext_brig_module_t) + sizeof(void*)*number_of_sections));
  brig_module->section_count = number_of_sections;
  for (uint32_t i = 0; i < number_of_sections; ++i) {
    uint64_t size_section_bytes = brig_container_get_section_size(brig, i);
    void* section_copy = malloc(size_section_bytes);
    memcpy((char*)section_copy,
      brig_container_get_section_bytes(brig, i),
      size_section_bytes);
    brig_module->section[i] = (hsa_ext_brig_section_header_t*) section_copy;
  }
  return new HsailModule(this, brig_module, GetBrigUniqueKernelOffset(brig));
}

Module* HsailRuntimeContextState::NewModuleFromHsailText(const std::string& hsailText)
{
  brig_container_t brig = brig_container_create_empty();
  int status = brig_container_assemble_from_memory(brig, hsailText.c_str(), hsailText.size());
  if (status != 0) { Runtime()->hsailcError("brig_container_assemble_from_memory failed", brig, status); return 0; }
  return NewModuleFromBrig(brig);
}

Code* HsailRuntimeContextState::Finalize(Program* program, BrigCodeOffset32_t kernelOffset, DispatchSetup* dsetup)
{
  HsailProgram* hsailProgram = static_cast<HsailProgram*>(program);
  if (kernelOffset == 0) { kernelOffset = hsailProgram->UniqueKernelOffset(); }
  hsa_ext_finalization_request_t request;
  request.module = hsailProgram->ModuleHandle();
  request.symbol = kernelOffset;
  request.program_call_convention = 0;
  hsa_status_t status =
    Runtime()->Hsa()->hsa_ext_finalize_program(hsailProgram->Handle(),
      Runtime()->Agent(), 1,
      &request, NULL, NULL, 0, "", 0);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_finalize_program failed", status); return 0; }
  hsa_ext_code_descriptor_t* handle;
  status =
    Runtime()->Hsa()->hsa_ext_query_kernel_descriptor_address(hsailProgram->Handle(),
       hsailProgram->ModuleHandle(), kernelOffset, &handle);
  context->Put("kerneldesc", Value(context->IsLarge() ? MV_UINT64 : MV_UINT32, (uintptr_t) handle));
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_query_kernel_descriptor_address failed", status); return 0; }
  return new HsailCode(this, handle);
}

Dispatch* HsailRuntimeContextState::NewDispatch(Code* code)
{
  HsailCode* hsailCode = static_cast<HsailCode*>(code);
  return new HsailDispatch(this, hsailCode->CodeDescriptor());
}

static hsa_status_t IterateAgentGetHsaDevice(hsa_agent_t agent, void *data) {
  assert(data);
  IterateData<hsa_agent_t, int> idata(data);
  hsa_status_t status;
  if (!idata.IsSet()) {
    uint32_t features;
    hsa_device_type_t device_type;
    status = idata.Runtime()->Hsa()->hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    if (status != HSA_STATUS_SUCCESS) { return status; }
    status = idata.Runtime()->Hsa()->hsa_agent_get_info(agent, HSA_AGENT_INFO_FEATURE, &features);
    if (status != HSA_STATUS_SUCCESS) { return status; }
    if (device_type == HSA_DEVICE_TYPE_GPU && (features & HSA_AGENT_FEATURE_KERNEL_DISPATCH)) {
      *(idata.Data()) = agent;
      return HSA_STATUS_SUCCESS;
    }
  }
  return HSA_STATUS_SUCCESS;
}

static hsa_status_t IterateRegionsGet(hsa_region_t region, void* data) {
  IterateData<hsa_region_t, RegionMatch> idata(data);
  RegionMatch match = idata.Param();
  if (!idata.IsSet()) {
    if (!match || match(idata.Runtime(), region)) { 
      *(idata.Data()) = region;
    }
  }
  return HSA_STATUS_SUCCESS;
}

bool HsailRuntimeContext::Init() {
  if (!hsaApi.Init()) { return false; }
  hsa_status_t status;
  status = Hsa()->hsa_init();
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_init failed", status); return false; }
  IterateData<hsa_agent_t, int> idata(this, &agent);
  status = Hsa()->hsa_iterate_agents(IterateAgentGetHsaDevice, &idata);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_iterate_agents failed", status); return false; }
  if (!agent.handle) { HsaError("Failed to find agent"); return false; }
  status = Hsa()->hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queueSize);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_agent_get_info failed", status); return false; }
  status = Hsa()->hsa_queue_create(agent, queueSize, HSA_QUEUE_TYPE_SINGLE, HsaQueueErrorCallback, this, UINT32_MAX, UINT32_MAX, &queue);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_queue_create failed", status); return false; }
  context->Put("queueid", Value(MV_UINT32, Queue()->id));
  context->Put("queueptr", Value(context->IsLarge() ? MV_UINT64 : MV_UINT32, (uintptr_t) Queue()));
  return true;
}

void HsailRuntimeContext::Dispose()
{
  if (context) {
    hsa_status_t status;
    status = Hsa()->hsa_queue_destroy(queue);
    if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_queue_destroy failed", status); }
    Hsa()->hsa_shut_down();
    context = 0;
  }
}

hsa_region_t HsailRuntimeContext::GetRegion(RegionMatch match)
{
  hsa_region_t region;
  region.handle = 0;
  IterateData<hsa_region_t, RegionMatch> idata(this, &region, match);
  hsa_status_t status = Hsa()->hsa_agent_iterate_regions(Agent(), IterateRegionsGet, &idata);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_agent_iterate_regions failed", status); return region; }
  return region;
}

bool RegionMatchKernarg(HsailRuntimeContext* runtime, hsa_region_t region)
{
  hsa_status_t status;
  bool allocAllowed;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_RUNTIME_ALLOC_ALLOWED, &allocAllowed);
  if (status != HSA_STATUS_SUCCESS) { return false; }
  if (!allocAllowed) { return false; }
  size_t granule;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_RUNTIME_ALLOC_GRANULE, &granule);
  if (status != HSA_STATUS_SUCCESS) { return false; }
  if (granule > sizeof(size_t)) { return false; }
  size_t maxSize;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_ALLOC_MAX_SIZE, &maxSize);
  if (status != HSA_STATUS_SUCCESS) { return false; }
  if (maxSize < 256) { return false; }
  return true;
}

HsailRuntimeContext* HsailRuntimeFromContext(RuntimeContext* runtime)
{
  assert(runtime->Name() == "hsa");
  return static_cast<HsailRuntimeContext*>(runtime);
}

const HsaApi& HsaApiFromContext(RuntimeContext* runtime)
{
  return HsailRuntimeFromContext(runtime)->Hsa();
}

}

RuntimeContext* CreateHsailRuntimeContext(Context* context)
{
  return new hsail_runtime::HsailRuntimeContext(context);
}

namespace scenario {

using namespace hsail_runtime;

class CreateSignalCommand : public Command {
private:
  std::string signalId;
  uint64_t signalInitialValue;
  hsa_signal_t signal;

public:
  CreateSignalCommand(const std::string& signalId_, uint64_t signalInitialValue_)
    : signalId(signalId_), signalInitialValue(signalInitialValue_) { }
  virtual ~CreateSignalCommand() {
    HsailRuntimeContext* hsailRTContext = HsailRuntimeFromContext(context->Runtime());
    if (hsailRTContext)
      hsailRTContext->Hsa()->hsa_signal_destroy(signal);
  }
  bool Finish() { return true; }

  virtual bool Run() {
    bool result = true;
    hsa_status_t status;
    HsailRuntimeContext* hsailRTContext = HsailRuntimeFromContext(context->Runtime());
    status = hsailRTContext->Hsa()->hsa_signal_create(signalInitialValue, hsailRTContext->AgentCount(), hsailRTContext->Agents(), &signal);
    if (status != HSA_STATUS_SUCCESS) { hsailRTContext->HsaError("Failed to create signal", status); result = false; }
    context->Put(signalId, Value(MV_UINT64, U64(signal.handle)));
    std::stringstream ss;
    ss << "Signal '" << signalId << "' handle: " << std::hex << signal.handle << std::dec
       << ", initial value: " << signalInitialValue << std::endl;
    context->Info() << ss.str();
    return result;
  }

  void Print(std::ostream& out) const {
    out << "create_signal " << signalId << " " << signalInitialValue;
  }
};

class SendSignalCommand : public Command {
private:
  std::string signalId;
  uint64_t signalSendValue;
  hsa_signal_t signal;

public:
  SendSignalCommand(const std::string& signalId_, uint64_t signalSendValue_)
    : signalId(signalId_), signalSendValue(signalSendValue_) { }
  bool Finish() { return true; }

  virtual bool Run() {
    HsailRuntimeContext* hsailRTContext = HsailRuntimeFromContext(context->Runtime());
    signal.handle = context->GetValue(signalId).U64();
    hsailRTContext->Hsa()->hsa_signal_store_relaxed(signal, (hsa_signal_value_t) signalSendValue);
    std::stringstream ss;
    ss << "Signal '" << signalId << "' handle: " << std::hex << signal.handle << std::dec
       << ", stored value: " << signalSendValue << std::endl;
    context->Info() << ss.str();
    return true;
  }

  void Print(std::ostream& out) const {
    out << "send_signal " << signalId << " " << signalSendValue;
  }
};

class WaitSignalCommand : public Command {
private:
  std::string signalId;
  hsa_signal_value_t expectedValue;
  hsa_signal_value_t acquiredValue;
  hsa_signal_t signal;
  uint64_t timeout;

public:
  WaitSignalCommand(const std::string& signalId_, hsa_signal_value_t expectedValue_)
    : signalId(signalId_), expectedValue(expectedValue_), timeout(60 * CLOCKS_PER_SEC) { }

  bool Finish() { return true; }

  virtual bool Run() {
    const HsaApi& hsa = HsailRuntimeFromContext(context->Runtime())->Hsa();
    signal.handle = context->GetValue(signalId).U64();
    bool result = true;
    clock_t beg = clock();
    do {
      acquiredValue = hsa->hsa_signal_wait_acquire(signal, HSA_SIGNAL_CONDITION_EQ, expectedValue, timeout, HSA_WAIT_STATE_ACTIVE);
      clock_t clocks = clock() - beg;
      if (clocks > (clock_t) timeout && acquiredValue != expectedValue) {
        context->Info() << "Signal '" << signalId << "' wait timed out, elapsed time: " << (uint64_t) clocks << " clocks (clocks per second " << (uint64_t) CLOCKS_PER_SEC << ")" << std::endl;
        result = false;
        break;
      }
    } while (expectedValue != acquiredValue);
    context->Info() << "Signal '" << signalId << "' handle: " << std::hex << signal.handle << std::dec
                    << ", expected value: " << expectedValue << ", acquired value: " << acquiredValue << std::endl;
    return result;
  }

  void Print(std::ostream& out) const {
    out << "wait_signal " << signalId << " " << expectedValue;
  }
};

class CreateQueueCommand : public Command {
private:
  std::string queueId;
  uint32_t size;
  hsa_queue_t* queue;

public:
  CreateQueueCommand(const std::string& queueId_, uint32_t size_)
    : queueId(queueId_), size(size_), queue(0)  { }

  bool Finish() {
    if (queue) {
      HsailRuntimeContext* runtime = HsailRuntimeFromContext(context->Runtime());
      const HsaApi& hsa = runtime->Hsa();
      hsa_status_t status = hsa->hsa_queue_destroy(queue);
      if (status != HSA_STATUS_SUCCESS) {
        runtime->HsaError("hsa_queue_destroy failed", status);
        return false;
      }
    }
    return true;
  }

  virtual bool Run() {
    HsailRuntimeContext* runtime = HsailRuntimeFromContext(context->Runtime());
    const HsaApi& hsa = runtime->Hsa();
    hsa_status_t status;
    if (size == 0) {
      status = hsa->hsa_agent_get_info(runtime->Agent(), HSA_AGENT_INFO_QUEUE_MAX_SIZE, &size);
      if (status != HSA_STATUS_SUCCESS) {
        runtime->HsaError("hsa_agent_get_info failed", status);
        return false;
      }
    }
    status = hsa->hsa_queue_create(runtime->Agent(), size, HSA_QUEUE_TYPE_MULTI, HsaQueueErrorCallback, runtime, UINT32_MAX, UINT32_MAX, &queue);
    if (status != HSA_STATUS_SUCCESS) {
      runtime->HsaError("hsa_queue_create failed", status);
      return false;
    }
    context->Put(queueId, queue);
    context->Info() << "Created queue " << queueId << ": " << std::hex << (uint64_t) queue << std::dec << std::endl;
    return true;
  }

  void Print(std::ostream& out) const {
    out << "create_queue " << queueId << " " << size;
  }
};

void CommandSequence::CreateSignal(const std::string& signalId, uint64_t signalInitialValue)
{
  Add(new CreateSignalCommand(signalId, signalInitialValue));
}

void CommandSequence::SendSignal(const std::string& signalId, uint64_t signalSendValue)
{
  Add(new SendSignalCommand(signalId, signalSendValue));
}

void CommandSequence::WaitSignal(const std::string& signalId, uint64_t signalExpectedValue)
{
  Add(new WaitSignalCommand(signalId, signalExpectedValue));
}

void CommandSequence::CreateQueue(const std::string& queueId, uint32_t size)
{
  Add(new CreateQueueCommand(queueId, size));
}

}

}
