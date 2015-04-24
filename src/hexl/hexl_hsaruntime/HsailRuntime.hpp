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

#ifndef HEXL_HSAIL_RUNTIME_HPP
#define HEXL_HSAIL_RUNTIME_HPP

#include "RuntimeCommon.hpp"
#include "Options.hpp"
#include "DllApi.hpp"
#include "hsa.h"
#include "hsa_ext_finalize.h"
#include "hsa_ext_image.h"
#include "hsail_c.h"
#include <functional>

namespace hexl {

class EnvContext;

runtime::RuntimeContext* CreateHsailRuntimeContext(Context* context);

namespace hsail_runtime {

struct HsaApiTable {
  hsa_status_t (*hsa_status_string)(hsa_status_t status, const char **status_string);
  hsa_status_t (*hsa_init)();
  hsa_status_t (*hsa_shut_down)();
  hsa_status_t (*hsa_iterate_agents)(hsa_status_t(*callback)(hsa_agent_t agent, void *data), void *data);
  hsa_status_t (*hsa_agent_iterate_regions)(hsa_agent_t agent, hsa_status_t(*callback)(hsa_region_t region, void *data), void *data);
  hsa_status_t (*hsa_system_get_info)(hsa_system_info_t attribute, void *value);
  hsa_status_t (*hsa_region_get_info)(hsa_region_t region, hsa_region_info_t attribute, void *value);
  hsa_status_t (*hsa_agent_get_info)(hsa_agent_t agent, hsa_agent_info_t attribute, void *value);
  hsa_status_t (*hsa_agent_get_exception_policies)(hsa_agent_t agent, hsa_profile_t profile, uint16_t *mask);
  hsa_status_t (*hsa_queue_create)(hsa_agent_t agent, size_t size, hsa_queue_type_t type, void (*callback)(hsa_status_t status, hsa_queue_t *queue, void *data), void *data, uint32_t private_segment_size, uint32_t group_segment_size, hsa_queue_t **queue);
  hsa_status_t (*hsa_queue_destroy)(hsa_queue_t *queue);
  uint64_t (*hsa_queue_load_write_index_relaxed)(hsa_queue_t *queue);
  void (*hsa_queue_store_write_index_relaxed)(hsa_queue_t *queue, uint64_t value);
  uint64_t (*hsa_queue_add_write_index_relaxed)(hsa_queue_t *queue, uint64_t value);
  hsa_status_t (*hsa_memory_allocate)(hsa_region_t region, size_t size_bytes, void** address);
  hsa_status_t (*hsa_memory_free)(void* ptr);
  hsa_status_t (*hsa_memory_register)(void* address, size_t size);
  hsa_status_t (*hsa_memory_deregister)(void* address);
  hsa_status_t (*hsa_signal_create)(hsa_signal_value_t initial_value, uint32_t num_consumers, const hsa_agent_t* consumers, hsa_signal_t* signal);
  hsa_status_t (*hsa_signal_destroy)(hsa_signal_t signal_handle);
  void (*hsa_signal_store_relaxed)(hsa_signal_t signal_handle, hsa_signal_value_t signal_value);
  void (*hsa_signal_store_release)(hsa_signal_t signal, hsa_signal_value_t value);
  hsa_signal_value_t (*hsa_signal_wait_acquire)(hsa_signal_t signal, hsa_signal_condition_t condition, hsa_signal_value_t compare_value, uint64_t timeout_hint, hsa_wait_state_t wait_expectancy_hint);
  hsa_status_t (*hsa_ext_program_create)(
    hsa_machine_model_t machine_model,
    hsa_profile_t profile,
    hsa_default_float_rounding_mode_t default_float_rounding_mode,
    const char *options,
    hsa_ext_program_t *program);
  hsa_status_t (*hsa_ext_program_destroy)(
    hsa_ext_program_t program);
  hsa_status_t (*hsa_ext_program_add_module)(
    hsa_ext_program_t program,
    hsa_ext_module_t module);
  hsa_status_t (*hsa_ext_program_finalize)(
    hsa_ext_program_t program,
    hsa_isa_t isa,
    int32_t call_convention,
    hsa_ext_control_directives_t control_directives,
    const char *options,
    hsa_code_object_type_t code_object_type,
    hsa_code_object_t *code_object);
  hsa_status_t (*hsa_ext_program_get_info)(
    hsa_ext_program_t program,
    hsa_ext_program_info_t attribute,
    void *value);

