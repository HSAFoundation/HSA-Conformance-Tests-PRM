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

#include "Utils.hpp"
#include "HSAILItems.h"
#include "HSAILParser.h"
#include "HSAILValidator.h"
#include "HSAILBrigObjectFile.h"
#include "hsail_c.h"

using namespace Brig;
using namespace HSAIL_ASM;

namespace hexl {

BrigContainer* BrigC(brig_container_t brig)
{
  return new BrigContainer(
    (char *) brig_container_get_section_bytes(brig, BRIG_SECTION_INDEX_DATA),
    (char *) brig_container_get_section_bytes(brig, BRIG_SECTION_INDEX_CODE),
    (char *) brig_container_get_section_bytes(brig, BRIG_SECTION_INDEX_OPERAND),
    0);
}

brig_container_t CreateBrigFromContainer(HSAIL_ASM::BrigContainer* container)
{
  return brig_container_create_copy(
    container->sectionById(BRIG_SECTION_INDEX_DATA).getData(0),
    container->sectionById(BRIG_SECTION_INDEX_CODE).getData(0),
    container->sectionById(BRIG_SECTION_INDEX_OPERAND).getData(0),
    0
  );
}


BrigMachineModel8_t GetBrigMachineModel(brig_container_t brig)
{
  BrigContainer* brigc = BrigC(brig);
  for (Code d = brigc->code().begin(), e = brigc->code().end(); d != e; ) {
    if (DirectiveVersion v = d) {
      BrigMachineModel8_t model = v.machineModel().enumValue();
      delete brigc;
      return model;
    }
    if (DirectiveExecutable exec = d) {
      d = exec.nextModuleEntry();
    } else {
      d = d.next();
    }
  }
  delete brigc;
  return BRIG_MACHINE_UNDEF;
}

BrigCodeOffset32_t GetBrigUniqueKernelOffset(brig_container_t brig)
{
  BrigCodeOffset32_t uniqueKernelOffset = 0;
  BrigContainer* brigc = BrigC(brig);

  DirectiveKernel k;
  for (Code d = brigc->code().begin(), e = brigc->code().end(); d != e; ) {
    k = d;
    if (k) {
      if (!uniqueKernelOffset) {
        uniqueKernelOffset = k.brigOffset();
      } else {
        // More than one kernel found.
        uniqueKernelOffset = 0;
        break;
      }
    }
    if (DirectiveExecutable exec = d) {
      d = exec.nextModuleEntry();
    } else {
      d = d.next();
    }
  }
  delete brigc;
  return uniqueKernelOffset;
}

std::string GetBrigKernelName(brig_container_t brig, BrigCodeOffset32_t kernelOffset)
{
  BrigContainer* brigc = BrigC(brig);
  DirectiveKernel kernel(brigc, kernelOffset);
  assert(kernel != 0);
  std::string kernelName = kernel.name().str();
  delete brigc;
  return kernelName;
}

unsigned GetBrigKernelInArgCount(brig_container_t brig, BrigCodeOffset32_t kernelOffset)
{
  BrigContainer* brigc = BrigC(brig);
  DirectiveKernel k(brigc, kernelOffset);
  assert(k != 0);
  unsigned res = k.inArgCount();
  delete brigc;
  return res;
}

std::string ExtractTestPath(const std::string& name, unsigned level)
{
  size_t pos = 0;
  for (unsigned i = 0; i < level; ++i) {
    pos = name.find_first_of('/', pos + 1);
    if (pos == std::string::npos) { break; }
  }
  return name.substr(0, pos);
}

hexl::ValueType Brig2ValueType(BrigTypeX type)
{
  switch (type) {
  case BRIG_TYPE_B8:
  case BRIG_TYPE_U8: return MV_UINT8;
  case BRIG_TYPE_S8: return MV_INT8;
  case BRIG_TYPE_B16:
  case BRIG_TYPE_U16: return MV_UINT16;
  case BRIG_TYPE_S16: return MV_INT16;
  case BRIG_TYPE_B32:
  case BRIG_TYPE_U32: return MV_UINT32;
  case BRIG_TYPE_S32: return MV_INT32;
  case BRIG_TYPE_B64:
  case BRIG_TYPE_U64: return MV_UINT64;
  case BRIG_TYPE_S64: return MV_INT64;
  case BRIG_TYPE_F16: return MV_FLOAT16;
  case BRIG_TYPE_F32: return MV_FLOAT;
  case BRIG_TYPE_F64: return MV_DOUBLE;
  case BRIG_TYPE_U8X4: return MV_UINT8X4;
  case BRIG_TYPE_U8X8: return MV_UINT8X8;
  case BRIG_TYPE_S8X4: return MV_INT8X4;
  case BRIG_TYPE_S8X8: return MV_INT8X8;
  case BRIG_TYPE_U16X2: return MV_UINT16X2;
  case BRIG_TYPE_U16X4: return MV_UINT16X4;
  case BRIG_TYPE_S16X2: return MV_INT16X2;
  case BRIG_TYPE_S16X4: return MV_INT16X4;
  case BRIG_TYPE_U32X2: return MV_UINT32X2;
  case BRIG_TYPE_S32X2: return MV_INT32X2;
  case BRIG_TYPE_F32X2: return MV_FLOATX2;

  case BRIG_TYPE_U8X16: return MV_UINT8X8;
  case BRIG_TYPE_U16X8: return MV_UINT16X4;
  case BRIG_TYPE_U32X4: return MV_UINT32X2;
  case BRIG_TYPE_U64X2: return MV_UINT64;
  case BRIG_TYPE_S8X16: return MV_INT8X8;
  case BRIG_TYPE_S16X8: return MV_INT16X4;
  case BRIG_TYPE_S32X4: return MV_INT32X2;
  case BRIG_TYPE_S64X2: return MV_INT64;
  case BRIG_TYPE_F32X4: return MV_FLOATX2;
  case BRIG_TYPE_F64X2: return MV_DOUBLE;

  default: assert(!"Unsupported type in Brig2ValueType"); return MV_LAST;
  }
}


Brig::BrigTypeX Value2BrigType(hexl::ValueType type)
{
  switch (type) {
  case MV_UINT8: return BRIG_TYPE_U8;
  case MV_INT8: return BRIG_TYPE_S8;
  case MV_UINT16: return BRIG_TYPE_U16;
  case MV_INT16: return BRIG_TYPE_S16;
  case MV_UINT32: return BRIG_TYPE_U32;
  case MV_INT32: return BRIG_TYPE_S32;
  case MV_UINT64: return BRIG_TYPE_U64;
  case MV_INT64: return BRIG_TYPE_S64;
  case MV_FLOAT16: return BRIG_TYPE_F16;
  case MV_FLOAT: return BRIG_TYPE_F32;
  case MV_DOUBLE: return BRIG_TYPE_F64;
  case MV_UINT8X4: return BRIG_TYPE_U8X4; 
  case MV_UINT8X8: return BRIG_TYPE_U8X8; 
  case MV_INT8X4: return BRIG_TYPE_S8X4; 
  case MV_INT8X8: return BRIG_TYPE_S8X8;
  case MV_UINT16X2: return BRIG_TYPE_U16X2;
  case MV_UINT16X4: return BRIG_TYPE_U16X4;
  case MV_INT16X2: return BRIG_TYPE_S16X2;
  case MV_INT16X4: return BRIG_TYPE_S16X4;
  case MV_UINT32X2: return BRIG_TYPE_U32X2;
  case MV_INT32X2: return BRIG_TYPE_S32X2;
  case MV_FLOATX2: return BRIG_TYPE_F32X2;
  case MV_EXPR:
    assert(false); return BRIG_TYPE_NONE;
/*
    switch (SpecialValueSize(value.U64())) {
    case 4: return BRIG_TYPE_U32;
    case 8: return BRIG_TYPE_U64;
    default: assert(false); return BRIG_TYPE_NONE;
    }
*/
  default: assert(false); return BRIG_TYPE_NONE;
  }
}

bool Is128Bit(BrigTypeX type) {
  return HSAIL_ASM::getBrigTypeNumBytes(type) == 16;
}


}
