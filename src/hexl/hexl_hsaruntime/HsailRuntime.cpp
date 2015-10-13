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

#include "HexlTest.hpp"
#include "HsailRuntime.hpp"
#include "RuntimeContext.hpp"
#include "Scenario.hpp"
#include "Utils.hpp"
#include "DllApi.hpp"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <time.h>
#include <set>
#include <bitset>

#if defined(_WIN32) || defined(_WIN64)  // Windows
  #include <intrin.h>
  #pragma intrinsic (_InterlockedExchange16)
#endif

using namespace hexl;
using namespace hexl::runtime;

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
  GET_FUNCTION(hsa_isa_get_info);
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
  GET_FUNCTION(hsa_ext_image_get_capability);
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
bool RegionMatchSystem(HsailRuntimeContext* runtime, hsa_region_t region);

void HsaQueueErrorCallback(hsa_status_t status, hsa_queue_t *source, void *data)
{
  HsailRuntimeContext* runtime = static_cast<HsailRuntimeContext*>(data);
  runtime->QueueError(status);
}

  class HsailRuntimeContextState : public runtime::RuntimeState {
  private:
    HsailRuntimeContext* runtime;
    Context* context;
    HostThreads hostThreads;
    std::vector<std::string> keys;

    const uint32_t TIMEOUT;

  public:
    HsailRuntimeContextState(HsailRuntimeContext* runtime_, Context* context_, uint32_t timeout)
      : runtime(runtime_), context(context_), hostThreads(this), TIMEOUT(timeout) { }

    ~HsailRuntimeContextState()
    {
      for (size_t i = 0; i < keys.size(); ++i) {
        context->Delete(keys[keys.size() - 1 - i]);
      }
    }

    template <typename T>
    void Put(const std::string& key, T* t)
    {
      keys.push_back(key);
      context->Move(key, t);
    }

    HsailRuntimeContext* Runtime() { return runtime; }

    Context* GetContext() override { return context; }

    bool StartThread(unsigned id, Command* command) override
    {
      return hostThreads.StartThread(id, command);
    }

    bool WaitThreads() override
    {
      return hostThreads.WaitThreads();
    }

    virtual bool ModuleCreateFromBrig(const std::string& moduleId = "module", const std::string& brigId = "brig") override
    {
      HSAIL_ASM::BrigContainer* brig = context->Get<HSAIL_ASM::BrigContainer>(brigId);
      BrigModule_t module = brig->getBrigModule();
      context->Put(moduleId, module);
      return true;
    }

    class HsailProgram {
    private:
      HsailRuntimeContextState* rt;
      hsa_ext_program_t program;

    public:
      HsailProgram(HsailRuntimeContextState* rt_, hsa_ext_program_t program_)
        : rt(rt_), program(program_) { }
      ~HsailProgram()
      {
#ifndef _WIN32
        rt->ProgramDestroy(program);
#endif // _WIN32
      }

      hsa_ext_program_t Program() { return program; }
    };

    void ProgramDestroy(hsa_ext_program_t program)
    {
      hsa_status_t status = Runtime()->Hsa()->hsa_ext_program_destroy(program);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_program_destroy failed", status); }
    }

    virtual bool ProgramCreate(const std::string& programId = "program") override
    {
      hsa_ext_program_t program;
      hsa_machine_model_t machineModel = context->IsLarge() ? HSA_MACHINE_MODEL_LARGE : HSA_MACHINE_MODEL_SMALL;
      hsa_profile_t profile = HSA_PROFILE_FULL;
      hsa_status_t status =
        Runtime()->Hsa()->hsa_ext_program_create(
          machineModel, profile, HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO, "", &program);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_program_create failed", status); return false; }
      Put(programId, new HsailProgram(this, program));
      return true;
    }

    virtual bool ProgramAddModule(const std::string& programId = "program", const std::string& moduleId = "module") override
    {
      HsailProgram* program = context->Get<HsailProgram>(programId);
      BrigModule_t module = context->Get<BrigModuleHeader>(moduleId);
      hsa_status_t status = Runtime()->Hsa()->hsa_ext_program_add_module(program->Program(), module);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_add_module failed", status); return false; }
      return true;
    }

    class HsailCode {
    private:
      HsailRuntimeContextState* rt;
      hsa_code_object_t code;

    public:
      HsailCode(HsailRuntimeContextState* rt_, hsa_code_object_t code_)
        : rt(rt_), code(code_) { }
      ~HsailCode()
      {
#ifndef _WIN32
        // Temporarily disable due to crash on Windows.
        rt->CodeDestroy(code);
#endif // _WIN32
      }

      hsa_code_object_t Code() { return code; }
    };

    void CodeDestroy(hsa_code_object_t code)
    {
      hsa_status_t status = Runtime()->Hsa()->hsa_code_object_destroy(code);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_code_object_destroy failed", status); }
    }


    virtual bool ProgramFinalize(const std::string& codeId = "code", const std::string& programId = "program") override
    {
      HsailProgram* program = context->Get<HsailProgram>(programId);
      hsa_isa_t isa;
      hsa_status_t status = Runtime()->Hsa()->hsa_agent_get_info(Runtime()->Agent(), HSA_AGENT_INFO_ISA, &isa);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_agent_get_info(HSA_AGENT_INFO_ISA) failed", status); return 0; }
      hsa_ext_control_directives_t cd;
      memset(&cd, 0, sizeof(cd));
      hsa_code_object_t codeObject;
      status = Runtime()->Hsa()->hsa_ext_program_finalize(
        program->Program(),
        isa, 0, cd, "", HSA_CODE_OBJECT_TYPE_PROGRAM, &codeObject);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_finalize_program failed", status); return false; }
      Put(codeId, new HsailCode(this, codeObject));
      return true;
    }

    class HsailExecutable {
    private:
      HsailRuntimeContextState* rt;
      hsa_executable_t executable;

    public:
      HsailExecutable(HsailRuntimeContextState* rt_, hsa_executable_t executable_)
        : rt(rt_), executable(executable_) { }
      ~HsailExecutable()
      {
#ifndef _WIN32
        rt->ExecutableDestroy(executable);
#endif // _WIN32
      }

      hsa_executable_t Executable() { return executable; }
    };

    void ExecutableDestroy(hsa_executable_t executable)
    {
      hsa_status_t status = Runtime()->Hsa()->hsa_executable_destroy(executable);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_destroy failed", status); }
    }

    virtual bool ExecutableCreate(const std::string& executableId = "executable") override
    {
      hsa_executable_t executable;
      hsa_status_t status = Runtime()->Hsa()->hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN, "", &executable);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_create failed", status); return false; }
      Put(executableId, new HsailExecutable(this, executable));
      return true;
    }

    virtual bool ExecutableLoadCode(const std::string& executableId = "executable", const std::string& codeId = "code") override
    {
      HsailExecutable* executable = context->Get<HsailExecutable>(executableId);
      HsailCode* code = context->Get<HsailCode>(codeId);
      hsa_status_t status = Runtime()->Hsa()->hsa_executable_load_code_object(executable->Executable(), Runtime()->Agent(), code->Code(), "");
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_load_code failed", status); return false; }
      return true;
    }

    virtual bool ExecutableFreeze(const std::string& executableId = "executable") override
    {
      HsailExecutable* executable = context->Get<HsailExecutable>(executableId);
      hsa_status_t status = Runtime()->Hsa()->hsa_executable_freeze(executable->Executable(), "");
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_freeze failed", status); return false; }
      return true;
    }

    class HsailBuffer {
    private:
      HsailRuntimeContextState* rt;
      void *ptr;

    public:
      HsailBuffer(HsailRuntimeContextState* rt_, void *ptr_)
        : rt(rt_), ptr(ptr_) { }
      ~HsailBuffer()
      {
#ifndef _WIN32
        rt->BufferDestroy(ptr);
#endif // _WIN32
      }

      void* Ptr() { return ptr; }
    };

    void BufferDestroy(void *ptr)
    {
      hsa_status_t status;
      switch (Runtime()->Profile()) {
      case HSA_PROFILE_FULL:
        status = Runtime()->Hsa()->hsa_memory_deregister(ptr);
        if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_deregister failed", status); }
        alignedFree(ptr);
        break;
      case HSA_PROFILE_BASE:
        status = Runtime()->Hsa()->hsa_memory_free(ptr);
        if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_free failed", status); }
        break;
      default:
        assert(false); return;
      }
    }

    virtual bool BufferCreate(const std::string& bufferId, size_t size, const std::string& initValuesId) override
    {
      size = (std::max)(size, (size_t) 256);
      void *ptr;
      hsa_status_t status;
      switch (Runtime()->Profile()) {
      case HSA_PROFILE_FULL:
        ptr = alignedMalloc(size, 256);
        status = Runtime()->Hsa()->hsa_memory_register(ptr, size);
        if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_register failed", status); alignedFree(ptr); return false; }
        break;
      case HSA_PROFILE_BASE:
        status = Runtime()->Hsa()->hsa_memory_allocate(Runtime()->SystemRegion(), size, &ptr);
        if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_allocate failed", status); return false; }
        break;
      default:
        assert(false); return false;
      }
      if (!initValuesId.empty()) {
        Values* initValues = context->Get<Values>(initValuesId);
        assert(initValues->size() <= size);
        char *vptr = (char *) ptr;
        for (size_t i = 0; i < initValues->size(); ++i) {
          Value v = context->GetRuntimeValue((*initValues)[i]);
          v.WriteTo(vptr);
          vptr += v.Size();
        }
      }
      Put(bufferId, new HsailBuffer(this, ptr));
      return true;
    }

    virtual bool BufferValidate(const std::string& bufferId, const std::string& expectedValuesId, ValueType memoryType, const std::string& method = "") override
    {
      HsailBuffer *buf = context->Get<HsailBuffer>(bufferId);
      context->Info() << "Validating buffer " << bufferId << " with expected values " << expectedValuesId << "(method: " << method << ")" << std::endl;
      Values* expectedValues = context->Get<Values>(expectedValuesId);
      return ValidateMemory(context, memoryType, *expectedValues, buf->Ptr(), method);
    }

    hsa_access_permission_t ImageType2HsaAccessPermission(BrigType type)
    {
      switch (type) {
      case BRIG_TYPE_ROIMG:
        return HSA_ACCESS_PERMISSION_RO;
      case BRIG_TYPE_RWIMG:
        return HSA_ACCESS_PERMISSION_RW;
      case BRIG_TYPE_WOIMG:
        return HSA_ACCESS_PERMISSION_WO;
      default:
        assert(!"Unsupported type in ImageType2HsaAccessPermission"); return (hsa_access_permission_t) 0;
      }
    }

    class HsailImage {
    private:
      HsailRuntimeContextState* rt;
      hsa_ext_image_t image;
      void *data;

    public:
      HsailImage(HsailRuntimeContextState* rt_, hsa_ext_image_t image_, void *data_)
        : rt(rt_), image(image_), data(data_) { }
      ~HsailImage()
      {
#ifndef _WIN32
        rt->ImageDestroy(image, data);
#endif // _WIN32
      }

      hsa_ext_image_t Image() { return image; }
      void *Data() { return data; }
    };

    void ImageDestroy(hsa_ext_image_t image, void *data)
    {
      hsa_status_t status;
      /*
      status = Runtime()->Hsa()->hsa_memory_deregister(data);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_deregister (image data) failed", status); }
      */

      status = Runtime()->Hsa()->hsa_ext_image_destroy(Runtime()->Agent(), image);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_image_destroy failed", status); }
      alignedFree(data);
    }

    virtual bool ImageInitialize(const std::string& imageId, const std::string& imageParamsId,
                                 const std::string& initValueId) override
    {
      auto image = context->Get<HsailImage>(imageId);
      auto initValue = context->GetValue(initValueId);
      auto imageParams = context->Get<ImageParams>(imageParamsId);

      hsa_ext_image_region_t hsaRegion;
      hsaRegion.offset.x = 0;
      hsaRegion.offset.y = 0;
      hsaRegion.offset.z = 0;
      hsaRegion.range.x = (uint32_t)imageParams->width;
      hsaRegion.range.y = (uint32_t)imageParams->height;
      hsaRegion.range.z = (uint32_t)imageParams->depth;

      auto size = imageParams->width * imageParams->height * imageParams->depth;
      auto buff = new char[size * initValue.Size()];
      auto cbuff = buff;
      for (uint32_t i = 0; i < size; ++i) {
        initValue.WriteTo(cbuff);
        cbuff += initValue.Size();
      }
      hsa_status_t status = Runtime()->Hsa()->hsa_ext_image_import(Runtime()->Agent(), buff,
        imageParams->width, imageParams->width * imageParams->height, image->Image(), &hsaRegion);
      delete[] buff;
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_image_import failed", status); return false; }
      return true;
    }

    virtual bool ImageWrite(const std::string& imageId, const std::string& writeValuesId,
                            const ImageRegion& region) override
    {
      if (region.size_x == 0 || region.size_y == 0 || region.size_z == 0) {
        return true;
      }
      auto size = region.size_x * region.size_y * region.size_z;
      auto image = context->Get<HsailImage>(imageId);
      auto writeValues = context->Get<Values>(writeValuesId);
      assert(writeValues->size() == size);

      hsa_ext_image_region_t hsaRegion;
      hsaRegion.offset.x = region.x;
      hsaRegion.offset.y = region.y;
      hsaRegion.offset.z = region.z;
      hsaRegion.range.x = region.size_x;
      hsaRegion.range.y = region.size_y;
      hsaRegion.range.z = region.size_z;

      char* buff = new char[SizeOf(*writeValues)];
      WriteTo(buff, *writeValues);
      hsa_status_t status = Runtime()->Hsa()->hsa_ext_image_import(Runtime()->Agent(), buff,
        region.size_x, region.size_x * region.size_y, image->Image(), &hsaRegion);
      delete[] buff;
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_image_import failed", status); return false; }
      return true;
    }

    virtual bool ImageCreate(const std::string& imageId, const std::string& imageParamsId, bool optionalFormat) override
    {
      hsa_status_t status;

      const ImageParams* ip = context->Get<ImageParams>(imageParamsId);

      hsa_access_permission_t access_permission = ImageType2HsaAccessPermission(ip->imageType);

      if (optionalFormat) {
        hsa_ext_image_format_t format;
        format.channel_order = (hsa_ext_image_channel_order_t) ip->channelOrder;
        format.channel_type = (hsa_ext_image_channel_type_t) ip->channelType;
        uint32_t capability_mask;
        status = Runtime()->Hsa()->hsa_ext_image_get_capability(Runtime()->Agent(), (hsa_ext_image_geometry_t) ip->geometry, &format, &capability_mask);
        if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_image_get_capability failed", status); return false; }
        bool supported;
        switch (access_permission) {
        case HSA_ACCESS_PERMISSION_RO: supported = capability_mask & HSA_EXT_IMAGE_CAPABILITY_READ_ONLY; break;
        case HSA_ACCESS_PERMISSION_WO: supported = capability_mask & HSA_EXT_IMAGE_CAPABILITY_WRITE_ONLY; break;
        case HSA_ACCESS_PERMISSION_RW: supported = capability_mask & HSA_EXT_IMAGE_CAPABILITY_READ_WRITE; break;
        default: assert(false); supported = false; break;
        }
        if (!supported) {
          context->Move(TEST_STATUS_KEY, new TestStatus(NA));
          return false;
        }
      }

      hsa_ext_image_descriptor_t image_descriptor;
      image_descriptor.geometry = (hsa_ext_image_geometry_t) ip->geometry;
      image_descriptor.format.channel_order = (hsa_ext_image_channel_order_t) ip->channelOrder;
      image_descriptor.format.channel_type = (hsa_ext_image_channel_type_t) ip->channelType;
      image_descriptor.width = ip->width;
      image_descriptor.height = ip->height;
      image_descriptor.depth = ip->depth;
      image_descriptor.array_size = ip->arraySize;

      hsa_ext_image_data_info_t image_info = {0};
      status = Runtime()->Hsa()->hsa_ext_image_data_get_info(Runtime()->Agent(), &image_descriptor, access_permission, &image_info);
      if (status == static_cast<hsa_status_t>(HSA_EXT_STATUS_ERROR_IMAGE_SIZE_UNSUPPORTED)) {
        context->Move(TEST_STATUS_KEY, new TestStatus(NA));
        return false;
      } else if (status != HSA_STATUS_SUCCESS) {
        Runtime()->HsaError("hsa_ext_image_data_get_info failed", status);
        return false;
      }

      hsa_ext_image_t image = {0};
      void *imageData = NULL;
      size_t size = (std::max)(image_info.size, (size_t) 256);
      imageData = alignedMalloc(size, image_info.alignment);

      /*
      status = Runtime()->Hsa()->hsa_memory_register(imageData, size);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_register (image data) failed", status); alignedFree(imageData); return 0; }
      */

/*
  hsa_region_t region = Runtime()->GetRegion(ImageRegionMatcher(image_info));
  if (!region.handle) { Runtime()->HsaError("Failed to find image region"); return 0; }
  status = Runtime()->Hsa()->hsa_memory_allocate(region, image_info.size, &ptr);
  if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_allocate failed", status); return 0; }
*/

      status = Runtime()->Hsa()->hsa_ext_image_create(Runtime()->Agent(), &image_descriptor, imageData, access_permission, &image);
      if (status == HSA_STATUS_ERROR_OUT_OF_RESOURCES) {
        context->Move(TEST_STATUS_KEY, new TestStatus(NA));
        return false;
      }
      if (status != HSA_STATUS_SUCCESS) {
        Runtime()->HsaError("hsa_ext_image_create failed", status); alignedFree(imageData);
        return false;
      }

      Put(imageId, new HsailImage(this, image, imageData));
      context->Put(imageId + ".handle", Value(MV_UINT64, image.handle));
      return true;
    }

    virtual bool ImageValidate(const std::string& imageId, const std::string& expectedValuesId, ValueType memoryType, const std::string& method = "") override
    {
      HsailImage* image = context->Get<HsailImage>(imageId);
      Values expectedValues = context->GetValues(expectedValuesId);
      return ValidateMemory(context, memoryType, expectedValues, image->Data(), method);
    }

    class HsailSampler {
    private:
      HsailRuntimeContextState* rt;
      hsa_ext_sampler_t sampler;

    public:
      HsailSampler(HsailRuntimeContextState* rt_, hsa_ext_sampler_t sampler_)
        : rt(rt_), sampler(sampler_) { }
      ~HsailSampler()
      {
#ifndef _WIN32
        rt->SamplerDestroy(sampler);
#endif // _WIN32
      }

      hsa_ext_sampler_t Sampler() { return sampler; }
    };

    void SamplerDestroy(hsa_ext_sampler_t sampler)
    {
      hsa_status_t status = Runtime()->Hsa()->hsa_ext_sampler_destroy(Runtime()->Agent(), sampler);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_sampler_destroy failed", status); }
    }

    virtual bool SamplerCreate(const std::string& samplerId, const std::string& samplerParamsId)
    {
      SamplerParams* params = context->Get<SamplerParams>(samplerParamsId);
      hsa_ext_sampler_descriptor_t sampler_descriptor;
      sampler_descriptor.address_mode = (hsa_ext_sampler_addressing_mode_t) params->Addressing();
      sampler_descriptor.coordinate_mode = (hsa_ext_sampler_coordinate_mode_t) params->Coord();
      sampler_descriptor.filter_mode = (hsa_ext_sampler_filter_mode_t) params->Filter();
      hsa_ext_sampler_t sampler;
      hsa_status_t status = Runtime()->Hsa()->hsa_ext_sampler_create(Runtime()->Agent(), &sampler_descriptor, &sampler);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_ext_sampler_create failed", status); return false; }
      Put(samplerId, new HsailSampler(this, sampler));
      return true;
    }

    struct HsailDispatch {
      HsailRuntimeContextState* rt;
      hsa_executable_t executable;
      hsa_executable_symbol_t kernel;
      uint64_t packetId;
      hsa_kernel_dispatch_packet_t* packet;
      uint64_t timeout;
      size_t kernargOffset;
      void* kernargAddr;
      hsa_signal_t completionSignal;

      HsailDispatch(HsailRuntimeContextState* rt_)
        : rt(rt_) { }

      ~HsailDispatch() {
#ifndef _WIN32
        rt->DispatchDestroy(this);
#endif // _WIN32
      }
    };

    void DispatchDestroy(HsailDispatch* dispatch)
    {
      Runtime()->Hsa()->hsa_memory_free(dispatch->kernargAddr);
      Runtime()->Hsa()->hsa_signal_destroy(dispatch->completionSignal);
    }

    virtual bool DispatchCreate(const std::string& dispatchId, const std::string& executableId, const std::string& kernelName) override
    {
      hsa_status_t status;
      HsailExecutable* executable = context->Get<HsailExecutable>(executableId);

      bool hasMain = context->Has(dispatchId, "main_module_name");
      std::string mainModuleName = hasMain ? (std::string("&") + context->GetString(dispatchId, "main_module_name")) : "";

      hsa_executable_symbol_t kernel;
      if (!kernelName.empty()) {
        std::string kname = "&" + kernelName;
        status = Runtime()->Hsa()->hsa_executable_get_symbol(
          executable->Executable(),
          hasMain ? mainModuleName.c_str() : nullptr,
          kname.c_str(),
          Runtime()->Agent(),
          0,
          &kernel);
        if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_get_symbol failed", status); return false; }
      } else {
        IterateData<hsa_executable_symbol_t, int> idata(Runtime(), &kernel);
        status = Runtime()->Hsa()->hsa_executable_iterate_symbols(executable->Executable(), IterateExecutableSymbolsGetKernel, &idata);
      }
      assert(kernel.handle);

      uint32_t kernargSize;
      status = Runtime()->Hsa()->hsa_executable_symbol_get_info(kernel, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE, &kernargSize);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE) failed", status); return false; }

      hsa_queue_t* queue = Runtime()->QueueNoError();
      if (!queue) { runtime->HsaError("Queue is not available"); return false; }
      uint64_t packetId = Runtime()->Hsa()->hsa_queue_add_write_index_relaxed(queue, 1);
      context->Put(dispatchId, "dispatchpacketid", Value(MV_UINT64, packetId));
      hsa_kernel_dispatch_packet_t* p = (hsa_kernel_dispatch_packet_t*) queue->base_address + (packetId % queue->size);
      memset(((uint8_t*) p) + 4, 0, sizeof(hsa_kernel_dispatch_packet_t) - 4);

      status = Runtime()->Hsa()->hsa_executable_symbol_get_info(
        kernel, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &p->kernel_object);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT) failed", status); return false; }

      if (kernargSize > 0) {
        status = Runtime()->Hsa()->hsa_memory_allocate(Runtime()->KernargRegion(), kernargSize, &p->kernarg_address);
        if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_memory_allocate(kernargRegion) failed", status); return false; }
      } else {
        p->kernarg_address = 0;
      }

      status = Runtime()->Hsa()->hsa_executable_symbol_get_info(
        kernel, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE, &p->private_segment_size);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE) failed", status); return false; }
      bool dynamicCallStack = false;
      status = Runtime()->Hsa()->hsa_executable_symbol_get_info(
        kernel, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_DYNAMIC_CALLSTACK, &dynamicCallStack);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_CODE_SYMBOL_INFO_KERNEL_DYNAMIC_CALLSTACK) failed", status); return false; }
      if (dynamicCallStack) {
        // Set to max minimum allowed by the spec for now (64k per work-group).
        // TODO: a strategy for choosing this size, for example, based on expected number of frames/extra allocation used by test.
        context->Info() << "Enabling dynamic call stack: setting private_segment_size to 256/workitem" << std::endl;
        p->private_segment_size = (std::max)((uint32_t) 256, p->private_segment_size);
      }

      status = Runtime()->Hsa()->hsa_executable_symbol_get_info(
        kernel, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &p->group_segment_size);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_executable_symbol_get_info(HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE) failed", status); return false; }
      context->Put(dispatchId, "staticgroupsize", Value(MV_UINT32, p->group_segment_size));
      if (context->Has(dispatchId, "dynamicgroupsize")) {
        p->group_segment_size += context->GetValue(dispatchId, "dynamicgroupsize").U32();
      }

      status = Runtime()->Hsa()->hsa_signal_create(1, 0, 0, &p->completion_signal);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_signal_create(completion_signal) failed", status); return false; }
      context->Put(dispatchId, "packetcompletionsig", Value(MV_UINT64, p->completion_signal.handle));

      p->workgroup_size_x = context->GetValue(dispatchId, "workgroupSize[0]").U16();
      p->workgroup_size_y = context->GetValue(dispatchId, "workgroupSize[1]").U16();
      p->workgroup_size_z = context->GetValue(dispatchId, "workgroupSize[2]").U16();
      p->grid_size_x = context->GetValue(dispatchId, "gridSize[0]").U32();
      p->grid_size_y = context->GetValue(dispatchId, "gridSize[1]").U32();
      p->grid_size_z = context->GetValue(dispatchId, "gridSize[2]").U32();

      HsailDispatch* d = new HsailDispatch(this);
      d->executable = executable->Executable();
      d->kernel = kernel;
      d->packetId = packetId;
      d->packet = p;
      d->timeout = TIMEOUT * CLOCKS_PER_SEC;
      d->kernargOffset = 0;
      d->kernargAddr = p->kernarg_address;
      d->completionSignal = p->completion_signal;
      Put(dispatchId, d);
      return true;
    }

    Value GetValue(const std::string& dispatchId, DispatchArgType argType, const std::string& argKey)
    {
      switch (argType) {
      case DARG_VALUE:
        return context->GetValue(argKey);
      case DARG_BUFFER:
      {
        HsailBuffer* buf = context->Get<HsailBuffer>(argKey);
        return Value(MV_POINTER, P(buf->Ptr()));
      }
      case DARG_IMAGE:
      {
        HsailImage* image = context->Get<HsailImage>(argKey);
        return Value(MV_UINT64, image->Image().handle);
      }
      case DARG_SAMPLER:
      {
        HsailSampler* sampler = context->Get<HsailSampler>(argKey);
        return Value(MV_UINT64, sampler->Sampler().handle);
      }
      case DARG_SIGNAL:
      {
        HsailSignal* signal = context->Get<HsailSignal>(argKey);
        return Value(MV_UINT64, signal->Signal().handle);
      }
      case DARG_QUEUE:
      {
        HsailQueue* queue = context->Get<HsailQueue>(argKey);
        return Value(context->IsLarge() ? MV_UINT64 : MV_UINT32, (uint64_t) (uintptr_t) queue->Queue());
      }
      case DARG_GROUPOFFSET :
      {
        Value dynamicOffset = context->GetValue(argKey);
        assert(dynamicOffset.Type() == MV_UINT32);
        Value groupSize = context->GetValue(dispatchId, "staticgroupsize");
        return Value(MV_UINT32, groupSize.U32() + dynamicOffset.U32());
      }
      default:
        assert(!"Unsupported arg type in GetValue"); return Value(MV_UINT64, 0);
      }
    }

    bool DispatchArg(const std::string& dispatchId, DispatchArgType argType, const std::string& argKey) override
    {
      HsailDispatch* d = context->Get<HsailDispatch>(dispatchId);
      char *kernarg = (char *) d->kernargAddr;
      switch (argType) {
      case DARG_VALUES:
      {
        Values* values = context->Get<Values>(argKey);
        assert(values->size() > 0);
        Value v = (*values)[0];
        d->kernargOffset = ((d->kernargOffset + v.Size() - 1) / v.Size()) * v.Size();
        WriteTo(kernarg + d->kernargOffset, *values);
        d->kernargOffset += v.Size() * values->size();
        break;
      }
      default:
      {
        Value v = GetValue(dispatchId, argType, argKey);
        d->kernargOffset = ((d->kernargOffset + v.Size() - 1) / v.Size()) * v.Size();
        v.WriteTo(kernarg + d->kernargOffset);
        d->kernargOffset += v.Size();
        break;
      }
      }
      return true;
    }

    void SetPacketHeader(uint32_t* packet, uint16_t header, uint16_t setup) {
      uint32_t header32 = header | (setup << 16);
      #if defined(_WIN32) || defined(_WIN64)  // Windows
        _InterlockedExchange(packet, header32);
      #else // Linux
        __atomic_store_n(packet, header32, __ATOMIC_RELEASE);
      #endif
    }

    virtual bool DispatchExecute(const std::string& dispatchId) override
    {
      HsailDispatch* d = context->Get<HsailDispatch>(dispatchId);
      assert(d);

      hsa_queue_t* queue = Runtime()->Queue();

      // Notify.
      uint16_t header = (1 << HSA_PACKET_HEADER_BARRIER) |
        (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE) |
        (HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE) |
        (HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE);
      uint16_t setup = context->GetValue(dispatchId, "dimensions").U16() << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
      SetPacketHeader(reinterpret_cast<uint32_t*>(d->packet), header, setup);
      Runtime()->Hsa()->hsa_signal_store_release(queue->doorbell_signal, d->packetId);

      // Wait for kernel completion.
      hsa_signal_value_t result;
      clock_t beg = clock();
      do {
        result = Runtime()->Hsa()->hsa_signal_wait_acquire(d->completionSignal, HSA_SIGNAL_CONDITION_EQ, 0, d->timeout, HSA_WAIT_STATE_ACTIVE);
        clock_t clocks = clock() - beg;
        if (clocks > (clock_t) d->timeout && result != 0) {
          context->Error() << "Kernel execution timed out, elapsed time: " << (long) clocks << " clocks (clocks per second " << (long) CLOCKS_PER_SEC << ")" << std::endl;
          return false;
        }
      } while (result!= 0 && !runtime->IsQueueError());
      return !runtime->IsQueueError();
    }

    class HsailSignal {
    private:
      HsailRuntimeContextState* rt;
      hsa_signal_t signal;

    public:
      HsailSignal(HsailRuntimeContextState* rt_, hsa_signal_t signal_)
        : rt(rt_), signal(signal_) { }
      ~HsailSignal()
      {
#ifndef _WIN32
        rt->SignalDestroy(signal);
#endif // _WIN32
      }

      hsa_signal_t Signal() { return signal; }
    };

    void SignalDestroy(hsa_signal_t signal)
    {
      hsa_status_t status = Runtime()->Hsa()->hsa_signal_destroy(signal);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_signal_destroy failed", status); }
    }

    virtual bool SignalCreate(const std::string& signalId, uint64_t signalInitialValue = 1) override
    {
      hsa_status_t status;
      hsa_signal_t signal;
      status = Runtime()->Hsa()->hsa_signal_create(signalInitialValue, 0, 0, &signal);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_signal_create failed", status); return false; }
      Put(signalId, new HsailSignal(this, signal));
      return true;
    }

    virtual bool SignalSend(const std::string& signalId, uint64_t signalSendValue = 1) override
    {
      HsailSignal* signal = context->Get<HsailSignal>(signalId);
      Runtime()->Hsa()->hsa_signal_store_release(signal->Signal(), signalSendValue);
      return true;
    }

    virtual bool SignalWait(const std::string& signalId, uint64_t expectedValue = 1) override
    {
      uint64_t timeout = TIMEOUT * CLOCKS_PER_SEC;
      HsailSignal* signal = context->Get<HsailSignal>(signalId);
      hsa_signal_value_t acquiredValue;
      bool result = true;
      clock_t beg = clock();
      do {
        acquiredValue = Runtime()->Hsa()->hsa_signal_wait_acquire(signal->Signal(), HSA_SIGNAL_CONDITION_EQ, expectedValue, timeout, HSA_WAIT_STATE_ACTIVE);
        clock_t clocks = clock() - beg;
        if (clocks > (clock_t) timeout && acquiredValue != (hsa_signal_value_t) expectedValue) {
          context->Info() << "Signal '" << signalId << "' wait timed out, elapsed time: " <<
            (uint64_t)clocks << " clocks (clocks per second " << (uint64_t)CLOCKS_PER_SEC << ")" << std::endl;
          result = false;
          break;
        }
      } while ((hsa_signal_value_t) expectedValue != acquiredValue);
      context->Info() << "Signal '" << signalId << "' handle: " << std::hex << signal->Signal().handle << std::dec
                      << ", expected value: " << expectedValue << ", acquired value: " << acquiredValue << std::endl;
      return result;
    }

    class HsailQueue {
    private:
      HsailRuntimeContextState* rt;
      hsa_queue_t* queue;

    public:
      HsailQueue(HsailRuntimeContextState* rt_, hsa_queue_t* queue_)
        : rt(rt_), queue(queue_) { }
      ~HsailQueue()
      {
#ifndef _WIN32
        rt->QueueDestroy(queue);
#endif // _WIN32
      }

      hsa_queue_t* Queue() { return queue; }
    };

    void QueueDestroy(hsa_queue_t* queue)
    {
      hsa_status_t status = Runtime()->Hsa()->hsa_queue_destroy(queue);
      if (status != HSA_STATUS_SUCCESS) { Runtime()->HsaError("hsa_queue_destroy failed", status); }
    }

    virtual bool QueueCreate(const std::string& queueId, uint32_t size = 0) override
    {
      hsa_queue_t* queue;
      hsa_status_t status;
      if (size == 0) {
        status = Runtime()->Hsa()->hsa_agent_get_info(runtime->Agent(), HSA_AGENT_INFO_QUEUE_MAX_SIZE, &size);
        if (status != HSA_STATUS_SUCCESS) {
          runtime->HsaError("hsa_agent_get_info failed", status);
          return false;
        }
      }
      status = Runtime()->Hsa()->hsa_queue_create(runtime->Agent(), size, HSA_QUEUE_TYPE_MULTI, HsaQueueErrorCallback, runtime, UINT32_MAX, UINT32_MAX, &queue);
      if (status != HSA_STATUS_SUCCESS) {
        runtime->HsaError("hsa_queue_create failed", status);
        return false;
      }
      Put(queueId, new HsailQueue(this, queue));
      return true;
    }

    virtual bool IsDetectSupported() override {
      bool supported = false;
      uint16_t exceptionMask;
      hsa_status_t status = Runtime()->Hsa()->hsa_agent_get_exception_policies(
        Runtime()->Agent(),
        Runtime()->IsFullProfile() ? HSA_PROFILE_FULL : HSA_PROFILE_BASE,
        &exceptionMask);
      if (status == HSA_STATUS_SUCCESS) {
        supported = static_cast<bool>(exceptionMask & HSA_EXCEPTION_POLICY_DETECT);
      }
      if (!supported) {
        context->Move(TEST_STATUS_KEY, new TestStatus(NA));
        return false;
      }
      return true;
    }

    virtual bool IsBreakSupported() override {
      bool supported = false;
      uint16_t exceptionMask;
      hsa_status_t status = Runtime()->Hsa()->hsa_agent_get_exception_policies(
        Runtime()->Agent(),
        Runtime()->IsFullProfile() ? HSA_PROFILE_FULL : HSA_PROFILE_BASE,
        &exceptionMask);
      if (status == HSA_STATUS_SUCCESS) {
        supported = static_cast<bool>(exceptionMask & HSA_EXCEPTION_POLICY_BREAK);
      }
      if (!supported) {
        context->Move(TEST_STATUS_KEY, new TestStatus(NA));
        return false;
      }
      return true;
    }

  };

