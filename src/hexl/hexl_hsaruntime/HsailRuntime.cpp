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

namespace hexl {

namespace hsail_runtime {

#define GET_FUNCTION(NAME) \
  api->NAME = GetFunction(#NAME, api->NAME); \
  if (!api->NAME) { delete api; return 0; }

const HsaApiTable* HsaApi::InitApiTable() {
  HsaApiTable* api = new HsaApiTable();
  GET_FUNCTION(hsa_status_string);
  GET_FUNCTION(hsa_init);
  GET_FUNCTION(hsa_shut_down);
  GET_FUNCTION(hsa_iterate_agents);
  GET_FUNCTION(hsa_agent_iterate_regions);
  GET_FUNCTION(hsa_system_get_info);
  GET_FUNCTION(hsa_region_get_info);
  GET_FUNCTION(hsa_agent_get_info);
  GET_FUNCTION(hsa_agent_get_exception_policies);
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
  GET_FUNCTION(hsa_ext_program_add_module);
  GET_FUNCTION(hsa_ext_program_finalize);
  GET_FUNCTION(hsa_ext_program_get_info);
  
  GET_FUNCTION(hsa_executable_create);
  GET_FUNCTION(hsa_code_object_destroy);
  GET_FUNCTION(hsa_executable_load_code_object);
  GET_FUNCTION(hsa_executable_symbol_get_info);
  GET_FUNCTION(hsa_executable_get_symbol);
  GET_FUNCTION(hsa_executable_iterate_symbols);
  GET_FUNCTION(hsa_executable_freeze);
  GET_FUNCTION(hsa_executable_destroy);

//  GET_FUNCTION();
  GET_FUNCTION(hsa_queue_load_write_index_relaxed);
  GET_FUNCTION(hsa_queue_store_write_index_relaxed);
  GET_FUNCTION(hsa_queue_add_write_index_relaxed);
  GET_FUNCTION(hsa_signal_store_relaxed);
  GET_FUNCTION(hsa_signal_store_release);
  GET_FUNCTION(hsa_signal_wait_acquire);
  GET_FUNCTION(hsa_ext_image_create);
  GET_FUNCTION(hsa_ext_image_destroy);
  GET_FUNCTION(hsa_ext_sampler_create);
  GET_FUNCTION(hsa_ext_sampler_destroy);
  GET_FUNCTION(hsa_ext_image_data_get_info);
  GET_FUNCTION(hsa_ext_image_import);
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
  void Set(D data) { this->data->handle = data.handle; }
  P Param() { return param; }
private:
  HsailRuntimeContext* runtime;
  D* data;
  P param;
};

static hsa_status_t IterateAgentGetHsaDevice(hsa_agent_t agent, void *data);
static hsa_status_t IterateRegionsGet(hsa_region_t region, void* data);
static hsa_status_t IterateExecutableSymbolsGetKernel(hsa_executable_t executable, hsa_executable_symbol_t symbol, void* data);

bool RegionMatchAny(HsailRuntimeContext* runtime, hsa_region_t region) { return true; }

bool RegionMatchKernarg(HsailRuntimeContext* runtime, hsa_region_t region);

void HsaQueueErrorCallback(hsa_status_t status, hsa_queue_t *source, void *data)
{
  HsailRuntimeContext* runtime = static_cast<HsailRuntimeContext*>(data);
  runtime->HsaError("Queue error", status);
  runtime->SetError();
}

class HsailModule : public Module {
private:
  HsailRuntimeContextState* state;
  brig_container_t brig;
  BrigCodeOffset32_t uniqueKernelOffset;

public:
  HsailModule(HsailRuntimeContextState* state_, brig_container_t brig_, BrigCodeOffset32_t uniqueKernelOffset_)
    : state(state_), brig(brig_), uniqueKernelOffset(uniqueKernelOffset_) { }
  ~HsailModule();
  hsa_ext_module_t Module();
  BrigCodeOffset32_t UniqueKernelOffset() const { return uniqueKernelOffset; }
};

class HsailProgram : public Program {
private:
  HsailRuntimeContextState* state;
  hsa_ext_program_t handle;
  BrigCodeOffset32_t uniqueKernelOffset;

public:
  HsailProgram(HsailRuntimeContextState* state_, hsa_ext_program_t handle_)
    : state(state_), handle(handle_), uniqueKernelOffset(0) { }
  ~HsailProgram();
  void AddModule(Module* module);
  hsa_ext_program_t Handle() { return handle; }
  BrigCodeOffset32_t UniqueKernelOffset() const { return uniqueKernelOffset; }
  bool Validate(std::string& errMsg);
  HsailRuntimeContext* Runtime();
};

class HsailCode : public Code {
private:
  HsailRuntimeContextState* state;
  hsa_code_object_t codeObject;

public:
  HsailCode(HsailRuntimeContextState* state_, hsa_code_object_t codeObject_)
    : state(state_), codeObject(codeObject_) { }
  ~HsailCode();

  hsa_code_object_t CodeObject() { return codeObject; }
  HsailRuntimeContext* Runtime();
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

class HImage : public HObject {
private:
  MImage* mi;
  void *ptr;
  hsa_ext_image_t imgh;
public:
  HImage(HsailMemoryState* mstate_, MImage* mi_, void *ptr_, hsa_ext_image_t imgh_)
    : HObject(mstate_), mi(mi_), ptr(ptr_), imgh(imgh_) { }
  ~HImage();
  MImage* Mi() { return mi; }
  void *Ptr() { return ptr; }
  uint64_t Handle() { return imgh.handle; }
  virtual bool Push();
  virtual bool Pull();
  virtual bool Validate();
  virtual void Print(std::ostream& out) const;
  virtual size_t Size() const;
};

class HRImage : public HObject {
private:
  MRImage* mr;
  HImage* hi;
public:
  HRImage(HsailMemoryState* mstate_, MRImage* mr_, HImage* hi_)
    : HObject(mstate_), mr(mr_), hi(hi_) { }
  virtual bool Push();
  virtual bool Pull();
  virtual bool Validate();
  virtual void Print(std::ostream& out) const;
  virtual size_t Size() const;
};

class HSampler : public HObject {
private:
  MSampler* ms;
  hsa_ext_sampler_t smph;
public:
  HSampler(HsailMemoryState* mstate_, MSampler* ms_, hsa_ext_sampler_t smph_)
    : HObject(mstate_), ms(ms_), smph(smph_) { }
  ~HSampler();
  MSampler* Ms() { return ms; }
  uint64_t Handle() { return smph.handle; }
  virtual bool Push();
  virtual bool Pull();
  virtual bool Validate();
  virtual void Print(std::ostream& out) const;
  virtual size_t Size() const;
};

class HRSampler : public HObject {
private:
  MRSampler* mr;
  HSampler* hs;
public:
  HRSampler(HsailMemoryState* mstate_, MRSampler* mr_, HSampler* hs_)
    : HObject(mstate_), mr(mr_), hs(hs_) { }
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
  virtual HObject* AllocateSampler(MSampler* mi);
  virtual HObject* AllocateRSampler(MRSampler* mi);

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
  hsa_executable_t executable;
  hsa_executable_symbol_t kernel;
  HsailMemoryState mstate;
  uint32_t dimensions;
  uint32_t gridSize[3];
  uint16_t workgroupSize[3];
  uint64_t timeout;

public:
  HsailDispatch(HsailRuntimeContextState* state_, hsa_executable_t executable_, hsa_executable_symbol_t kernel_)
    : state(state_), executable(executable_), kernel(kernel_),
      mstate(state),
      dimensions(0), timeout(60 * CLOCKS_PER_SEC) { }
  ~HsailDispatch();

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
    hsaApi(context, context->Opts(), HSARUNTIMECORENAME),
    queue(0), queueSize(0), error(false),
    profile(HSA_PROFILE_BASE), wavesize(64), wavesPerGroup(4), endianness(HSA_ENDIANNESS_LITTLE),
    isBreakSupported(false), isDetectSupported(false)
{
}

RuntimeContextState* HsailRuntimeContext::NewState(Context* context)
{
  return new HsailRuntimeContextState(this, context);
}


HsailModule::~HsailModule()
{
  brig_container_destroy(brig);
}

hsa_ext_module_t HsailModule::Module()
{
  hsa_ext_module_t module;
  module = (BrigModule_t) brig_container_get_brig_module(brig);
  return module;
}

HsailProgram::~HsailProgram()
{
  hsa_status_t status =
    Runtime()->Hsa()->hsa_ext_program_destroy(handle);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_program_destroy failed", status); }
}

HsailCode::~HsailCode()
{
  hsa_status_t status =
    Runtime()->Hsa()->hsa_code_object_destroy(codeObject);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_code_object_destroy failed", status); }
}

HsailRuntimeContext* HsailCode::Runtime()
{
  return state->Runtime();
}

HsailRuntimeContext* HsailProgram::Runtime()
{
  return state->Runtime();
}

void HsailProgram::AddModule(Module* module)
{
  HsailModule* hsailModule = static_cast<HsailModule*>(module);

  hsa_status_t status = Runtime()->Hsa()->hsa_ext_program_add_module(handle, hsailModule->Module());
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_add_module failed", status); return; }
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

HsailDispatch::~HsailDispatch()
{
  hsa_status_t status = Runtime()->Hsa()->hsa_executable_destroy(executable);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_destroy failed", status); }
}

bool HsailDispatch::Execute()
{
  if (!mstate.Allocate()) { Runtime()->HsaError("Failed to allocate memory state"); return false; }
  mstate.Push();

  uint64_t kernelObject;
  hsa_status_t status = Runtime()->Hsa()->hsa_executable_symbol_get_info(
    kernel, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &kernelObject);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT) failed", status); return false; }
  uint32_t privateSize, groupSize;
  status = Runtime()->Hsa()->hsa_executable_symbol_get_info(
    kernel, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE, &privateSize);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE) failed", status); return false; }
  status = Runtime()->Hsa()->hsa_executable_symbol_get_info(
    kernel, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &groupSize);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE) failed", status); return false; }

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
  dispatch->kernel_object = kernelObject;
  dispatch->kernarg_address = mstate.KernArg();
  dispatch->private_segment_size = privateSize;
  dispatch->group_segment_size = groupSize + (uint32_t) mstate.GetDynamicGroupSize();
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
      Runtime()->GetContext()->Error() << "Kernel execution timed out, elapsed time: " << (long) clocks << " clocks (clocks per second " << (long) CLOCKS_PER_SEC << ")" << std::endl;
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


HImage::~HImage()
{
    if (ptr) {
        State()->Runtime()->Hsa()->hsa_ext_image_destroy(State()->Runtime()->Agent(), imgh);
        alignedFree(ptr);
/*
        State()->Runtime()->Hsa()->hsa_memory_free(ptr);
*/
    }
}

bool HImage::Push()
{
  assert(mi && state && ptr);
  char *p = (char *) ptr;
  Value value = Value(MV_UINT8, 0);
  if (mi->ContentData().size() > 0) {
    value = state->GetValue(mi->ContentData()[0]);
  }
  if (mi->IsLimitTest())
  {
    hsa_ext_image_region_t img_region;

    img_region.offset.x = (uint32_t)mi->Width() - 1;
    img_region.offset.y = (uint32_t)mi->Height() - 1;
    img_region.offset.z = (uint32_t)mi->Depth() - 1;

    img_region.range.x = 1;
    img_region.range.y = 1;
    img_region.range.z = 1;

    char* pBuff = new char[value.Size()];
    value.WriteTo(pBuff);
    hsa_status_t status = State()->Runtime()->Hsa()->hsa_ext_image_import(State()->Runtime()->Agent(), pBuff, value.Size(), 1, imgh, &img_region);
    delete[] pBuff;
    if (status != HSA_STATUS_SUCCESS) { State()->Runtime()->HsaError("hsa_ext_image_import failed", status); return false; }
  }
  else
  {
    size_t size_val = value.Size();
    for (size_t i = 0; i < mi->Size()/size_val; ++i) {
      value.WriteTo(p);
      p += size_val;
    }
  }
  return true;
}

bool HImage::Pull()
{
  // Do nothing.
  return true;
}

bool HImage::Validate()
{
  // Do nothing.
  return true;
}

void HImage::Print(std::ostream& out) const
{
  // Do nothing.
  return;
}

size_t HImage::Size() const
{
  return mi->Size();
}

bool HRImage::Push()
{
  // Do nothing.
  return true;
}

bool HRImage::Pull()
{
  // Do nothing for now.
  return true;
}

bool HRImage::Validate()
{
  Values actual;
  MImage* mi = hi->Mi();
  assert(mi);
  ReadFrom(hi->Ptr(), mr->VType(), mr->Data().size(), actual);
  state->GetContext()->Info() << "Validating: "; hi->Print(state->GetContext()->Info());
  state->GetContext()->Info() << ", " << mr->GetComparison() << std::endl;
  return state->ValidateRImage(mi, mr, actual, state->Runtime()->Opts());
}

void HRImage::Print(std::ostream& out) const
{
  MImage* mi = hi->Mi();
  out << "buffer check at " << hi->Ptr() << ", size " << Size();
  if (mi->Size() == 1) {
    Value v = state->GetValue(mr->Data()[0]);
    out << ", expected value " << v;
  }
  return;
}

size_t HRImage::Size() const
{
  return hi->Size();
}

HSampler::~HSampler()
{
    State()->Runtime()->Hsa()->hsa_ext_sampler_destroy(State()->Runtime()->Agent(), smph);
}

bool HSampler::Push()
{
    // Do nothing.
   return true;
}

bool HSampler::Pull()
{
  // Do nothing.
  return true;
}

bool HSampler::Validate()
{
  // Do nothing.
  return true;
}

void HSampler::Print(std::ostream& out) const
{
  // Do nothing.
  return;
}

size_t HSampler::Size() const
{
  return ms->Size();
}


bool HRSampler::Push()
{
  // Do nothing.
  return true;
}

bool HRSampler::Pull()
{
  // Do nothing for now.
  return true;
}

bool HRSampler::Validate()
{
  // Do nothing.
  return true;
}

void HRSampler::Print(std::ostream& out) const
{
  // Do nothing.
  return;
}

size_t HRSampler::Size() const
{
  return hs->Size();
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
  case MV_SAMPLERREF:
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
    HImage* hi = Get<HImage>(v.U32());
    return Value(MV_POINTER, hi->Handle());
  }
  case MV_SAMPLERREF: {
    HSampler* hs = Get<HSampler>(v.U32());
    return Value(MV_POINTER, hs->Handle());
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

HObject* HsailMemoryState::AllocateImage(MImage* mi)
{
  assert(mi && state);

  class ImageRegionMatcher {
  private:
    hsa_ext_image_data_info_t image_info;

  public:
    ImageRegionMatcher(hsa_ext_image_data_info_t image_info_): image_info(image_info_) {};

    bool operator() (HsailRuntimeContext* runtime, hsa_region_t region) {
      size_t align = 0;
      hsa_region_segment_t seg;

      runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &seg);
      if (seg == HSA_REGION_SEGMENT_GLOBAL)
      {
        runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_RUNTIME_ALLOC_ALIGNMENT, &align);
        if (align >= image_info.alignment)
          return true;
      }
      return false;
    }  
  };

  hsa_ext_image_descriptor_t image_descriptor;
  image_descriptor.height = mi->Height();
  image_descriptor.width = mi->Width();
  image_descriptor.geometry = (hsa_ext_image_geometry_t)mi->Geometry();
  image_descriptor.format.channel_order = (hsa_ext_image_channel_order_t)mi->ChannelOrder();
  image_descriptor.format.channel_type = (hsa_ext_image_channel_type_t)mi->ChannelType();
  image_descriptor.depth = mi->Depth();
  image_descriptor.array_size = mi->ArraySize(); //image_descriptor.depth > 1 ? image_descriptor.depth : 0;
  hsa_access_permission_t access_permission = (hsa_access_permission_t)mi->AccessPermission();

  hsa_ext_image_data_info_t image_info = {0};
  hsa_status_t status = Runtime()->Hsa()->hsa_ext_image_data_get_info(Runtime()->Agent(), &image_descriptor, access_permission, &image_info);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_image_data_get_info failed", status); return 0; }

  hsa_ext_image_t image = {0};
  void *ptr = NULL;
  ptr = alignedMalloc(image_info.size, image_info.alignment);
/*
  hsa_region_t region = Runtime()->GetRegion(ImageRegionMatcher(image_info));
  if (!region.handle) { Runtime()->HsaError("Failed to find image region"); return 0; }
  status = Runtime()->Hsa()->hsa_memory_allocate(region, image_info.size, &ptr);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_allocate failed", status); return 0; }
*/

  status = Runtime()->Hsa()->hsa_ext_image_create(Runtime()->Agent(), &image_descriptor, ptr, access_permission, &image);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_image_create failed", status); free(ptr); return 0; }

  //Write image handle
  mi->Data() = Value(MV_IMAGE, image.handle);

  mi->Size() = image_info.size;

  return new HImage(this, mi, ptr, image);
}

HObject* HsailMemoryState::AllocateRImage(MRImage* mr)
{
  assert(mr);
  HImage* hi = Get<HImage>(mr->RefId());
  return new HRImage(this, mr, hi);
}

HObject* HsailMemoryState::AllocateSampler(MSampler* ms)
{
  assert(ms);

  hsa_ext_sampler_descriptor_t samp_descriptor;
  samp_descriptor.coordinate_mode = (hsa_ext_sampler_coordinate_mode_t)ms->Coords();
  samp_descriptor.filter_mode = (hsa_ext_sampler_filter_mode_t)ms->Filter();
  samp_descriptor.address_mode = (hsa_ext_sampler_addressing_mode_t)ms->Addressing();
  hsa_ext_sampler_t sampler = {0};
  hsa_status_t status = Runtime()->Hsa()->hsa_ext_sampler_create(Runtime()->Agent(), &samp_descriptor, &sampler);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_sampler_create failed", status); return 0; }

  ms->Data() = Value(MV_SAMPLER, sampler.handle);

  return new HSampler(this, ms, sampler);
}

HObject* HsailMemoryState::AllocateRSampler(MRSampler* mr)
{
  assert(mr);
  HSampler* hs = Get<HSampler>(mr->RefId());
  return new HRSampler(this, mr, hs);
}

HsailRuntimeContext* HsailMemoryState::Runtime()
{
  return state->Runtime();
}

Program* HsailRuntimeContextState::NewProgram()
{
  hsa_ext_program_t handle;
  hsa_machine_model_t machineModel =
    sizeof(void *) == 4 ? HSA_MACHINE_MODEL_SMALL : HSA_MACHINE_MODEL_LARGE;
  hsa_profile_t profile = HSA_PROFILE_FULL;
  hsa_status_t status =
    Runtime()->Hsa()->hsa_ext_program_create(
      machineModel, profile, HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO, "", &handle);
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

  return new HsailModule(this, brig, GetBrigUniqueKernelOffset(brig));
}

Module* HsailRuntimeContextState::NewModuleFromHsailText(const std::string& hsailText)
{
  brig_container_t brig = brig_container_create_empty();
  int status = brig_container_assemble_from_memory(brig, hsailText.c_str(), hsailText.size(), NULL);
  if (status != 0) { Runtime()->hsailcError("brig_container_assemble_from_memory failed", brig, status); return 0; }
  return NewModuleFromBrig(brig);
}

Code* HsailRuntimeContextState::Finalize(Program* program, BrigCodeOffset32_t kernelOffset, DispatchSetup* dsetup)
{
  HsailProgram* hsailProgram = static_cast<HsailProgram*>(program);
  if (kernelOffset == 0) { kernelOffset = hsailProgram->UniqueKernelOffset(); }
  hsa_isa_t isa;
  hsa_status_t status = Runtime()->Hsa()->hsa_agent_get_info(Runtime()->Agent(), HSA_AGENT_INFO_ISA, &isa);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_agent_get_info(HSA_AGENT_INFO_ISA) failed", status); return 0; }
  hsa_ext_control_directives_t cd;
  memset(&cd, 0, sizeof(cd));
  hsa_code_object_t codeObject;
  status = Runtime()->Hsa()->hsa_ext_program_finalize(
    hsailProgram->Handle(),
    isa, 0, cd, "", HSA_CODE_OBJECT_TYPE_PROGRAM, &codeObject);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_finalize_program failed", status); return 0; }
  return new HsailCode(this, codeObject);
}

Dispatch* HsailRuntimeContextState::NewDispatch(Code* code)
{
  HsailCode* hsailCode = static_cast<HsailCode*>(code);
  hsa_executable_t executable;
  hsa_status_t status;
  status = Runtime()->Hsa()->hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN, "", &executable);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_create failed", status); return 0; }
  status = Runtime()->Hsa()->hsa_executable_load_code_object(executable, Runtime()->Agent(), hsailCode->CodeObject(), "");
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_create failed", status); return 0; }
  hsa_executable_symbol_t kernel;
  IterateData<hsa_executable_symbol_t, int> idata(Runtime(), &kernel);
  status = Runtime()->Hsa()->hsa_executable_iterate_symbols(executable, IterateExecutableSymbolsGetKernel, &idata);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_iterate_symbols failed", status); return 0; }
  assert(kernel.handle);
  return new HsailDispatch(this, executable, kernel);
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
      idata.Set(agent);
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
      idata.Set(region);
    }
  }
  return HSA_STATUS_SUCCESS;
}

