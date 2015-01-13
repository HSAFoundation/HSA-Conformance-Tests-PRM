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

#ifndef HEXL_HSAIL_RUNTIME_HPP
#define HEXL_HSAIL_RUNTIME_HPP

#include "RuntimeContext.hpp"
#include "Options.hpp"
#include "DllApi.hpp"
#include "hsa.h"
#include "hsa_ext_finalize.h"

namespace hexl {

class EnvContext;

RuntimeContext* CreateHsailRuntimeContext(Context* context);

namespace hsail_runtime {

struct HsaApiTable {
  hsa_status_t (*hsa_init)();
  hsa_status_t (*hsa_shut_down)();
  hsa_status_t (*hsa_iterate_agents)(hsa_status_t(*callback)(hsa_agent_t agent, void *data), void *data);
  hsa_status_t (*hsa_agent_iterate_regions)(hsa_agent_t agent, hsa_status_t(*callback)(hsa_region_t region, void *data), void *data);
  hsa_status_t (*hsa_region_get_info)(hsa_region_t region, hsa_region_info_t attribute, void *value);
  hsa_status_t (*hsa_agent_get_info)(hsa_agent_t agent, hsa_agent_info_t attribute, void *value);
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
    hsa_agent_t *agents,
    uint32_t agent_count,
    hsa_ext_brig_machine_model8_t machine_model,
    hsa_ext_brig_profile8_t profile,
    hsa_ext_program_handle_t *program);
  hsa_status_t (*hsa_ext_program_destroy)(
    hsa_ext_program_handle_t program);
  hsa_status_t (*hsa_ext_add_module)(
    hsa_ext_program_handle_t program,
    hsa_ext_brig_module_t *brig_module,
    hsa_ext_brig_module_handle_t *module);
  hsa_status_t (*hsa_ext_finalize_program)(
    hsa_ext_program_handle_t program,
    hsa_agent_t agent,
    size_t finalization_request_count,
    hsa_ext_finalization_request_t *finalization_request_list,
    hsa_ext_control_directives_t *control_directives,
    hsa_ext_error_message_callback_t error_message_callback,
    uint8_t optimization_level,
    const char *options,
    int debug_information);
  hsa_status_t (*hsa_ext_query_kernel_descriptor_address)(
    hsa_ext_program_handle_t program,
    hsa_ext_brig_module_handle_t module,
    hsa_ext_brig_code_section_offset32_t symbol,
    hsa_ext_code_descriptor_t** kernel_descriptor);
};

class HsaApi : public DllApi<HsaApiTable> {
public:
  HsaApi(EnvContext* env, const Options* options, const char *libName)
    : DllApi<HsaApiTable>(env, options, libName) { }

  const HsaApiTable* InitApiTable();
};

class HsailRuntimeContext;

typedef bool (*RegionMatch)(HsailRuntimeContext* runtime, hsa_region_t region);

class HsailRuntimeContext : public RuntimeContext {
private:
  HsaApi hsaApi;
  hsa_agent_t agent;
  hsa_queue_t* queue;
  uint32_t queueSize;
  bool error;

public:
  HsailRuntimeContext(Context* context);
  ~HsailRuntimeContext() { Dispose(); }

  std::string Name() const { return "hsa"; }

  bool Init();
  void Dispose();

  std::string Description() const {
    return "HSA Foundation Runtime";
  }

  EnvContext* Env() { return context->Env(); }
  const Options* Opts() const { return context->Opts(); }
  virtual RuntimeContextState* NewState(Context* context);

  bool IsError() const { return error; }

  void HsaError(const char *msg, hsa_status_t err) {
    error = true;
    context->Error() << msg << ": error " << err << std::endl;
  }

  void HsaError(const char *msg) {
    error = true;
    context->Error() << msg << std::endl;
  }

  void hsailcError(const char *msg, brig_container_t brig, int status) {
    error = true;
    context->Error() << msg << ": error " << status << ": " << brig_container_get_error_text(brig) << std::endl;
  }

  hsa_agent_t Agent() { return agent; }
  hsa_agent_t* Agents() { return &agent; }
  uint32_t AgentCount() { return 1; }
  hsa_queue_t* Queue() { return queue; }
  uint32_t QueueSize() const { return queue->size; }
  const HsaApi& Hsa() const { return hsaApi; }
  hsa_region_t GetRegion(RegionMatch match = 0);
};

HsailRuntimeContext* HsailRuntimeFromContext(RuntimeContext* runtime);
const HsaApi& HsaApiFromContext(RuntimeContext* runtime);

}
};

#endif // HEXL_HSAIL_RUNTIME_HPP