#ifdef _WIN32
#define HSARUNTIMEDEFAULTNAME (sizeof(void*) == 4) ? "hsa-runtime.dll" : "hsa-runtime64.dll"
#else
#define HSARUNTIMEDEFAULTNAME (sizeof(void*) == 4) ? "libhsa-runtime.so.1" : "libhsa-runtime64.so.1"
#endif

HsailRuntimeContext::HsailRuntimeContext(Context* context)
  : RuntimeContext(context),
    hsaApi(context, context->Opts(), context->Opts()->GetString("rtlib", HSARUNTIMEDEFAULTNAME)),
    queue(0), queueSize(0), queueError(false)
{
}

runtime::RuntimeState* HsailRuntimeContext::NewState(Context* context)
{
  this->context = context;
  return new HsailRuntimeContextState(this, context, context->Opts()->GetUnsigned("timeout", HSAILRUNTIMEDEFAULTTIMEOUT));
}

void HsailRuntimeContext::QueueDestroy()
{
  assert(queue);
  hsa_status_t status;
  status = Hsa()->hsa_queue_destroy(queue);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_queue_destroy failed", status); }
  queue = 0;
}

bool HsailRuntimeContext::QueueInit()
{
  assert(!queue);
  hsa_status_t status = Hsa()->hsa_queue_create(agent, queueSize, HSA_QUEUE_TYPE_SINGLE, HsaQueueErrorCallback, this, UINT32_MAX, UINT32_MAX, &queue);
  if (status != HSA_STATUS_SUCCESS) { HsaError("hsa_queue_create failed", status); return false; }
  return true;
}

