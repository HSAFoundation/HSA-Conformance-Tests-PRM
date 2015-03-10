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

#include "RuntimeContext.hpp"
#include "Options.hpp"
#include <stdlib.h>

namespace hexl {

void Dispatch::Apply(const DispatchSetup& setup)
{
  SetDimensions(setup.Dimensions());
  SetGridSize(setup.GridSize());
  SetWorkgroupSize(setup.WorkgroupSize());
  for (size_t i = 0; i < setup.MSetup().Count(); ++i) {
    MState()->Add(setup.MSetup().Get(i));
  }
}

void *alignedMalloc(size_t size, size_t align)
{
  assert((align & (align-1)) == 0);
  #if defined(_WIN32) || defined(_WIN64)
    return _aligned_malloc(size, alignment);
  #else
    void *ptr;
    int res = posix_memalign(&ptr, align, size);
    assert(res == 0);
    return ptr;
  #endif // _WIN32 || _WIN64
}

void *alignedFree(void *ptr)
{
  #if defined(_WIN32) || defined(_WIN64)
    _aligned_free(ptr);
  #else
    free(ptr);
  #endif
}

}
