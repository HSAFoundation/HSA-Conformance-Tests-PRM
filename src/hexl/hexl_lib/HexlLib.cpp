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

#include "HexlLib.hpp"
#ifdef ENABLE_HEXL_HSARUNTIMEOLD
#include "HSARuntimeContext.hpp"
#endif // ENABLE_HEXL_HSARUNTIMEOLD
#ifdef ENABLE_HEXL_HSARUNTIME
#include "HsailRuntime.hpp"
#endif // ENABLE_HEXL_HSARUNTIME
#ifdef ENABLE_HEXL_ORCA
#include "OrcaRuntime.hpp"
#endif // ENABLE_HEXL_ORCA


namespace hexl {

RuntimeContext* CreateRuntimeContext(Context* context) {
  const Options* options = context->Opts();
  RuntimeContext* runtime;
  std::string rt = options->GetString("rt", "hsa");
#ifdef ENABLE_HEXL_HSARUNTIMEOLD
  if (rt == "hsaold") {
    runtime = CreateHSARuntimeContext(context);
  } else
#endif // ENABLE_HEXL_HSARUNTIMEOLD
#ifdef ENABLE_HEXL_HSARUNTIME
  if (rt == "hsa") {
    runtime = CreateHsailRuntimeContext(context);
  } else
#endif // ENABLE_HEXL_HSARUNTIME
#ifdef ENABLE_HEXL_ORCA
  if (rt == "orca") {
    runtime = CreateOrcaRuntimeContext(context);
  } else
#endif // ENABLE_HEXL_ORCA
  {
    context->Env()->Error("Unsupported runtime: %s", rt.c_str());
    return 0;
  }
  if (!runtime->Init()) {
    context->Env()->Error("Failed to initialize runtime");
    return 0;
  }
  return runtime;
}

}