void HsailRuntimeContext::QueueError(hsa_status_t status)
{
  // Note: cannot simply do QueueError here because of cleanup of other resource.
  // That's why queue restart is done in DispatchCreate after previous test
  // has already completed. Here. simply note the fact that queue is in error state.
  HsaError("Queue error", status);
  queueError = true;
}

hsa_queue_t* HsailRuntimeContext::QueueNoError()
{
  if (queueError && queue) { QueueDestroy(); }
  if (!queue) { QueueInit(); }
  queueError = false;
  return queue;
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
    if (features & HSA_AGENT_FEATURE_KERNEL_DISPATCH) {
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
  if (!QueueInit()) { return false; }

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

  kernargRegion = GetRegion(RegionMatchKernarg);
  if (!kernargRegion.handle) { context->Error() << "Failed to find kernarg region" << std::endl; return false; }

  systemRegion = GetRegion(RegionMatchSystem);
  if (!systemRegion.handle) { context->Error() << "Failed to find system region" << std::endl; return false; }

  context->Put("queueid", Value(MV_UINT32, Queue()->id));
  context->Put("queueptr", Value(context->IsLarge() ? MV_UINT64 : MV_UINT32, (uintptr_t) Queue()));
  return true;
}

void HsailRuntimeContext::Dispose()
{
  if (context) {
    QueueDestroy();
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

#define CHECK_HSA_STATUS(MESSAGE, FUNC) \
  { \
    hsa_status_t status = Hsa()->FUNC; \
    if (status != HSA_STATUS_SUCCESS) { \
      HsaError(MESSAGE, status); \
      return; \
    } \
  }

void HsailRuntimeContext::PrintSystemInfo(std::ostream& out)
{
  uint16_t major;
  CHECK_HSA_STATUS("hsa_system_get_info failed",
                   hsa_system_get_info(HSA_SYSTEM_INFO_VERSION_MAJOR, &major));
  out << "Major version of the HSA runtime specification supported: " << major << std::endl;

  uint16_t minor;
  CHECK_HSA_STATUS("hsa_system_get_info failed",
                   hsa_system_get_info(HSA_SYSTEM_INFO_VERSION_MAJOR, &minor));
  out << "Minor version of the HSA runtime specification supported: " << minor << std::endl;

  uint64_t timestampFreq;
  CHECK_HSA_STATUS("hsa_system_get_info failed",
                   hsa_system_get_info(HSA_SYSTEM_INFO_TIMESTAMP_FREQUENCY, &timestampFreq));
  out << "Timestamp value increase rate, in Hz: " << timestampFreq << std::endl;

  uint64_t signalMaxWait;
  CHECK_HSA_STATUS("hsa_system_get_info failed",
                   hsa_system_get_info(HSA_SYSTEM_INFO_SIGNAL_MAX_WAIT, &signalMaxWait));
  out << "Maximum duration of signal wait operation: " << signalMaxWait << std::endl;

  hsa_endianness_t endianness;
  CHECK_HSA_STATUS("hsa_system_get_info failed",
                   hsa_system_get_info(HSA_SYSTEM_INFO_ENDIANNESS, &endianness));
  out << "Endianness of the system: "
      << (endianness == HSA_ENDIANNESS_BIG ? "HSA_ENDIANNESS_BIG" : "HSA_ENDIANNESS_LITTLE")
      << std::endl;

  hsa_machine_model_t machineModel;
  CHECK_HSA_STATUS("hsa_system_get_info failed",
                   hsa_system_get_info(HSA_SYSTEM_INFO_MACHINE_MODEL, &machineModel));
  out << "Machine model: "
      << (machineModel == HSA_MACHINE_MODEL_LARGE ? "HSA_MACHINE_MODEL_LARGE" : "HSA_MACHINE_MODEL_SMALL")
      << std::endl;

  uint8_t extensionMask[128];
  CHECK_HSA_STATUS("hsa_system_get_info failed",
                   hsa_system_get_info(HSA_SYSTEM_INFO_EXTENSIONS, extensionMask));
  out << "Extensions:" << std::endl;
  out << '\t' << "Finalizer: "
      << ((1<<0 | extensionMask[0]) ? "supported" : "not supported") << std::endl;
  out << '\t' << "Images: "
      << ((1<<1 | extensionMask[0]) ? "supported" : "not supported") << std::endl;
}

void HsailRuntimeContext::PrintAgentInfo(std::ostream& out, hsa_agent_t agent)
{
  char name[64];
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, name));
  out << "Agent name: " << name << std::endl;

  char vendorName[64];
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_VENDOR_NAME, vendorName));
  out << "Name of vendor: " << vendorName << std::endl;

  hsa_agent_feature_t feature;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_FEATURE, &feature));
  out << "Agent capability: "
      << (feature == HSA_AGENT_FEATURE_AGENT_DISPATCH ? "HSA_AGENT_FEATURE_AGENT_DISPATCH" : "HSA_AGENT_FEATURE_KERNEL_DISPATCH")
      << std::endl;

  hsa_machine_model_t machineModel;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_MACHINE_MODEL, &machineModel));
  out << "Machine model: "
      << (machineModel == HSA_MACHINE_MODEL_LARGE ? "HSA_MACHINE_MODEL_LARGE" : "HSA_MACHINE_MODEL_SMALL")
      << std::endl;

  hsa_profile_t profile;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_PROFILE, &profile));
  out << "Profile: "
      << (profile == HSA_PROFILE_BASE ? "HSA_PROFILE_BASE" : "HSA_PROFILE_FULL")
      << std::endl;

  hsa_default_float_rounding_mode_t rounding;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_DEFAULT_FLOAT_ROUNDING_MODE, &rounding));
  out << "Default floating-point rounding mode: ";
  switch (rounding) {
    case HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT:
      out << "HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT"; break;
    case HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO:
      out << "HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO"; break;
    case HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR:
      out << "HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR"; break;
  default:
    HsaError("hsa_agent_get_info failed");
  }
  out << std::endl;

  hsa_default_float_rounding_mode_t baseRounding;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_BASE_PROFILE_DEFAULT_FLOAT_ROUNDING_MODES, &baseRounding));
  out << "Base profile default floating-point rounding modes:" << std::endl;
  out << '\t' << "HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO: "
      << ((baseRounding | HSA_DEFAULT_FLOAT_ROUNDING_MODE_ZERO) ? "supported" : "not supported") << std::endl;
  out << '\t' << "HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR: "
      << ((baseRounding | HSA_DEFAULT_FLOAT_ROUNDING_MODE_NEAR) ? "supported" : "not supported") << std::endl;

  if (feature == HSA_AGENT_FEATURE_KERNEL_DISPATCH) {
    bool fastF16;
    CHECK_HSA_STATUS("hsa_agent_get_info failed",
                     hsa_agent_get_info(agent, HSA_AGENT_INFO_FAST_F16_OPERATION, &fastF16));
    out << "Fast f16 HSAIL operations: " << (fastF16 ? "supported" : "not supported") << std::endl;

    uint32_t wavesize;
    CHECK_HSA_STATUS("hsa_agent_get_info failed",
                     hsa_agent_get_info(agent, HSA_AGENT_INFO_WAVEFRONT_SIZE, &wavesize));
    out << "Number of work-items in a wavefront: " << wavesize << std::endl;

    uint16_t wgMaxDim[3];
    CHECK_HSA_STATUS("hsa_agent_get_info failed",
                     hsa_agent_get_info(agent, HSA_AGENT_INFO_WORKGROUP_MAX_DIM, wgMaxDim));
    out << "Maximum number of work-items in work-group: " << wgMaxDim[0] << "x" << wgMaxDim[1] << "x" << wgMaxDim[2] << std::endl;

    uint32_t wgMaxSize;
    CHECK_HSA_STATUS("hsa_agent_get_info failed",
                     hsa_agent_get_info(agent, HSA_AGENT_INFO_WORKGROUP_MAX_SIZE, &wgMaxSize));
    out << "Maximum total number of work-items in a work-group: " << wgMaxSize << std::endl;

    hsa_dim3_t gridMaxDim;
    CHECK_HSA_STATUS("hsa_agent_get_info failed",
                     hsa_agent_get_info(agent, HSA_AGENT_INFO_GRID_MAX_DIM, &gridMaxDim));
    out << "Maximum number of work-items in a grid: " << gridMaxDim.x << "x" << gridMaxDim.y << "x" << gridMaxDim.z << std::endl;

    uint32_t gridMaxSize;
    CHECK_HSA_STATUS("hsa_agent_get_info failed",
                     hsa_agent_get_info(agent, HSA_AGENT_INFO_GRID_MAX_SIZE, &gridMaxSize));
    out << "Maximum total number of work-items in a grid: " << gridMaxSize << std::endl;

    uint32_t fbarMax;
    CHECK_HSA_STATUS("hsa_agent_get_info failed",
                     hsa_agent_get_info(agent, HSA_AGENT_INFO_FBARRIER_MAX_SIZE, &fbarMax));
    out << "Maximum number of fbarriers per work-group: " << fbarMax << std::endl;
  }

  uint32_t queuesMax;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                    hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUES_MAX, &queuesMax));
  out << "Maximum number of queues that can be active at one time: " << queuesMax << std::endl;

  uint32_t queueMinSize;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                    hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MIN_SIZE, &queueMinSize));
  out << "Minimum number of packets that a queue can hold: " << queueMinSize << std::endl;

  uint32_t queueMaxSize;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                    hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queueMaxSize));
  out << "Maximum number of packets that a queue can hold: " << queueMaxSize << std::endl;

  hsa_queue_type_t queueType;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_TYPE, &queueType));
  out << "Type of a queue: "
      << (queueType == HSA_QUEUE_TYPE_MULTI ? "HSA_QUEUE_TYPE_MULTI " : "HSA_QUEUE_TYPE_SINGLE")
      << std::endl;

  uint32_t node;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                    hsa_agent_get_info(agent, HSA_AGENT_INFO_NODE, &node));
  out << "Identifier of the NUMA node: " << node << std::endl;

  hsa_device_type_t deviceType;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &deviceType));
  out << "Type of hardware device: ";
  switch (deviceType) {
    case HSA_DEVICE_TYPE_CPU:
      out << "HSA_DEVICE_TYPE_CPU"; break;
    case HSA_DEVICE_TYPE_GPU:
      out << "HSA_DEVICE_TYPE_GPU"; break;
    case HSA_DEVICE_TYPE_DSP:
      out << "HSA_DEVICE_TYPE_DSP"; break;
  default:
    HsaError("hsa_agent_get_info failed");
  }
  out << std::endl;

  uint32_t cacheSize[4];
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                    hsa_agent_get_info(agent, HSA_AGENT_INFO_CACHE_SIZE, &cacheSize));
  out << "Data cache sizes:" << std::endl;
  for (int i = 0; i < 4; ++i) {
    if (cacheSize[i] == 0) {
      continue;
    }
    out << '\t' << 'L' << i + 1 << ": " << cacheSize[i] << std::endl;
  }

  hsa_isa_t isa;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                    hsa_agent_get_info(agent, HSA_AGENT_INFO_ISA, &isa));
  out << "Instruction set architecture:" << std::endl;
  uint32_t isaNameLength;
  CHECK_HSA_STATUS("hsa_isa_get_info failed", hsa_isa_get_info(isa, HSA_ISA_INFO_NAME_LENGTH, 0, &isaNameLength));
  std::vector<char> isaName(isaNameLength, '\0');
  CHECK_HSA_STATUS("hsa_isa_get_info failed", hsa_isa_get_info(isa, HSA_ISA_INFO_NAME, 0, isaName.data()));
  isaName.resize(isaNameLength + 1);
  isaName.back() = '\0';
  out << '\t' << "Name: " << isaName.data() << std::endl;
  uint32_t isaConventionCount;
  CHECK_HSA_STATUS("hsa_isa_get_info failed", hsa_isa_get_info(isa, HSA_ISA_INFO_CALL_CONVENTION_COUNT, 0, &isaConventionCount));
  out << '\t' << "Number of call conventions: " << isaConventionCount << std::endl;
  for (uint32_t i = 0; i < isaConventionCount; ++i) {
    out << '\t' << "Convention " << i << ": " << std::endl;
    uint32_t isaWavesize;
    CHECK_HSA_STATUS("hsa_isa_get_info failed", hsa_isa_get_info(isa, HSA_ISA_INFO_CALL_CONVENTION_INFO_WAVEFRONT_SIZE, i, &isaWavesize));
    out << "\t\t" << "Number of work-items in a wavefront: " << isaWavesize << std::endl;
    uint32_t isaWavesCU;
    CHECK_HSA_STATUS("hsa_isa_get_info failed", hsa_isa_get_info(isa, HSA_ISA_INFO_CALL_CONVENTION_INFO_WAVEFRONTS_PER_COMPUTE_UNIT, i, &isaWavesCU));
    out << "\t\t" << "Number of wavefronts per compute: " << isaWavesCU << std::endl;
  }

  uint8_t extensionMask[128];
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                   hsa_agent_get_info(agent, HSA_AGENT_INFO_EXTENSIONS, extensionMask));
  out << "Extensions:" << std::endl;
  out << '\t' << "Finalizer: "
      << ((1<<0 | extensionMask[0]) ? "supported" : "not supported") << std::endl;
  out << '\t' << "Images: "
      << ((1<<1 | extensionMask[0]) ? "supported" : "not supported") << std::endl;

  uint16_t versionMajor;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                    hsa_agent_get_info(agent, HSA_AGENT_INFO_VERSION_MAJOR, &versionMajor));
  out << "Major version of the HSA runtime specification supported: " << versionMajor << std::endl;

  uint16_t versionMinor;
  CHECK_HSA_STATUS("hsa_agent_get_info failed",
                    hsa_agent_get_info(agent, HSA_AGENT_INFO_VERSION_MINOR, &versionMinor));
  out << "Minor version of the HSA runtime specification supported: " << versionMinor << std::endl;
}

