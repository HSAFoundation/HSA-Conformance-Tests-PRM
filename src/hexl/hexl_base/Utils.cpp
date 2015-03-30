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

#include <sstream>
#include "Utils.hpp"
#include "HSAILItems.h"
#include "HSAILParser.h"
#include "HSAILValidator.h"
#include "HSAILBrigObjectFile.h"
#include "hsail_c.h"

using namespace HSAIL_ASM;

namespace hexl {

BrigContainer* BrigC(brig_container_t brig)
{
  BrigModuleHeader* brigm = static_cast<BrigModuleHeader*>(brig_container_get_brig_module(brig));
  return new BrigContainer(brigm);
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
    if (DirectiveModule v = d) {
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

hexl::ValueType Brig2ValueType(BrigType type)
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
  case BRIG_TYPE_F16: 
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    return MV_PLAIN_FLOAT16;
#else
    return MV_FLOAT16;
#endif
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
  case BRIG_TYPE_F16X2: return MV_FLOAT16X2;
  case BRIG_TYPE_F16X4: return MV_FLOAT16X4;

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

  case BRIG_TYPE_SIG32: return MV_UINT32;
  case BRIG_TYPE_SIG64: return MV_UINT64;

  default: assert(!"Unsupported type in Brig2ValueType"); return MV_LAST;
  }
}


BrigType Value2BrigType(hexl::ValueType type)
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
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
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
  case MV_FLOAT16X2: return BRIG_TYPE_F16X2;
  case MV_FLOAT16X4: return BRIG_TYPE_F16X4;
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

bool Is128Bit(BrigType type) {
  return HSAIL_ASM::getBrigTypeNumBytes(type) == 16;
}

std::string ExceptionsNumber2Str(uint32_t exceptionsNumber) {
  std::stringstream sstream;
  // 'v' - INVALID_OPERATION, 'd' - DIVIDE_BY_ZERO, 'o' - OVERFLOW, 'u' - underflow, 'e' - INEXACT
  if ((exceptionsNumber & 0x10) != 0) { sstream << 'e'; }
  if ((exceptionsNumber & 0x08) != 0) { sstream << 'u'; }
  if ((exceptionsNumber & 0x04) != 0) { sstream << 'o'; }
  if ((exceptionsNumber & 0x02) != 0) { sstream << 'd'; }
  if ((exceptionsNumber & 0x01) != 0) { sstream << 'v'; }
  if (exceptionsNumber == 0) { sstream << '0'; }
  return sstream.str();
}

uint32_t ImageGeometryDims(BrigImageGeometry geometry) {
  switch (geometry) {
  case BRIG_GEOMETRY_1D:       return 1;
  case BRIG_GEOMETRY_2D:       return 2;
  case BRIG_GEOMETRY_3D:       return 3;
  case BRIG_GEOMETRY_1DA:      return 1;
  case BRIG_GEOMETRY_2DA:      return 2;
  case BRIG_GEOMETRY_1DB:      return 1;
  case BRIG_GEOMETRY_2DDEPTH:  return 2;
  case BRIG_GEOMETRY_2DADEPTH: return 2;
  default:  assert(false); return 0;
  }
}

bool IsImageGeometryArray(BrigImageGeometry geometry) {
  if (geometry == BRIG_GEOMETRY_1DA ||
      geometry == BRIG_GEOMETRY_2DA ||
      geometry == BRIG_GEOMETRY_2DADEPTH) 
  {
    return true;
  } else {
    return false;
  }
}

bool IsImageDepth(BrigImageGeometry geometry) {
  if (geometry == BRIG_GEOMETRY_2DDEPTH || geometry == BRIG_GEOMETRY_2DADEPTH) {
    return true;
  } else {
    return false;
  }
}

bool IsImageQueryGeometrySupport(BrigImageGeometry imageGeometryProp, BrigImageQuery imageQuery)
{
  switch (imageGeometryProp)
  {
  case BRIG_GEOMETRY_1D:
  case BRIG_GEOMETRY_1DB:
      if ((imageQuery == BRIG_IMAGE_QUERY_HEIGHT) || (imageQuery == BRIG_IMAGE_QUERY_DEPTH) || (imageQuery == BRIG_IMAGE_QUERY_ARRAY))
        return false;
      break;
  case BRIG_GEOMETRY_1DA:
    if ((imageQuery == BRIG_IMAGE_QUERY_HEIGHT) || (imageQuery == BRIG_IMAGE_QUERY_DEPTH))
        return false;
      break;
  case BRIG_GEOMETRY_2D:
  case BRIG_GEOMETRY_2DDEPTH:
    if  ((imageQuery == BRIG_IMAGE_QUERY_DEPTH) || (imageQuery == BRIG_IMAGE_QUERY_ARRAY))
      return false;
    break;
  case BRIG_GEOMETRY_2DA:
  case BRIG_GEOMETRY_2DADEPTH:
     if  (imageQuery == BRIG_IMAGE_QUERY_DEPTH)
      return false;
    break;
  case BRIG_GEOMETRY_3D:
    if  (imageQuery == BRIG_IMAGE_QUERY_ARRAY)
      return false;
    break;
  default:
    break;
  }
  return true;
}

bool IsImageGeometrySupported(BrigImageGeometry imageGeometryProp, ImageGeometry imageGeometry) 
{
  switch (imageGeometryProp)
  {
  case BRIG_GEOMETRY_1D:
  case BRIG_GEOMETRY_1DB:
    if ((imageGeometry.ImageHeight() > 1) || (imageGeometry.ImageDepth() > 1) || (imageGeometry.ImageArray() > 1))
      return false;
    break;
  case BRIG_GEOMETRY_1DA:
    if ((imageGeometry.ImageHeight() > 1) || (imageGeometry.ImageDepth() > 1) || (imageGeometry.ImageArray() < 2))
      return false;
    break;
    case BRIG_GEOMETRY_2D:
    case BRIG_GEOMETRY_2DDEPTH:
    if ((imageGeometry.ImageHeight() < 2) || (imageGeometry.ImageDepth() > 1) || (imageGeometry.ImageArray() > 1))
      return false;
    break;
  case BRIG_GEOMETRY_2DA:
    if ((imageGeometry.ImageHeight() < 2) || (imageGeometry.ImageDepth() > 1) || (imageGeometry.ImageArray() < 2))
      return false;
    break;
  case BRIG_GEOMETRY_2DADEPTH:
    if (imageGeometry.ImageDepth() > 1)
      return false;
    break;
  case BRIG_GEOMETRY_3D:
    if ((imageGeometry.ImageHeight() < 2) || (imageGeometry.ImageDepth() < 2) || (imageGeometry.ImageArray() > 1))
      return false;
    break;
  default:
    if (imageGeometry.ImageArray() > 1)
      return false;
  }
  return true;
}

bool IsImageLegal(BrigImageGeometry geometry, BrigImageChannelOrder channelOrder, BrigImageChannelType channelType)
{
  switch (geometry)
  {
  case BRIG_GEOMETRY_1D:  case BRIG_GEOMETRY_2D:
  case BRIG_GEOMETRY_3D:  case BRIG_GEOMETRY_1DA:
  case BRIG_GEOMETRY_2DA: case BRIG_GEOMETRY_1DB:
    switch (channelOrder)
    {
    case BRIG_CHANNEL_ORDER_A:
    case BRIG_CHANNEL_ORDER_R:
    case BRIG_CHANNEL_ORDER_RX:
    case BRIG_CHANNEL_ORDER_RG:
    case BRIG_CHANNEL_ORDER_RGX:
    case BRIG_CHANNEL_ORDER_RA:
    case BRIG_CHANNEL_ORDER_RGBA:
      switch (channelType) {
      case BRIG_CHANNEL_TYPE_SNORM_INT8:   case BRIG_CHANNEL_TYPE_UNORM_INT8:
      case BRIG_CHANNEL_TYPE_SNORM_INT16:  case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:  case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16: case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32: case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:   case BRIG_CHANNEL_TYPE_FLOAT:
        return true;
      default:
        return false;
      }

    case BRIG_CHANNEL_ORDER_RGB:
    case BRIG_CHANNEL_ORDER_RGBX:
      switch (channelType) {
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_555:
      case BRIG_CHANNEL_TYPE_UNORM_SHORT_565:
      case BRIG_CHANNEL_TYPE_UNORM_INT_101010:
        return true;
      default:
        return false;
      }
    
    case BRIG_CHANNEL_ORDER_BGRA:
    case BRIG_CHANNEL_ORDER_ARGB:
    case BRIG_CHANNEL_ORDER_ABGR:
      switch (channelType) {
      case BRIG_CHANNEL_TYPE_SNORM_INT8: case BRIG_CHANNEL_TYPE_SIGNED_INT8:
      case BRIG_CHANNEL_TYPE_UNORM_INT8: case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
        return true;
      default:
        return false;
      }

    case BRIG_CHANNEL_ORDER_SRGB:
    case BRIG_CHANNEL_ORDER_SRGBX:
    case BRIG_CHANNEL_ORDER_SRGBA:
    case BRIG_CHANNEL_ORDER_SBGRA:
      return (channelType == BRIG_CHANNEL_TYPE_UNORM_INT8) ? true : false;

    case BRIG_CHANNEL_ORDER_INTENSITY:
    case BRIG_CHANNEL_ORDER_LUMINANCE:
      switch (channelType) {
      case BRIG_CHANNEL_TYPE_SNORM_INT8: case BRIG_CHANNEL_TYPE_SNORM_INT16:
      case BRIG_CHANNEL_TYPE_UNORM_INT8: case BRIG_CHANNEL_TYPE_UNORM_INT16:
      case BRIG_CHANNEL_TYPE_HALF_FLOAT: case BRIG_CHANNEL_TYPE_FLOAT:
        return true;
      default:
        return false;
      }
    
    default:
      return false;
    }

  case BRIG_GEOMETRY_2DDEPTH: case BRIG_GEOMETRY_2DADEPTH:
    switch (channelOrder)
    {
    case BRIG_CHANNEL_ORDER_DEPTH:
      switch (channelType) {
      case BRIG_CHANNEL_TYPE_UNORM_INT16: case BRIG_CHANNEL_TYPE_UNORM_INT24:
      case BRIG_CHANNEL_TYPE_FLOAT:
        return true;
      default:
        return false;
      }

    case BRIG_CHANNEL_ORDER_DEPTH_STENCIL:
      switch (channelType) {
      case BRIG_CHANNEL_TYPE_UNORM_INT24: case BRIG_CHANNEL_TYPE_FLOAT:
        return true;
      default:
        return false;
      }

    default: return false;
    }

  default:
    assert(0);
    break;
  }

  return false;
}

bool IsImageOptional(BrigImageGeometry geometry, BrigImageChannelOrder channelOrder, BrigImageChannelType channelType, BrigType accessPermission)
{
  bool bRO = (accessPermission == BRIG_TYPE_ROIMG) || (accessPermission == BRIG_TYPE_ROIMG_ARRAY);
  bool bRW = (accessPermission == BRIG_TYPE_RWIMG) || (accessPermission == BRIG_TYPE_RWIMG_ARRAY);
  bool bWO = (accessPermission == BRIG_TYPE_WOIMG) || (accessPermission == BRIG_TYPE_WOIMG_ARRAY);

  assert(bRO || bRW || bWO); //you should set one of the ro,wo,rw image access permission

  switch (geometry)
  {
  case BRIG_GEOMETRY_1D:
  case BRIG_GEOMETRY_2D:
  case BRIG_GEOMETRY_3D:
  case BRIG_GEOMETRY_1DA:
  case BRIG_GEOMETRY_2DA:
  case BRIG_GEOMETRY_1DB:
    switch (channelOrder) {
    case BRIG_CHANNEL_ORDER_R:
    case BRIG_CHANNEL_ORDER_RGBA:
    case BRIG_CHANNEL_ORDER_RG:
      if(bRW && (channelOrder == BRIG_CHANNEL_ORDER_RG))
        return true;
      if( ((channelType == BRIG_CHANNEL_TYPE_UNORM_INT16) ||
           (channelType == BRIG_CHANNEL_TYPE_SNORM_INT16) ||
           (channelType == BRIG_CHANNEL_TYPE_SNORM_INT8) ) && (bRO || bWO) )
           return false;
      switch (channelType) {
      case BRIG_CHANNEL_TYPE_UNORM_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT8:  case BRIG_CHANNEL_TYPE_UNSIGNED_INT8:
      case BRIG_CHANNEL_TYPE_SIGNED_INT16: case BRIG_CHANNEL_TYPE_UNSIGNED_INT16:
      case BRIG_CHANNEL_TYPE_SIGNED_INT32: case BRIG_CHANNEL_TYPE_UNSIGNED_INT32:
      case BRIG_CHANNEL_TYPE_HALF_FLOAT:   case BRIG_CHANNEL_TYPE_FLOAT:
        return false;
      default:
        return true;
      }

    case BRIG_CHANNEL_ORDER_BGRA:
      if( (bRO || bWO) && (channelType == BRIG_CHANNEL_TYPE_UNORM_INT8) )
        return false;
      return true;
    
    case BRIG_CHANNEL_ORDER_SRGBA:
      if(bRO && (channelType == BRIG_CHANNEL_TYPE_UNORM_INT8))
        return false;
      return true;
    
    default:
      return true;
    }

  case BRIG_GEOMETRY_2DDEPTH:
  case BRIG_GEOMETRY_2DADEPTH:
    if((channelOrder == BRIG_CHANNEL_ORDER_DEPTH) && (bRO || bWO) &&
       ((channelType == BRIG_CHANNEL_TYPE_UNORM_INT16) || (channelType == BRIG_CHANNEL_TYPE_FLOAT)) )
       return false;
    return true;
  
  default:
    return true;
  }

  return true;
}

bool IsSamplerLegal(BrigSamplerCoordNormalization coord, BrigSamplerFilter filter, BrigSamplerAddressing addressing)
{
  switch (coord)
  {
  case BRIG_COORD_UNNORMALIZED:
  case BRIG_COORD_NORMALIZED:
    break;
  default:
    return false;
  }

  switch (filter)
  {
  case BRIG_FILTER_NEAREST:
  case BRIG_FILTER_LINEAR:
    break;
  default:
    return false;
  }

  //see PRM Table 7-6 Image Instruction Combination
  switch (addressing)
  {
  case BRIG_ADDRESSING_UNDEFINED:
  case BRIG_ADDRESSING_CLAMP_TO_EDGE:
  case BRIG_ADDRESSING_CLAMP_TO_BORDER:
    return true;
  case BRIG_ADDRESSING_REPEAT:
  case BRIG_ADDRESSING_MIRRORED_REPEAT:
    return (coord == BRIG_COORD_NORMALIZED) ? true : false;
  default:
    return false;
  }

  assert(0);
  return false;
}

}