hsa_status_t IterateExecutableSymbolsGetKernel(hsa_executable_t executable, hsa_executable_symbol_t symbol, void* data)
{
  assert(data);
  IterateData<hsa_executable_symbol_t, int> idata(data);
  hsa_status_t status;
  hsa_symbol_kind_t type;
  status = idata.Runtime()->Hsa()->hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_TYPE, &type);
  if (status != HSA_STATUS_SUCCESS) { idata.Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_EXECUTABLE_SYMBOL_INFO_TYPE) failed", status); return status; }
  if (type == HSA_SYMBOL_KIND_KERNEL) {
    if (idata.IsSet()) {
      idata.Runtime()->HsaError("Found more than one kernel", HSA_STATUS_ERROR);
      return HSA_STATUS_ERROR;
    }
    idata.Set(symbol);
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

  status = Hsa()->hsa_agent_get_info(agent, HSA_AGENT_INFO_PROFILE, &profile);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_agent_get_info failed", status); return false; }
  status = Hsa()->hsa_agent_get_info(agent, HSA_AGENT_INFO_WAVEFRONT_SIZE, &wavesize);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_agent_get_info failed", status); return false; }
  uint32_t wgMaxSize;
  status = Hsa()->hsa_agent_get_info(agent, HSA_AGENT_INFO_WORKGROUP_MAX_SIZE, &wgMaxSize);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_agent_get_info failed", status); return false; }
  wavesPerGroup = wgMaxSize / wavesize;
  status = Hsa()->hsa_system_get_info(HSA_SYSTEM_INFO_ENDIANNESS, &endianness);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_system_get_info failed", status); return false; }
  uint16_t exceptionMask;
  status = Hsa()->hsa_agent_get_exception_policies(agent, profile, &exceptionMask);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_agent_get_exception_policies failed", status); return false; }
  isBreakSupported = static_cast<bool>(exceptionMask & HSA_EXCEPTION_POLICY_BREAK);
  isDetectSupported = static_cast<bool>(exceptionMask & HSA_EXCEPTION_POLICY_DETECT);

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