void HsailRuntimeContext::PrintInfo(std::ostream& out)
{
  out << "--------------- System Info ---------------" << std::endl;
  PrintSystemInfo(out);
  hsa_agent_t* agents = Agents();
  for (uint32_t i = 0; i < AgentCount(); ++i) {
    out << std::endl << std::endl;
    out << "--------------- Agent " << i << " info ---------------" << std::endl;
    PrintAgentInfo(out, agents[i]);
  }
}

#undef CHECK_HSA_STATUS

bool RegionMatchKernarg(HsailRuntimeContext* runtime, hsa_region_t region)
{
  hsa_status_t status;
  hsa_region_global_flag_t flags;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
  if (status != HSA_STATUS_SUCCESS) { runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_GLOBAL_FLAGS) failed", status); return false; }
  if (!(flags & HSA_REGION_GLOBAL_FLAG_KERNARG)) { return false; }

  bool allocAllowed;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_RUNTIME_ALLOC_ALLOWED, &allocAllowed);
  if (status != HSA_STATUS_SUCCESS) { runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_SEGMENT) failed", status); return false; }
  if (!allocAllowed) { return false; }

  size_t granule;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_RUNTIME_ALLOC_GRANULE, &granule);
  if (status != HSA_STATUS_SUCCESS) { runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_RUNTIME_ALLOC_GRANULE) failed", status); return false; }
  // if (granule > sizeof(size_t)) { return false; }

  size_t maxSize;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_ALLOC_MAX_SIZE, &maxSize);
  if (status != HSA_STATUS_SUCCESS) {  runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_ALLOC_MAX_SIZE) failed", status); return false; }
  if (maxSize < 256) { return false; }
  return true;
}

