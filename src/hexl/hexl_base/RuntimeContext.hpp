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

#ifndef HEXL_RUNTIME_CONTEXT_HPP
#define HEXL_RUNTIME_CONTEXT_HPP

#include "RuntimeCommon.hpp"
#include "HexlContext.hpp"

namespace hexl {

  void *alignedMalloc(size_t size, size_t align);
  void alignedFree(void *ptr);

  runtime::RuntimeContext* CreateNoneRuntime(Context* context);

  template <>
  inline void Print(const runtime::RuntimeState& state, std::ostream& out) { state.Print(out); }
};

#endif // HEXL_RUNTIME_CONTEXT_HPP