  hsa_status_t (*hsa_executable_create)(
    hsa_profile_t profile,
    hsa_executable_state_t executable_state,
    const char *options,
    hsa_executable_t *executable);
  hsa_status_t (*hsa_executable_load_code_object)(
    hsa_executable_t executable,
    hsa_agent_t agent,
    hsa_code_object_t code_object,
    const char *options);
  hsa_status_t (*hsa_code_object_destroy)(
    hsa_code_object_t code_object);
  hsa_status_t (*hsa_executable_symbol_get_info)(
    hsa_executable_symbol_t executable_symbol,
    hsa_executable_symbol_info_t attribute,
    void *value);
  hsa_status_t (*hsa_executable_get_symbol)(
    hsa_executable_t executable,
    const char *module_name,
    const char *symbol_name,
    hsa_agent_t agent,
    int32_t call_convention,
    hsa_executable_symbol_t *symbol);
  hsa_status_t (*hsa_executable_iterate_symbols)(
    hsa_executable_t executable,
    hsa_status_t (*callback)(hsa_executable_t executable, hsa_executable_symbol_t symbol, void* data),
    void* data);
  hsa_status_t (*hsa_executable_freeze)(
    hsa_executable_t executable,
    const char *options);
  hsa_status_t (*hsa_executable_destroy)(
    hsa_executable_t executable);

  hsa_status_t (*hsa_ext_image_data_get_info)(hsa_agent_t agent, 
    const hsa_ext_image_descriptor_t *image_descriptor,
    hsa_access_permission_t access_permission,
    hsa_ext_image_data_info_t *image_data_info);
  hsa_status_t (*hsa_ext_image_create)(hsa_agent_t agent,
    const hsa_ext_image_descriptor_t *image_descriptor,
    const void *image_data,
    hsa_access_permission_t access_permission,
    hsa_ext_image_t *image);
  hsa_status_t (*hsa_ext_image_destroy)(hsa_agent_t agent, hsa_ext_image_t image);
  hsa_status_t (*hsa_ext_sampler_create)(hsa_agent_t agent, 
    const hsa_ext_sampler_descriptor_t *sampler_descriptor,
    hsa_ext_sampler_t *sampler);
  hsa_status_t (*hsa_ext_sampler_destroy)(hsa_agent_t agent, hsa_ext_sampler_t sampler);
  hsa_status_t (*hsa_ext_image_import)(hsa_agent_t agent, const void *src_memory,
                         size_t src_row_pitch, size_t src_slice_pitch,
                         hsa_ext_image_t dst_image,
                         const hsa_ext_image_region_t *image_region);
  hsa_status_t (*hsa_ext_image_get_capability)(hsa_agent_t agent, hsa_ext_image_geometry_t geometry, 
                         const hsa_ext_image_format_t* format,
                         uint32_t* capability_mask);
};

class HsaApi : public DllApi<HsaApiTable> {
public:
  HsaApi(Context* context, const Options* options, const char *libName)
    : DllApi<HsaApiTable>(context, options, libName) { }

  const HsaApiTable* InitApiTable();
};

class HsailRuntimeContext;

typedef std::function<bool(HsailRuntimeContext*, hsa_region_t)> RegionMatch;

class HsailRuntimeContext : public runtime::RuntimeContext {
private:
  HsaApi hsaApi;
  hsa_agent_t agent;
  hsa_queue_t* queue;
  uint32_t queueSize;
  volatile bool queueError;
  hsa_profile_t profile;
  uint32_t wavesize;
  uint32_t wavesPerGroup;
  hsa_endianness_t endianness;

  bool QueueInit();
  void QueueDestroy();

public:
  HsailRuntimeContext(Context* context);
  ~HsailRuntimeContext() { Dispose(); }

  std::string Name() const { return "hsa"; }

  bool Init();
  void Dispose();

  std::string Description() const {
    return "HSA Foundation Runtime";
  }

  const Options* Opts() const { return context->Opts(); }
  virtual runtime::RuntimeState* NewState(Context* context);

  void HsaError(const char *msg, hsa_status_t err) {
    const char *hsamsg = "";
    if (Hsa()->hsa_status_string) {
      Hsa()->hsa_status_string(err, &hsamsg);
    }
    context->Error() << msg << ": error " << err << ": " << hsamsg << std::endl;
  }

  void HsaError(const char *msg) {
    context->Error() << msg << std::endl;
  }

  void hsailcError(const char *msg, brig_container_t brig, int status) {
    context->Error() << msg << ": error " << status << ": " << brig_container_get_error_text(brig) << std::endl;
  }

  hsa_agent_t Agent() { return agent; }
  hsa_agent_t* Agents() { return &agent; }
  uint32_t AgentCount() { return 1; }
  hsa_queue_t* Queue() { return queue; }
  hsa_queue_t* QueueNoError();
  void QueueError(hsa_status_t status);
  bool IsQueueError() const { return queueError; }

  uint32_t QueueSize() const { return queue->size; }
  const HsaApi& Hsa() const { return hsaApi; }
  hsa_region_t GetRegion(RegionMatch match = 0);

  bool IsFullProfile() override { return profile == HSA_PROFILE_FULL; }
  uint32_t Wavesize() override { return wavesize; }
  uint32_t WavesPerGroup() override { return wavesPerGroup; }
  bool IsLittleEndianness() override { return endianness == HSA_ENDIANNESS_LITTLE; }
};

HsailRuntimeContext* HsailRuntimeFromContext(runtime::RuntimeContext* runtime);
const HsaApi& HsaApiFromContext(runtime::RuntimeContext* runtime);

}

};

#endif // HEXL_HSAIL_RUNTIME_HPP