bool RegionMatchSystem(HsailRuntimeContext* runtime, hsa_region_t region)
{
  hsa_status_t status;
  hsa_region_global_flag_t flags;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
  if (status != HSA_STATUS_SUCCESS) { runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_GLOBAL_FLAGS) failed", status); return false; }
  if (!(flags & HSA_REGION_GLOBAL_FLAG_FINE_GRAINED)) { return false; }

  hsa_region_segment_t segment;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment);
  if (status != HSA_STATUS_SUCCESS) { runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_SEGMENT) failed", status); return false; }
  if (segment != HSA_REGION_SEGMENT_GLOBAL) { return false; }

  bool allocAllowed;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_RUNTIME_ALLOC_ALLOWED, &allocAllowed);
  if (status != HSA_STATUS_SUCCESS) { runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_RUNTIME_ALLOC_ALLOWED) failed", status); return false; }
  if (!allocAllowed) { return false; }

  size_t granule;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_RUNTIME_ALLOC_GRANULE, &granule);
  if (status != HSA_STATUS_SUCCESS) { runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_RUNTIME_ALLOC_GRANULE) failed", status); return false; }
  // if (granule > sizeof(size_t)) { return false; }

  size_t maxSize;
  status = runtime->Hsa()->hsa_region_get_info(region, HSA_REGION_INFO_ALLOC_MAX_SIZE, &maxSize);
  if (status != HSA_STATUS_SUCCESS) { runtime->HsaError("hsa_region_get_info(HSA_REGION_INFO_ALLOC_MAX_SIZE) failed", status); return false; }
  if (maxSize < 256) { return false; }
  return true;
}

}

runtime::RuntimeContext* CreateHsailRuntimeContext(Context* context)
{
  return new hsail_runtime::HsailRuntimeContext(context);
}

  template <>
  void Print<hsa_queue_t>(const hsa_queue_t& tf, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState>(const hsail_runtime::HsailRuntimeContextState&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailBuffer>(const hsail_runtime::HsailRuntimeContextState::HsailBuffer&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailProgram>(const hsail_runtime::HsailRuntimeContextState::HsailProgram&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailExecutable>(const hsail_runtime::HsailRuntimeContextState::HsailExecutable&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailCode>(const hsail_runtime::HsailRuntimeContextState::HsailCode&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailDispatch>(const hsail_runtime::HsailRuntimeContextState::HsailDispatch&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailImage>(const hsail_runtime::HsailRuntimeContextState::HsailImage&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailSampler>(const hsail_runtime::HsailRuntimeContextState::HsailSampler&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailSignal>(const hsail_runtime::HsailRuntimeContextState::HsailSignal&, std::ostream& out) { }

  template <>
  void Print<hsail_runtime::HsailRuntimeContextState::HsailQueue>(const hsail_runtime::HsailRuntimeContextState::HsailQueue&, std::ostream& out) { }
}
