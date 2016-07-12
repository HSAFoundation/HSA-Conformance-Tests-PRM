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

#include "MObject.hpp"
#include "Utils.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace hexl {

template <typename T> inline T FlushDenorms(const T& x) { return x.isSubnormal() ? T().copySign(x) : x; }

const uint32_t MBuffer::default_size[3] = {1,1,1};

Value::~Value()
{
}

unsigned SpecialValueSize(uint64_t id)
{
  switch (id) {
  case RV_QUEUEID: return 4;
  case RV_QUEUEPTR: return 8;
  default: assert(false); return 0;
  }
}

size_t ValueTypeSize(ValueType type)
{
  switch (type) {
  case MV_INT8: return 1;
  case MV_UINT8: return 1;
  case MV_INT16: return 2;
  case MV_UINT16: return 2;
  case MV_INT32: return 4;
  case MV_UINT32: return 4;
  case MV_INT64: return 8;
  case MV_UINT64: return 8;
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16: return 4;
#endif
  case MV_FLOAT16: return 2;
  case MV_FLOAT: return 4;
  case MV_DOUBLE: return 8;
  case MV_INT8X4: return 4;
  case MV_INT8X8: return 8;
  case MV_UINT8X4: return 4;
  case MV_UINT8X8: return 8;
  case MV_INT16X2: return 4;
  case MV_INT16X4: return 8;
  case MV_UINT16X2: return 4;
  case MV_UINT16X4: return 8;
  case MV_INT32X2: return 8;
  case MV_UINT32X2: return 8;
  case MV_FLOAT16X2: return 4;
  case MV_FLOAT16X4: return 8;
  case MV_FLOATX2: return 8;
  case MV_UINT128: return 16;
  case MV_IMAGE:
    return sizeof(void *);
  case MV_REF:
    return 4;
  case MV_IMAGEREF:
    return 8;
  case MV_SAMPLERREF:
    return 8;
  case MV_POINTER:
    return sizeof(void *);
  case MV_EXPR:
    assert(false); return 0;
  default:
    assert(false); return 0;
  }
}


ValueData U8X4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { 
  std::vector<uint8_t> vector(4);   
  vector[0] = a; vector[1] = b; vector[2] = c; vector[3] = d;
  ValueData data; 
  data.u32 = *reinterpret_cast<uint32_t *>(vector.data());
  return data; 
}

ValueData U8X8(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g, uint8_t h) { 
  std::vector<uint8_t> vector(8);   
  vector[0] = a; vector[1] = b; vector[2] = c; vector[3] = d; vector[4] = e; vector[5] = f; vector[6] = g; vector[7] = h;
  ValueData data; 
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data; 
}

ValueData S8X4(int8_t a, int8_t b, int8_t c, int8_t d) { 
  std::vector<int8_t> vector(4);   
  vector[0] = a; vector[1] = b; vector[2] = c; vector[3] = d;
  ValueData data; 
  data.u32 = *reinterpret_cast<uint32_t *>(vector.data());
  return data; 
}

ValueData S8X8(int8_t a, int8_t b, int8_t c, int8_t d, int8_t e, int8_t f, int8_t g, int8_t h) { 
  std::vector<int8_t> vector(8);   
  vector[0] = a; vector[1] = b; vector[2] = c; vector[3] = d; vector[4] = e; vector[5] = f; vector[6] = g; vector[7] = h;
  ValueData data; 
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data; 
}

ValueData U16X2(uint16_t a, uint16_t b) { 
  std::vector<uint16_t> vector(2);   
  vector[0] = a; vector[1] = b;
  ValueData data; 
  data.u32 = *reinterpret_cast<uint32_t *>(vector.data());
  return data;
}

ValueData U16X4(uint16_t a, uint16_t b, uint16_t c, uint16_t d) { 
  std::vector<uint16_t> vector(4);   
  vector[0] = a; vector[1] = b; vector[2] = c; vector[3] = d;
  ValueData data; 
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data;  
}

ValueData S16X2(int16_t a, int16_t b) { 
  std::vector<int16_t> vector(2);   
  vector[0] = a; vector[1] = b;
  ValueData data; 
  data.u32 = *reinterpret_cast<uint32_t *>(vector.data());
  return data;
}

ValueData S16X4(int16_t a, int16_t b, int16_t c, int16_t d) { 
  std::vector<int16_t> vector(4);   
  vector[0] = a; vector[1] = b; vector[2] = c; vector[3] = d;
  ValueData data; 
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data; 
}

ValueData U32X2(uint32_t a, uint32_t b) { 
  std::vector<uint32_t> vector(2);   
  vector[0] = a; vector[1] = b;
  ValueData data; 
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data;
}

ValueData S32X2(int32_t a, int32_t b) { 
  std::vector<int32_t> vector(2);   
  vector[0] = a; vector[1] = b;
  ValueData data; 
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data;
}

ValueData U64X2(uint64_t a, uint64_t b) {
  ValueData data; 
  data.u128.h = a;
  data.u128.l = b;
  return data;
}

ValueData FX2(float a, float b) { 
  std::vector<float> vector(2);   
  vector[0] = a; vector[1] = b;
  ValueData data; 
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data;
}

ValueData HX2(half a, half b) {
  std::vector<half::bits_t> vector(2);
  vector[0] = a.rawBits(); vector[1] = b.rawBits();
  ValueData data;
  data.u32 = *reinterpret_cast<uint32_t *>(vector.data());
  return data;
}

ValueData HX4(half a, half b, half c, half d) {
  std::vector<half::bits_t> vector(4);
  vector[0] = a.rawBits(); vector[1] = b.rawBits(); vector[2] = c.rawBits(); vector[3] = d.rawBits();
  ValueData data;
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data;
}

const char *MemString(MObjectMem mem)
{
  switch (mem) {
  case MEM_KERNARG: return "kernarg";
  case MEM_GLOBAL: return "global";
  case MEM_IMAGE: return "image";
  case MEM_GROUP: return "group";
  default: assert(false); return "<unknown mem>";
  }
}

const char *ImageGeometryString(MObjectImageGeometry mem)
{
  switch (mem) {
  case IMG_1D: return "1d";
  case IMG_2D: return "2d";
  case IMG_3D: return "3d";
  case IMG_1DA: return "1da";
  case IMG_2DA: return "2da";
  case IMG_1DB: return "1db";
  case IMG_2DDEPTH: return "2ddepth";
  case IMG_2DADEPTH: return "2dadepth";
  default: assert(false); return "<unknown geometry>";
  }
}

const char *ImageChannelTypeString(MObjectImageChannelType mem)
{
  switch (mem) {
  case IMG_SNORM_INT8: return "snorm_int8";
  case IMG_SNORM_INT16: return "snorm_int16";
  case IMG_UNORM_INT8: return "unorm_int8";
  case IMG_UNORM_INT16: return "unorm_int16";
  case IMG_UNORM_INT24: return "unorm_int24";
  case IMG_UNORM_SHORT_555: return "unorm_short_555";
  case IMG_UNORM_SHORT_565: return "unorm_short_565";
  case IMG_UNORM_INT_101010: return "unorm_int_101010";
  case IMG_SIGNED_INT8: return "signed_int8";
  case IMG_SIGNED_INT16: return "signed_int16";
  case IMG_SIGNED_INT32: return "signed_int32";
  case IMG_UNSIGNED_INT8: return "unsigned_int8";
  case IMG_UNSIGNED_INT16: return "unsigned_int16";
  case IMG_UNSIGNED_INT32: return "unsigned_int32";
  case IMG_HALF_FLOAT: return "half_float";
  case IMG_FLOAT: return "float";
  default: assert(false); return "<unknown channel type>";
  }
}

const char *ImageChannelOrderString(MObjectImageChannelOrder mem)
{
  switch (mem) {
  case IMG_ORDER_A: return "a";
  case IMG_ORDER_R: return "r";
  case IMG_ORDER_RX: return "rx";
  case IMG_ORDER_RG: return "rg";
  case IMG_ORDER_RGX: return "rgx";
  case IMG_ORDER_RA: return "ra";
  case IMG_ORDER_RGB: return "rgb";
  case IMG_ORDER_RGBX: return "rgbx";
  case IMG_ORDER_RGBA: return "rgba";
  case IMG_ORDER_BRGA: return "bgra";
  case IMG_ORDER_ARGB: return "argb";
  case IMG_ORDER_ABGR: return "abgr";
  case IMG_ORDER_SRGB: return "srgb";
  case IMG_ORDER_SRGBX: return "srgbx";
  case IMG_ORDER_SRGBA: return "srgba";
  case IMG_ORDER_SBGRA: return "sbgra";
  case IMG_ORDER_INTENSITY: return "intensity";
  case IMG_ORDER_LUMINANCE: return "luminance";
  case IMG_ORDER_DEPTH: return "depth";
  case IMG_ORDER_DEPTH_STENCIL: return "depth_stencil";
  default: assert(false); return "<unknown channel order>";
  }
}

const char *ImageQueryString(MObjectImageQuery mem)
{
  switch (mem)
  {
  case IMG_QUERY_WIDTH: return "query_width";
  case IMG_QUERY_HEIGHT: return "query_height";
  case IMG_QUERY_DEPTH: return "query_depth";
  case IMG_QUERY_ARRAY: return "query_array";
  case IMG_QUERY_CHANNELORDER: return "query_channel_order";
  case IMG_QUERY_CHANNELTYPE: return "query_channel_type";
  default: assert(false); return "<unknown image query type>";
  }
};


const char *SamplerQueryString(MObjectSamplerQuery mem)
{
  switch (mem)
  {
  case SMP_QUERY_ADDRESSING: return "query_addressing";
  case SMP_QUERY_COORD: return "query_coord";
  case SMP_QUERY_FILTER: return "query_filter"; 
  default: assert(false); return "<unknown sampler query type>";
  }

}

/// For X2/X4/X8 types - returns width per element
size_t ValueTypePrintWidth(ValueType type)
{
  switch (type) {
  case MV_INT8X4:
  case MV_INT8X8:
  case MV_INT8: return ValueTypePrintWidth(MV_UINT8)+1;
  case MV_UINT8X4:
  case MV_UINT8X8:
  case MV_UINT8: return std::strlen("255");
  case MV_INT16X2:
  case MV_INT16X4:
  case MV_INT16: return ValueTypePrintWidth(MV_UINT16)+1;
  case MV_UINT16X2:
  case MV_UINT16X4:
  case MV_UINT16: return std::strlen("65535");
  case MV_INT32X2: 
  case MV_INT32: return ValueTypePrintWidth(MV_UINT32)+1;
  case MV_UINT32X2:
  case MV_UINT32: return std::strlen("4294967295");
  case MV_UINT128: return ValueTypePrintWidth(MV_UINT64)*2 + 6;
  case MV_INT64: return ValueTypePrintWidth(MV_UINT64)+1;
  case MV_UINT64: return std::strlen("9223372036854775807");
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16X2:
  case MV_FLOAT16X4:
  case MV_FLOAT16: // return 8;
    return Comparison::F16_MAX_DECIMAL_PRECISION + std::strlen("+.-E12");
  case MV_FLOATX2:
  case MV_FLOAT: // return 10;
    return Comparison::F32_MAX_DECIMAL_PRECISION + std::strlen("+.-E123");
  case MV_DOUBLE: // return 18;
    return Comparison::F64_MAX_DECIMAL_PRECISION + std::strlen("+.-E1234");
  case MV_IMAGE:
  case MV_REF:
  case MV_IMAGEREF:
  case MV_POINTER:
  case MV_EXPR:
  case MV_STRING:
    return 0;
  default:
    assert(false); return 0;
  }
}


size_t Value::Size() const
{
  return ValueTypeSize(type);
}

size_t Value::PrintWidth() const
{
  return ValueTypePrintWidth(type);
}

std::ostream& operator<<(std::ostream& out, const Value& v)
{
  v.Print(out);
  return out;
}

static
void PrintExtraHex(std::ostream& out, uint64_t bits /* zero-extended */, size_t sizeof_bits) {
  const std::streamsize wSave = out.width(); /// \todo get rid of width save/restore? --artem
  out << " (0x" << std::hex << std::setw(sizeof_bits * 2) << std::setfill('0') << bits << ")";
  out << std::setfill(' ') << std::dec << std::setw(wSave);
}

template<typename T>
static
std::ostream& PrintFloating(const typename T::bits_t bits, std::ostream& out, const bool extraHex){
  const typename T::props_t props(bits);
  if (props.isNan() || props.isInf()) {
    if (!props.isNegative()) out << "+"; else out << "-";
    if (props.isInf()) {
      out << "INF";
    } else {
      if (props.isSignalingNan()) out << "s"; else out << "q";
      out << "NAN(" << props.getNanPayload() << ")";
    }
  } else {
    const int precision = (sizeof(bits) == 8) ? Comparison::F64_MAX_DECIMAL_PRECISION
                        : (sizeof(bits) == 2) ? Comparison::F16_MAX_DECIMAL_PRECISION
                        : Comparison::F32_MAX_DECIMAL_PRECISION;
    out << std::setprecision(precision) << T(props).floatValue();
  }
  if (extraHex) PrintExtraHex(out, bits, sizeof(bits));
  return out;
};

void Value::Print(std::ostream& out) const
{
  switch (type) {
  case MV_INT8:
    out << (int) S8();
    break;
  case MV_UINT8:
    out << (unsigned int) U8();
    break;
  case MV_INT16:
    out << (int) S16();
    break;
  case MV_UINT16:
    out << (unsigned int) U16();
    break;
  case MV_INT32:
    out << S32();
    break;
  case MV_UINT32:
    out << U32();
    break;
  case MV_INT64:
    out << S64();
    break;
  case MV_UINT64:
    out << U64();
    break;
   case MV_UINT128:
     out << "(" << U128().U64H() << ", " << U128().U64L() << ")" ;
    break;
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16:
    PrintFloating<half>(U16(), out, printExtraHex);
    break;
  case MV_FLOAT:
    PrintFloating<f32_t>(U32(), out, printExtraHex);
    break;
  case MV_DOUBLE:
    PrintFloating<f64_t>(U64(), out, printExtraHex);
    break;
  case MV_INT8X4: 
    out << "(" << S8X4(0) << ", " << S8X4(1) << ", " << S8X4(2) << ", " << S8X4(3) << ")";
    break;
  case MV_INT8X8: 
    out << "(" << S8X8(0) << ", " << S8X8(1) << ", " << S8X8(2) << ", " << S8X8(3) << ", " << S8X8(1) << ", " << S8X8(2) << ", " << S8X8(3) << ", " << S8X8(4) << ")";
    break;
  case MV_UINT8X4:
    out << "(" << U8X4(0) << ", " << U8X4(1) << ", " << U8X4(2) << ", " << U8X4(3) << ")";
    break;
  case MV_UINT8X8:
    out << "(" << U8X8(0) << ", " << U8X8(1) << ", " << U8X8(2) << ", " << U8X8(3) << ", " << U8X8(1) << ", " << U8X8(2) << ", " << U8X8(3) << ", " << U8X8(4) << ")";
    break;
  case MV_INT16X2:
    out << "(" << S16X2(0) << ", " << S16X2(1) << ")";
    break;
  case MV_INT16X4:
    out << "(" << S16X4(0) << ", " << S16X4(1) << ", " << S16X4(2) << ", " << S16X4(3) << ")";
    break;
  case MV_UINT16X2:
    out << "(" << U16X2(0) << ", " << U16X2(1) << ")";
    break;
  case MV_UINT16X4:
    out << "(" << U16X4(0) << ", " << U16X4(1) << ", " << U16X4(2) << ", " << U16X4(3) << ")";
    break;
  case MV_INT32X2:
    out << "(" << S32X2(0) << ", " << S32X2(1) << ")";
    break;
  case MV_UINT32X2:
    out << "(" << U32X2(0) << ", " << U32X2(1) << ")";
    break;
  case MV_FLOATX2:
    out << "(";
    PrintFloating<f32_t>(U32X2(0), out, printExtraHex);
    out << ", ";
    PrintFloating<f32_t>(U32X2(1), out, printExtraHex);
    out << ")";
    break;
  case MV_REF:
  case MV_IMAGEREF:
    out << "ref " << std::hex << std::uppercase << std::setfill('0') << std::setw(sizeof(void *) * 2) << U32() << std::setfill(' ') << std::dec;
    break;
  case MV_POINTER:
    out << "pointer " << std::hex << std::uppercase << P() << std::setfill(' ') << std::dec;
    break;
  case MV_EXPR:
    out << "expr " << S();
    break;
  case MV_STRING:
    out << Str();
    break;
  default:
    out << "Error: unsupported value type: " << type << std::endl;
    assert(false);
  }
}

bool operator<(const Value& v1, const Value& v2)
{
  assert(v1.Type() == v2.Type());
  switch (v1.Type()) {
  case MV_INT8: return v1.S8() < v2.S8();
  case MV_UINT8: return v1.U8() < v2.U8();
  case MV_INT16: return v1.S16() < v2.S16();
  case MV_UINT16: return v1.U16() < v2.U16();
  case MV_INT32: return v1.S32() < v2.S32();
  case MV_UINT32: return v1.U32() < v2.U32();
  case MV_INT64: return v1.S64() < v2.S64();
  case MV_UINT64: return v1.U64() < v2.U64();
  case MV_UINT128: return v1.U128() < v2.U128();
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16: return v1.H() < v2.H();
  case MV_FLOAT: return v1.F() < v2.F();
  case MV_DOUBLE: return v1.D() < v2.D();
  default:
    assert(!"Unsupported type in operator < (Value)");
    return false;
  }
}

void Value::WriteTo(void *dest) const
{
  switch (type) {
  case MV_INT8: *((int8_t *) dest) = data.s8; break;
  case MV_UINT8: *((uint8_t *) dest) = data.u8; break;
  case MV_INT16: *((int16_t *) dest) = data.s16; break;
  case MV_UINT16: *((uint16_t *) dest) = data.u16; break;
  case MV_INT32: *((int32_t *) dest) = data.s32; break;
  case MV_UINT32: *((uint32_t *) dest) = data.u32; break;
  case MV_INT64: *((int64_t *) dest) = data.s64; break;
  case MV_UINT64: *((uint64_t *) dest) = data.u64; break;
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
    ((uint32_t *) dest)[0] = 0; // whole u32
    ((half::bits_t *) dest)[0] = data.h_bits;
    break;
#endif
  case MV_FLOAT16: *((half::bits_t *) dest) = data.h_bits; break;
  case MV_FLOAT: *((float *) dest) = data.f; break;
  case MV_DOUBLE: *((double *) dest) = data.d; break;
  case MV_INT8X4: ((int8_t *) dest)[0] = S8X4(0); ((int8_t *) dest)[1] = S8X4(1); ((int8_t *) dest)[2] = S8X4(2); ((int8_t *) dest)[3] = S8X4(3); break;
  case MV_INT8X8: ((int8_t *) dest)[0] = S8X8(0); ((int8_t *) dest)[1] = S8X8(1); ((int8_t *) dest)[2] = S8X8(2); ((int8_t *) dest)[3] = S8X8(3); ((int8_t *) dest)[4] = S8X8(4); ((int8_t *) dest)[5] = S8X8(5); ((int8_t *) dest)[6] = S8X8(6); ((int8_t *) dest)[7] = S8X8(7); break;
  case MV_UINT8X4: ((uint8_t *) dest)[0] = U8X4(0); ((uint8_t *) dest)[1] = U8X4(1); ((uint8_t *) dest)[2] = U8X4(2); ((uint8_t *) dest)[3] = U8X4(3); break;
  case MV_UINT8X8: ((uint8_t *) dest)[0] = U8X8(0); ((uint8_t *) dest)[1] = U8X8(1); ((uint8_t *) dest)[2] = U8X8(2); ((uint8_t *) dest)[3] = U8X8(3); ((uint8_t *) dest)[4] = U8X8(4); ((uint8_t *) dest)[5] = U8X8(5); ((uint8_t *) dest)[6] = U8X8(6); ((uint8_t *) dest)[7] = U8X8(7); break;
  case MV_INT16X2: ((int16_t *) dest)[0] = S16X2(0); ((int16_t *) dest)[1] = S16X2(1); break;
  case MV_INT16X4: ((int16_t *) dest)[0] = S16X4(0); ((int16_t *) dest)[1] = S16X4(1); ((int16_t *) dest)[2] = S16X4(2); ((int16_t *) dest)[3] = S16X4(3); break;
  case MV_UINT16X2: ((uint16_t *) dest)[0] = U16X2(0); ((uint16_t *) dest)[1] = U16X2(1); break;
  case MV_UINT16X4: ((uint16_t *) dest)[0] = U16X4(0); ((uint16_t *) dest)[1] = U16X4(1); ((uint16_t *) dest)[2] = U16X4(2); ((uint16_t *) dest)[3] = U16X4(3); break;
  case MV_INT32X2: ((int32_t *) dest)[0] = S32X2(0); ((int32_t *) dest)[1] = S32X2(1); break;
  case MV_UINT32X2: ((uint32_t *) dest)[0] = U32X2(0); ((uint32_t *) dest)[1] = U32X2(1); break;
  case MV_UINT128: ((uint64_t *) dest)[1] = data.u128.l; ((uint64_t *) dest)[0] = data.u128.h; break; // little endian
  case MV_FLOATX2: ((float *) dest)[0] = FX2(0); ((float *) dest)[1] = FX2(1); break;
  case MV_REF: *((uint32_t *) dest) = data.u32; break;
  case MV_POINTER: *((void **) dest) = data.p; break;
  default:
    assert(false);
  }
}

void Value::ReadFrom(const void *src, ValueType type)
{
  switch (type) {
  case MV_INT8: data.s8 = *((int8_t *) src); break;
  case MV_UINT8: data.u8 = *((uint8_t *) src); break;
  case MV_INT16: data.s16 = *((int16_t *) src); break;
  case MV_UINT16: data.u16 = *((uint16_t *) src); break;
  case MV_INT32: data.s32 = *((int32_t *) src); break;
  case MV_UINT32: data.u32 = *((uint32_t *) src); break;
  case MV_INT64: data.s64 = *((int64_t *) src); break;
  case MV_UINT64: data.u64 = *((uint64_t *) src); break;
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16: data.h_bits = *((half::bits_t *) src); break;
  case MV_FLOAT: data.f = *((float *) src); break;
  case MV_DOUBLE: data.d = *((double *) src); break;
  case MV_INT8X4: data.u32 = *((uint32_t *) src); break;
  case MV_INT8X8: data.u64 = *((uint64_t *) src); break;
  case MV_UINT8X4: data.u32 = *((uint32_t *) src); break;
  case MV_UINT8X8: data.u64 = *((uint64_t *) src); break;
  case MV_INT16X2: data.u32 = *((uint32_t *) src); break;
  case MV_INT16X4: data.u64 = *((uint64_t *) src); break;
  case MV_UINT16X2: data.u32 = *((uint32_t *) src); break;
  case MV_UINT16X4: data.u64 = *((uint64_t *) src); break;
  case MV_INT32X2: data.u64 = *((uint64_t *) src); break;
  case MV_UINT32X2: data.u64 = *((uint64_t *) src); break;
  case MV_UINT128: data.u128.l = *((uint64_t *) src); data.u128.h = ((uint64_t *)src)[1]; break;  // little endian
  case MV_FLOATX2: data.u64 = *((uint64_t *) src); break;
  case MV_REF: case MV_IMAGEREF: data.u32 = *((uint32_t *) src); break;
  case MV_POINTER: data.p = *((void **) src); break;
  default: assert(false); break;
  }
  this->type = type;
}

void Value::Serialize(std::ostream& out) const
{
  WriteData(out, type);
  WriteData(out, data.u64);
}

void Value::Deserialize(std::istream& in)
{
  ReadData(in, type);
  ReadData(in, data.u64);
}

void WriteTo(void *dest, const Values& values)
{
  char *ptr = (char *) dest;
  for (size_t i = 0; i < values.size(); ++i) {
    values[i].WriteTo(ptr);
    ptr += values[i].Size();
  }
}

void ReadFrom(void *src, ValueType type, size_t count, Values& values)
{
  char *ptr = (char *) src;
  for (size_t i = 0; i < count; ++i) {
    values.push_back(Value());
    values[i].ReadFrom(ptr, type);
    ptr += values[i].Size();
  }
}

uint32_t SizeOf(const Values& values) 
{
  uint32_t size = 0;
  for (const auto value: values) {
    size += static_cast<uint32_t>(value.Size());
  }
  return size;
}

void MBuffer::Print(std::ostream& out) const
{
  MObject::Print(out);
  out << ", MBuffer in " << MemString(mtype) << ", type " << ValueType2Str(vtype) << ", ";
  for (unsigned i = 0; i < dim; ++i) {
    if (i != 0) { out << "x"; }
    out << size[i];
  }
  out << " (" << Count() << " total, " << Data().size() << " init values)";
}

void MBuffer::PrintWithBuffer(std::ostream& out) const
{
  Print(out);
  IndentStream indent(out);
  for (unsigned i = 0; i < Data().size(); ++i) {
    Value value = Data()[i];
    value.SetPrintExtraHex(true);
    out << std::endl;
    out << GetPosStr(i) << ": " << std::setw(value.PrintWidth()) << value;
  }
}

size_t MBuffer::GetDim(size_t pos, unsigned d) const
{
  switch (d) {
  case 0: return pos % size[0];
  case 1: return (pos / size[0]) % size[1];
  case 2: return ((pos / size[0]) / size[1]) % size[2];
  default:
    assert(false); return 0;
  }
}

/*

void Context::Print(std::ostream& out) const
{
  for (std::map<std::string, Value>::const_iterator i = vmap.begin(); i != vmap.end(); ++i) {
    out << i->first << ": ";
    i->second.Print(out); out << " (" << i->second.Type() << ")"; out << std::endl;
  }
}

void Context::Clear()
{
  for (std::map<std::string, Value>::iterator i = vmap.begin(); i != vmap.end(); ++i) {
    if (i->second.Type() == MV_STRING) {
      delete &i->second.Str();
    }
  }
  vmap.clear();
}

bool Context::Has(const std::string& key) const
{
  return vmap.find(key) != vmap.end();
}

bool Context::Contains(const std::string& key) const
{
 return Has(key) || (parent && parent->Contains(key));
}

void *Context::GetPointer(const std::string& key)
{
  Value v = GetValue(key);
  assert(v.Type() == MV_POINTER);
  return v.P();
}

std::string Context::GetString(const std::string& key)
{
  Value v = GetValue(key);
  assert(v.Type() == MV_STRING);
  return v.Str();
}

uint64_t Context::GetHandle(const std::string& key)
{
  Value v = GetValue(key);
  assert(v.Type() == MV_UINT64);
  return v.U64();
}

const void *Context::GetPointer(const std::string& key) const
{
  std::map<std::string, Value>::const_iterator i = vmap.find(key);
  if (i != vmap.end()) {
    assert(i->second.Type() == MV_POINTER);
    return i->second.P();
  } else if (parent) {
    return parent->GetPointer(key);
  } else {
    return 0;
  }
}

void Context::PutPointer(const std::string& key, void *value)
{
  PutValue(key, Value(MV_POINTER, P(value)));
}

void *Context::RemovePointer(const std::string& key)
{
  Value v = RemoveValue(key);
  assert(v.Type() == MV_POINTER);
  return v.P();
}

Value Context::GetValue(const std::string& key)
{
  if (Has(key)) { 
    return vmap[key];
  } else if (parent) {
    return parent->GetValue(key);
  } else {
    std::cout << "Key: " << key << std::endl;
    assert(!"Value not found");
    return Value();
  }
}

void Context::PutValue(const std::string& key, Value value)
{
  vmap[key] = value;
}

Value Context::RemoveValue(const std::string& key)
{
  if (Has(key)) { 
    Value res = vmap[key];
    vmap.erase(key);
    return res;
  } else if (parent) {
    return parent->RemoveValue(key);
  } else {
    std::cout << "Key: " << key << std::endl;
    assert(!"Value not found");
    return Value();
  }

}

void Context::PutString(const std::string& key, const std::string& value)
{
  vmap[key] = Value(MV_STRING, Str(new std::string(value)));
}

size_t MBuffer::Size(Context* context) const
{
  switch (vtype) {
  case MV_EXPR: {
      size_t size = 0;
      for (size_t i = 0; i < data.size(); ++i) {
        Value v = context->GetValue(data[i].S());
        assert(v.Type() != MV_EXPR);
        size += v.Size();
      }
      return size;
    }

  default:
    return Count() * ValueTypeSize(vtype);
  }
}
*/

std::string MBuffer::GetPosStr(size_t pos) const
{
  std::stringstream ss;
  switch (dim) {
  case 1: ss << "[" << std::setw(2) << pos << "]"; break;
  case 2: ss << "[" << GetDim(pos, 0) << "," << GetDim(pos, 1) << "]"; break;
  case 3: ss << "[" << GetDim(pos, 0) << "," << GetDim(pos, 1) << "," << GetDim(pos, 2) << "]"; break;
  default:
    assert(false);
  }
  return ss.str();
}

void MBuffer::PrintComparisonInfo(std::ostream& out, size_t pos, Comparison& comparison, bool detailed) const
{
  if (dim == 1 || detailed) {
    comparison.PrintLong(out);
  } else {
    switch (dim) {
    case 2:
      comparison.PrintShort(out);  out << "  ";
      if (GetDim(pos + 1, 0) == 0) { out << std::endl; }
      break;
    default:
      assert(false);
    }
  }
}

void MBuffer::PrintComparisonSummary(std::ostream& out, Comparison& comparison) const
{
  if (comparison.IsFailed()) {
    out << "Error: failed " << comparison.GetFailed() << " / " << comparison.GetChecks() << " comparisons, "
      << "max " << comparison.GetMethodDescription() << " error " << comparison.GetMaxError() << " at " << GetPosStr(comparison.GetMaxErrorIndex()) << "." << std::endl;
  } else {
    out << "Successful " << comparison.GetChecks() << " comparisons." << std::endl;
  }
}

void MBuffer::SerializeData(std::ostream& out) const
{
  WriteData(out, mtype);
  WriteData(out, vtype);
  WriteData(out, dim);
  for (unsigned i = 0; i < 3; ++i) { WriteData(out, size[i]); }
  WriteData(out, data);
}

void MBuffer::DeserializeData(std::istream& in)
{
  ReadData(in, mtype);
  ReadData(in, vtype);
  ReadData(in, dim);
  for (unsigned i = 0; i < 3; ++i) { ReadData(in, size[i]); }
  ReadData(in, data);
}

void MRBuffer::Print(std::ostream& out) const
{
  MObject::Print(out);
  out << ", MRBuffer for " << refid;
  out << " (" << Data().size() << " check values)";
}

void MRBuffer::PrintWithBuffer(std::ostream& out) const
{
  Print(out);
  IndentStream indent(out);
  for (unsigned i = 0; i < Data().size(); ++i) {
    Value value = Data()[i];
    value.SetPrintExtraHex(true);
    out << std::endl;
    out << "[" << std::setw(2) << i << "]" << ": " << std::setw(value.PrintWidth()) << value;
  }
}

void MRBuffer::SerializeData(std::ostream& out) const
{
  WriteData(out, vtype);
  WriteData(out, refid);
  WriteData(out, data);
}

void MRBuffer::DeserializeData(std::istream& in)
{
  ReadData(in, vtype);
  ReadData(in, refid);
  ReadData(in, data);
}

ValueType ImageValueType(unsigned geometry)
{
  assert(false);
  return MV_LAST;
}

const double Comparison::F_DEFAULT_ULPS_PRECISION = 1;
const double Comparison::F_DEFAULT_RELATIVE_PRECISION = 0.01;

void Comparison::Reset(ValueType type)
{
  result = false;
  checks = 0;
  failed = 0;
  switch (type) {
    case MV_INT8: maxError = Value(MV_UINT8, U8(0)); break;
    case MV_INT16: maxError = Value(MV_UINT16, U16(0)); break;
    case MV_INT32: maxError = Value(MV_UINT32, U32(0)); break;
    case MV_INT64: maxError = Value(MV_UINT64, U64(0)); break;
    case MV_UINT128: maxError = Value(uint128_t(0, 0)); break;
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16:
#endif
    case MV_FLOAT16: //fall
    case MV_FLOAT: //fall
    case MV_DOUBLE: maxError = Value(MV_DOUBLE, D(0)); break;
    case MV_INT8X4: maxError = Value(MV_INT8X4, S8X4(0, 0, 0, 0)); break;
    case MV_INT8X8: maxError = Value(MV_INT8X8, S8X8(0, 0, 0, 0, 0, 0, 0, 0)); break;
    case MV_UINT8X4: maxError = Value(MV_UINT8X4, U8X4(0, 0, 0, 0)); break;
    case MV_UINT8X8: maxError = Value(MV_UINT8X8, U8X8(0, 0, 0, 0, 0, 0, 0, 0)); break;
    case MV_INT16X2: maxError = Value(MV_INT16X2, S16X2(0, 0)); break;
    case MV_INT16X4: maxError = Value(MV_INT16X4, S16X4(0, 0, 0, 0)); break;
    case MV_UINT16X2: maxError = Value(MV_UINT16X2, U16X2(0, 0)); break;
    case MV_UINT16X4: maxError = Value(MV_UINT16X4, U16X4(0, 0, 0, 0)); break;
    case MV_INT32X2: maxError = Value(MV_INT32X2, S32X2(0, 0)); break;
    case MV_UINT32X2: maxError = Value(MV_UINT32X2, U32X2(0, 0)); break;
    case MV_FLOATX2: maxError = Value(MV_FLOATX2, FX2(0, 0)); break;
    case MV_FLOAT16X2: maxError = Value(MV_FLOAT16X2, HX2(half(), half())); break;
    case MV_FLOAT16X4: maxError = Value(MV_FLOAT16X4, HX4(half(), half(), half(), half())); break;
    default: maxError = Value(type, U64((uint64_t) 0)); break;
  }
  maxErrorIndex = 0;
  if (precision.U64() == uint64_t(-1)) {
    SetDefaultPrecision(type);
  }
}

void Comparison::SetDefaultPrecision(ValueType type)
{
  switch (method) {
  case CM_IMAGE:
  case CM_DECIMAL:
    switch (type) {
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16:
#endif
    case MV_FLOAT16:
    case MV_FLOAT16X2: case MV_FLOAT16X4:
      precision = Value(MV_DOUBLE, D(pow((double) 10, -F16_DEFAULT_DECIMAL_PRECISION))); break;
    case MV_FLOAT:
    case MV_FLOATX2:
      precision = Value(MV_DOUBLE, D(pow((double) 10, -F32_DEFAULT_DECIMAL_PRECISION))); break;
    case MV_DOUBLE:
      precision = Value(MV_DOUBLE, D(pow((double) 10, -F64_DEFAULT_DECIMAL_PRECISION))); break;
    default: break;
    }
    break;
  case CM_ULPS:
    switch (type) {
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16:
#endif
    case MV_FLOAT16: case MV_FLOAT: case MV_DOUBLE:
    case MV_FLOAT16X2: case MV_FLOAT16X4:
    case MV_FLOATX2:
      precision = Value(MV_DOUBLE, D(F_DEFAULT_ULPS_PRECISION)); break;
    default: break;
    }
    break;
  case CM_RELATIVE:
    switch (type) {
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16:
#endif
    case MV_FLOAT16: case MV_FLOAT16X2: case MV_FLOAT16X4:
    case MV_FLOAT: case MV_FLOATX2:
    case MV_DOUBLE:
      precision = Value(MV_DOUBLE, D(F_DEFAULT_RELATIVE_PRECISION)); break;
    default: break;
    }
    break;
  default:
    break;
  }
}

static
Value AbsDiffUlps(const Value& v1, const Value& v2)
{
  assert(v1.Type() == v2.Type() && "AbsDiffUlps: types do not match");
  if (v1.Type() != v2.Type()) {
    return Value(MV_DOUBLE, D(0xffff));
  }
  switch(v1.Type()) {
  case MV_DOUBLE:
    if (v1.D() == 0.0 && v2.D() == 0.0 && (v1.U64() - v2.U64() != 0)) {
      return Value(MV_DOUBLE, D(1)); // sign of 0 should be the same
    }
    if (v1.U64() > v2.U64()) {
      return Value(MV_DOUBLE, D(static_cast<double>(v1.U64() - v2.U64())));
    }
    return Value(MV_DOUBLE, D(static_cast<double>(v2.U64() - v1.U64())));
  case MV_FLOAT:
    if (v1.F() == 0.0f && v2.F() == 0.0f && (v1.U32() - v2.U32() != 0)) {
      return Value(MV_DOUBLE, D(1));
    }
    if (v1.U32() > v2.U32()) {
      return Value(MV_DOUBLE, D(v1.U32() - v2.U32()));
    }
    return Value(MV_DOUBLE, D(v2.U32() - v1.U32()));
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16:
    if (v1.H().isZero() && v2.H().isZero() && (v1.U16() - v2.U16() != 0)) {
      return Value(MV_DOUBLE, D(1));
    }
    if (v1.U16() > v2.U16()) {
      return Value(MV_DOUBLE, D(v1.U16() - v2.U16()));
    }
    return Value(MV_DOUBLE, D(v2.U16() - v1.U16()));
  default:
    assert(0 && "AbsDiffUlps: wrong type");
    return Value(MV_DOUBLE, D(0xffff));
  }
}

bool Comparison::CompareHalf(const Value& v1 /*expected*/, const Value& v2 /*actual*/) {
  bool res = false;
  bool compared = false;
  const half f1 = v1.H();
  const half f2 = v2.H();
  if (f1.isNan() || f2.isNan()) {
    /// \todo Check NAN payload and type [see HSAIL spec]
    /// Sign of NANs is generally ignored, except floating-point bit ops [IEEE-754].
    /// Let's use absolute compare mode for bit ops.
    res = f1.isNan() && f2.isNan() && (method != CM_DECIMAL || f1.isNegative() == f2.isNegative());
    compared = true;
  } else if (f1.isInf() || f2.isInf()) {
    res = (f1.rawBits() == f2.rawBits()); // infinities must be identical
    compared = true;
  }
  if ( minLimit > f2.floatValue() || f2.floatValue() > maxLimit ) {
    res = false;
    compared = true;
  }
  if (compared) {
    switch (method) {
    case CM_ULPS: // fall
    case CM_DECIMAL: // fall
    case CM_RELATIVE:
      error = Value(MV_DOUBLE, D(res ? 0.0 : 1.0));
      return res;
    default:
      assert(!"Unsupported compare method"); return false;
    }
  }
  switch (method) {
  case CM_DECIMAL:
    error = Value(MV_DOUBLE, D(std::fabs(double(f1.floatValue()) - double(f2.floatValue()))));
    if(flushDenorms){
      const double v1_ftz = FlushDenorms(f1).floatValue();
      const double v2_ftz = FlushDenorms(f2).floatValue();
      const double error_ftz = std::fabs(v1_ftz - v2_ftz);
      if (error_ftz < error.D())
        error = Value(MV_DOUBLE, D(error_ftz));
    }
    return error.D() <= precision.D();
  case CM_ULPS:
    error = AbsDiffUlps(v1, v2);
    if(flushDenorms){
      Value v1_ftz = Value(MV_FLOAT16, H(FlushDenorms(f1)));
      Value v2_ftz = Value(MV_FLOAT16, H(FlushDenorms(f2)));
      Value error_ftz = AbsDiffUlps(v1_ftz, v2_ftz);
      if (error_ftz.D() < error.D()) error = error_ftz;
    }
    return error.D() <= precision.D();
  case CM_RELATIVE: {
    double eps = precision.D();
    if (f1 == half(0.0f)) {
      error = Value(MV_DOUBLE, D( std::fabs(2.0 * double(f2.floatValue())) ));
    } else {
      error = Value(MV_DOUBLE, D( std::fabs( (double(f1.floatValue()) - double(f2.floatValue())) / double(f1.floatValue())) ));
    }
    if (flushDenorms){
      const double v1_ftz = FlushDenorms(f1).floatValue();
      const double v2_ftz = FlushDenorms(f2).floatValue();
      double error_ftz;
      if (v1_ftz == 0.0) {
        error_ftz = std::fabs(2.0 * v2_ftz);
      } else {
        error_ftz = std::fabs((v1_ftz - v2_ftz) / v1_ftz);
      }
      if (error_ftz < error.D())
        error = Value(MV_DOUBLE, D(error_ftz));
    }
    return error.D() <= eps;
  }
  default:
    assert(!"Unsupported compare method"); return false;
  }
}


float Comparison::ConvertToStandard(float f) const
{
  if(f != f) return 0.0f; //check for NaN
  if(f < 0.0f) return 0.0f;
  if(f > 1.0f) return 1.0f;

  //all magic numbers are from PRM section 7.1.4.1.2
  double df = f;
  if(df < 0.0031308)
  {
    df *= 12.92;
    return static_cast<float>(df);
  }

  df = 1.055 * pow(df, 1.0f/2.4);
  df -= 0.055;

  return static_cast<float>(df);
}

bool Comparison::CompareFloat(const Value& v1 /*expected*/, const Value& v2 /*actual*/) {
  bool res = false;
  bool compared = false;
  const f32_t f1 = f32_t::fromRawBits(v1.U32());
  const f32_t f2 = f32_t::fromRawBits(v2.U32());
  if (f1.isNan() || f2.isNan()) {
    /// \todo Check NAN payload and type [see HSAIL spec]
    /// Sign of NANs is generally ignored, except floating-point bit ops [IEEE-754].
    /// Let's use absolute compare mode for bit ops.
    res = f1.isNan() && f2.isNan() && (method != CM_DECIMAL || f1.isNegative() == f2.isNegative());
    compared = true;
  } else if (f1.isInf() || f2.isInf()) {
    res = (f1.rawBits() == f2.rawBits()); // infinities must be identical
    compared = true;
  }
  if ( minLimit > f2.floatValue() || f2.floatValue() > maxLimit ) {
    res = false;
    compared = true;
  }
  if (compared) {
    switch (method) {
    case CM_ULPS: // fall
    case CM_DECIMAL: //fall
    case CM_RELATIVE:
    case CM_IMAGE:
      error = Value(MV_DOUBLE, D(res ? 0.0 : 1.0));
      return res;
    default:
      assert(!"Unsupported compare method"); return false;
    }
  }
  switch (method) {
  case CM_DECIMAL:
    error = Value(MV_DOUBLE, D(std::fabs(double(f1.floatValue()) - double(f2.floatValue()))));
    if(flushDenorms){
      const double v1_ftz = FlushDenorms(f1).floatValue();
      const double v2_ftz = FlushDenorms(f2).floatValue();
      const double error_ftz = std::fabs(v1_ftz - v2_ftz);
      if (error_ftz < error.D())
        error = Value(MV_DOUBLE, D(error_ftz));
    }
    return error.D() <= precision.D();
  case CM_IMAGE:
    error = Value(MV_DOUBLE, D(std::fabs((double)ConvertToStandard(f1.floatValue()) - (double)ConvertToStandard(f2.floatValue()))));
    return error.D() < precision.D();
  case CM_ULPS:
    error = AbsDiffUlps(v1, v2);
    if(flushDenorms){
      Value v1_ftz = Value(MV_FLOAT, F(FlushDenorms(f1).floatValue()));
      Value v2_ftz = Value(MV_FLOAT, F(FlushDenorms(f2).floatValue()));
      Value error_ftz = AbsDiffUlps(v1_ftz, v2_ftz);
      if (error_ftz.D() < error.D()) error = error_ftz;
    }
    return error.D() <= precision.D();
  case CM_RELATIVE: {
    double eps = precision.D();
    if (f1 == f32_t(0.0f)) {
      error = Value(MV_DOUBLE, D( std::fabs(2.0 * double(f2.floatValue())) ));
    } else {
      error = Value(MV_DOUBLE, D( std::fabs( (double(f1.floatValue()) - double(f2.floatValue())) / double(f1.floatValue()) ) ));
    }
    if (flushDenorms){
      const double v1_ftz = FlushDenorms(f1).floatValue();
      const double v2_ftz = FlushDenorms(f2).floatValue();
      double error_ftz;
      if (v1_ftz == 0.0) {
        error_ftz = std::fabs(2.0 * v2_ftz);
      } else {
        error_ftz = std::fabs((v1_ftz - v2_ftz) / v1_ftz);
      }
      if (error_ftz < error.D())
        error = Value(MV_DOUBLE, D(error_ftz));
    }
    return error.D() <= eps;
  }
  default:
    assert(!"Unsupported compare method"); return false;
  }
}

bool Comparison::CompareDouble(const Value& v1 /*expected*/, const Value& v2 /*actual*/) {
  bool res = false;
  bool compared = false;
  const f64_t f1 = f64_t::fromRawBits(v1.U64());
  const f64_t f2 = f64_t::fromRawBits(v2.U64());
  if (f1.isNan() || f2.isNan()) {
    /// \todo Check NAN payload and type [see HSAIL spec]
    /// Sign of NANs is generally ignored, except floating-point bit ops [IEEE-754].
    /// Let's use absolute compare mode for bit ops.
    res = f1.isNan() && f2.isNan() && (method != CM_DECIMAL || f1.isNegative() == f2.isNegative());
    compared = true;
  } else if (f1.isInf() || f2.isInf()) {
    res = (f1.rawBits() == f2.rawBits()); // infinities must be identical
    compared = true;
  }
  if ( minLimit > f2.floatValue() || f2.floatValue() > maxLimit ) {
    res = false;
    compared = true;
  }
  if (compared) {
    switch (method) {
    case CM_ULPS: //fall
    case CM_DECIMAL: //fall
    case CM_RELATIVE:
      error = Value(MV_DOUBLE, D(res ? 0.0 : 1.0));
      return res;
    default:
      assert(!"Unsupported compare method"); return false;
    }
  }
  switch (method) {
  case CM_DECIMAL:
    error = Value(MV_DOUBLE, D(std::fabs(f1.floatValue() - f2.floatValue())));
    if(flushDenorms){
      const double v1_ftz = FlushDenorms(f1).floatValue();
      const double v2_ftz = FlushDenorms(f2).floatValue();
      const double error_ftz = std::fabs(v1_ftz - v2_ftz);
      if (error_ftz < error.D())
        error = Value(MV_DOUBLE, D(error_ftz));
    }
    return error.D() <= precision.D();
  case CM_ULPS:
    error = AbsDiffUlps(v1, v2);
    if(flushDenorms){
      Value v1_ftz = Value(MV_DOUBLE, D(FlushDenorms(f1).floatValue()));
      Value v2_ftz = Value(MV_DOUBLE, D(FlushDenorms(f2).floatValue()));
      Value error_ftz = AbsDiffUlps(v1_ftz, v2_ftz);
      if (error_ftz.D() < error.D()) error = error_ftz;
    }
    return error.D() <= precision.D();
  case CM_RELATIVE: {
    double eps = precision.D();
    if (f1 == f64_t(0.0)) {
      error = Value(MV_DOUBLE, D( std::fabs(2.0 * f2.floatValue()) ));
    } else {
      error = Value(MV_DOUBLE, D( std::fabs( (f1.floatValue() - f2.floatValue()) / f1.floatValue() ) ));
    }
    if (flushDenorms){
      const double v1_ftz = FlushDenorms(f1).floatValue();
      const double v2_ftz = FlushDenorms(f2).floatValue();
      double error_ftz;
      if (v1_ftz == 0.0) {
        error_ftz = std::fabs(2.0 * v2_ftz);
      } else {
        error_ftz = std::fabs((v1_ftz - v2_ftz) / v1_ftz);
      }
      if (error_ftz < error.D())
        error = Value(MV_DOUBLE, D(error_ftz));
    }
    return error.D() <= eps;
  }
  default:
    assert(!"Unsupported compare method"); return false;
  }
}

bool Comparison::CompareValues(const Value& v1 /*expected*/, const Value& v2 /*actual*/)
{
  // std::cout << v1.Type() << "  " << v2.Type() << std::endl;
  assert(v1.Type() == v2.Type());
  switch (v1.Type()) {
  case MV_INT8: {
    error = Value(MV_UINT8, U8((std::max)(v1.S8(), v2.S8()) - (std::min)(v1.S8(), v2.S8())));
    return error.U8() == 0;
  }
  case MV_UINT8: {
    error = Value(MV_UINT8, U8((std::max)(v1.U8(), v2.U8()) - (std::min)(v1.U8(), v2.U8())));
    return error.U8() == 0;
  }
  case MV_INT16: {
    error = Value(MV_UINT16, U16((std::max)(v1.S16(), v2.S16()) - (std::min)(v1.S16(), v2.S16())));
    return error.U16() == 0;
  }
  case MV_UINT16: {
    error = Value(MV_UINT16, U16((std::max)(v1.U16(), v2.U16()) - (std::min)(v1.U16(), v2.U16())));
    return error.U16() == 0;
  }
  case MV_INT32: {
    error = Value(MV_UINT32, U32((std::max)(v1.S32(), v2.S32()) - (std::min)(v1.S32(), v2.S32())));
    return error.U32() == 0;
  }
  case MV_UINT32: {
    error = Value(MV_UINT32, U32((std::max)(v1.U32(), v2.U32()) - (std::min)(v1.U32(), v2.U32())));
    return error.U32() == 0;
  }
  case MV_INT64: {
    error = Value(MV_UINT64, U64((std::max)(v1.S64(), v2.S64()) - (std::min)(v1.S64(), v2.S64())));
    return error.U64() == 0;
  }
  case MV_UINT64: {
    error = Value(MV_UINT64, U64((std::max)(v1.U64(), v2.U64()) - (std::min)(v1.U64(), v2.U64())));
    return error.U64() == 0;
  }
  case MV_UINT128: {
    error = Value(MV_UINT128, U128(uint128_t(
      (std::max)(v1.U128().U64H(), v2.U128().U64H()) - (std::min)(v1.U128().U64H(), v2.U128().U64H()),
      (std::max)(v1.U128().U64L(), v2.U128().U64L()) - (std::min)(v1.U128().U64L(), v2.U128().U64L())
    )));
    return (error.U128().U64H() == 0) && (error.U128().U64L() == 0);
  }
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16:
    return CompareHalf(v1, v2);
  case MV_FLOAT:
    return CompareFloat(v1, v2);
  case MV_DOUBLE:
    return CompareDouble(v1, v2);
  case MV_INT8X4: {
    error = Value(MV_INT8X4, S8X4((std::max)(v1.S8X4(0), v2.S8X4(0)) - (std::min)(v1.S8X4(0), v2.S8X4(0)),
                                  (std::max)(v1.S8X4(1), v2.S8X4(1)) - (std::min)(v1.S8X4(1), v2.S8X4(1)),
                                  (std::max)(v1.S8X4(2), v2.S8X4(2)) - (std::min)(v1.S8X4(2), v2.S8X4(2)),
                                  (std::max)(v1.S8X4(3), v2.S8X4(3)) - (std::min)(v1.S8X4(3), v2.S8X4(3))));
    return error.U32() == 0;
  }
  case MV_INT8X8: {
    error = Value(MV_INT8X8, S8X8((std::max)(v1.S8X8(0), v2.S8X8(0)) - (std::min)(v1.S8X8(0), v2.S8X8(0)),
                                  (std::max)(v1.S8X8(1), v2.S8X8(1)) - (std::min)(v1.S8X8(1), v2.S8X8(1)),
                                  (std::max)(v1.S8X8(2), v2.S8X8(2)) - (std::min)(v1.S8X8(2), v2.S8X8(2)),
                                  (std::max)(v1.S8X8(3), v2.S8X8(3)) - (std::min)(v1.S8X8(3), v2.S8X8(3)),
                                  (std::max)(v1.S8X8(4), v2.S8X8(4)) - (std::min)(v1.S8X8(4), v2.S8X8(4)),
                                  (std::max)(v1.S8X8(5), v2.S8X8(5)) - (std::min)(v1.S8X8(5), v2.S8X8(5)),
                                  (std::max)(v1.S8X8(6), v2.S8X8(6)) - (std::min)(v1.S8X8(6), v2.S8X8(6)),
                                  (std::max)(v1.S8X8(7), v2.S8X8(7)) - (std::min)(v1.S8X8(7), v2.S8X8(7))));
    return error.U64() == 0;
  }
  case MV_UINT8X4: {
    error = Value(MV_UINT8X4, U8X4((std::max)(v1.U8X4(0), v2.U8X4(0)) - (std::min)(v1.U8X4(0), v2.U8X4(0)),
                                    (std::max)(v1.U8X4(1), v2.U8X4(1)) - (std::min)(v1.U8X4(1), v2.U8X4(1)),
                                    (std::max)(v1.U8X4(2), v2.U8X4(2)) - (std::min)(v1.U8X4(2), v2.U8X4(2)),
                                    (std::max)(v1.U8X4(3), v2.U8X4(3)) - (std::min)(v1.U8X4(3), v2.U8X4(3))));
    return error.U32() == 0;
  }
  case MV_UINT8X8: {
    error = Value(MV_UINT8X8, U8X8((std::max)(v1.U8X8(0), v2.U8X8(0)) - (std::min)(v1.U8X8(0), v2.U8X8(0)),
                                    (std::max)(v1.U8X8(1), v2.U8X8(1)) - (std::min)(v1.U8X8(1), v2.U8X8(1)),
                                    (std::max)(v1.U8X8(2), v2.U8X8(2)) - (std::min)(v1.U8X8(2), v2.U8X8(2)),
                                    (std::max)(v1.U8X8(3), v2.U8X8(3)) - (std::min)(v1.U8X8(3), v2.U8X8(3)),
                                    (std::max)(v1.U8X8(4), v2.U8X8(4)) - (std::min)(v1.U8X8(4), v2.U8X8(4)),
                                    (std::max)(v1.U8X8(5), v2.U8X8(5)) - (std::min)(v1.U8X8(5), v2.U8X8(5)),
                                    (std::max)(v1.U8X8(6), v2.U8X8(6)) - (std::min)(v1.U8X8(6), v2.U8X8(6)),
                                    (std::max)(v1.U8X8(7), v2.U8X8(7)) - (std::min)(v1.U8X8(7), v2.U8X8(7))));
    return error.U64() == 0;
  }
  case MV_INT16X2: {
    error = Value(MV_INT16X2, S16X2((std::max)(v1.S16X2(0), v2.S16X2(0)) - (std::min)(v1.S16X2(0), v2.S16X2(0)),
                                    (std::max)(v1.S16X2(1), v2.S16X2(1)) - (std::min)(v1.S16X2(1), v2.S16X2(1))));
    return error.U32() == 0;
  }
  case MV_INT16X4: {
    error = Value(MV_INT16X4, S16X4((std::max)(v1.S16X4(0), v2.S16X4(0)) - (std::min)(v1.S16X4(0), v2.S16X4(0)),
                                    (std::max)(v1.S16X4(1), v2.S16X4(1)) - (std::min)(v1.S16X4(1), v2.S16X4(1)),
                                    (std::max)(v1.S16X4(2), v2.S16X4(2)) - (std::min)(v1.S16X4(2), v2.S16X4(2)),
                                    (std::max)(v1.S16X4(3), v2.S16X4(3)) - (std::min)(v1.S16X4(3), v2.S16X4(3))));
    return error.U64() == 0;
  }
  case MV_UINT16X2: {
    error = Value(MV_UINT16X2, U16X2((std::max)(v1.U16X2(0), v2.U16X2(0)) - (std::min)(v1.U16X2(0), v2.U16X2(0)),
                                      (std::max)(v1.U16X2(1), v2.U16X2(1)) - (std::min)(v1.U16X2(1), v2.U16X2(1))));
    return error.U32() == 0;
  }
  case MV_UINT16X4: {
    error = Value(MV_UINT16X4, U16X4((std::max)(v1.U16X4(0), v2.U16X4(0)) - (std::min)(v1.U16X4(0), v2.U16X4(0)),
                                      (std::max)(v1.U16X4(1), v2.U16X4(1)) - (std::min)(v1.U16X4(1), v2.U16X4(1)),
                                      (std::max)(v1.U16X4(2), v2.U16X4(2)) - (std::min)(v1.U16X4(2), v2.U16X4(2)),
                                      (std::max)(v1.U16X4(3), v2.U16X4(3)) - (std::min)(v1.U16X4(3), v2.U16X4(3))));
    return error.U64() == 0;
  }
  case MV_INT32X2: {
    error = Value(MV_INT32X2, S32X2((std::max)(v1.S32X2(0), v2.S32X2(0)) - (std::min)(v1.S32X2(0), v2.S32X2(0)),
                                    (std::max)(v1.S32X2(1), v2.S32X2(1)) - (std::min)(v1.S32X2(1), v2.S32X2(1))));
    return error.U64() == 0;
  }
  case MV_UINT32X2: {
    error = Value(MV_UINT32X2, U32X2((std::max)(v1.U32X2(0), v2.U32X2(0)) - (std::min)(v1.U32X2(0), v2.U32X2(0)),
                                     (std::max)(v1.U32X2(1), v2.U32X2(1)) - (std::min)(v1.U32X2(1), v2.U32X2(1))));
    return error.U64() == 0;
  }
  case MV_FLOATX2: {
    Value fv1(MV_FLOAT, F(v1.FX2(0)));
    Value fv2(MV_FLOAT, F(v2.FX2(0)));
    Value ferror1(error);
    bool res1 = CompareFloat(fv1, fv2);
    fv1 = Value(MV_FLOAT, F(v1.FX2(1)));
    fv2 = Value(MV_FLOAT, F(v2.FX2(1)));
    Value ferror2(error);
    bool res2 = CompareFloat(fv1, fv2);
    error = Value(MV_FLOATX2, FX2(ferror1.F(), ferror2.F()));
    return res1 && res2;
  }
  default:
    assert(!"Unsupported type in absdifference"); return false;
  }
}

bool Comparison::Compare(const Value& expected, const Value& actual)
{
  this->expected = expected;
  this->actual = actual;
  //assert(evalue.Type() == rvalue.Type());
  result = CompareValues(expected, actual);
  if (!result) {
    failed++;
    if (maxError < error) {
      maxError = error;
      maxErrorIndex = checks;
    }
  }
  checks++;
  return result;
}

std::string Comparison::GetMethodDescription() const
{
  switch (method) {
  case CM_RELATIVE:
    return "relative";
  case CM_ULPS:
    return "ulps";
  case CM_IMAGE:
    return "image";
  case CM_DECIMAL:
    return "absolute";
  default:
    assert(!"Unsupported method in GetMethodDescription"); return "";
  }
}

void Comparison::PrintShort(std::ostream& out) const
{
  Print(out);
  if (result) {
    out << "    ";
  } else {
    out << " (*)";
  }
}

void Comparison::Print(std::ostream& out) const
{
  out << std::setw(actual.PrintWidth()) << actual;
}

void Comparison::PrintLong(std::ostream& out)
{
  /// \todo We need to restore print width after each intermediate extraHex printout (X2/4/8 cases).
  /// Add new attribute to Value saying that we want fixed-width printing? and get rid of
  /// Value::PrintWidth() and save-restore of width in PrintExtraHex? --Artem
  actual.SetPrintExtraHex(true);
  expected.SetPrintExtraHex(true);
  Print(out);
  out << " (exp. " << std::setw(expected.PrintWidth()) << expected;
  out << ", " << GetMethodDescription() << " difference " << std::setw(error.PrintWidth()) << error;
  if (result) {
    out << " ";
  } else {
    out << "*";
  }
  out << ")";
}

void Comparison::PrintDesc(std::ostream& out) const
{
  out << GetMethodDescription() << " precision " << precision;
}

void Comparison::SetFlushDenorms(bool flush)
{
  flushDenorms = flush;
}

void Comparison::SetMinLimit(float limit)
{
  minLimit = limit;
}

void Comparison::SetMaxLimit(float limit)
{
  maxLimit = limit;
}

std::ostream& operator<<(std::ostream& out, const Comparison& comparison)
{
  comparison.PrintDesc(out);
  return out;
}

unsigned str2u(const std::string& s)
{
  std::istringstream ss(s);
  unsigned n;
  ss >> n;
  return n;
}

/// \todo Parse string from left to right instead of fixed priorities.
/// Right now "legacy_default" has the topmost priority, "ulps=" has lowest.
///
/// \todo Extend to cover all types.
/// Right now only affects comparison of floating types.
///
//  Comparision method (see enum ComparisionMethod):
//    ulps=X  - set CM_ULPS comparision method with X precision (X is double)
//    image=X - set CM_IMAGE comparsion method with X precision (X is double)
//    absf=X - set CM_DECIMAL comparision method with X precision (X is double)
//    relf=X - set CM_RELATIVE comparision method with X precision (X is double)
//    legacy_default - reset method and precision to MObject's defaults.
//  Additional options:
//    flushDenorms - if set, Compare() will also compute error_ftz (when denorms are flushed to 0) and return min(error, error_ftz)
//    minf=X - comparison fails if actual value is less then X (X is float)
//    maxf=X - comparison fails if actual value is greater then X (X is float)
//  Examples:
//    "ulps=0,maxf=52.8,flushDenorms"
//    "absf=0.05,maxf=1.0"
//    ""
//    "ulps=3.5"
//
Comparison* NewComparison(const std::string& c, ValueType type)
{
  Comparison* result;
  std::string options(c);
  const std::string flushOption="flushDenorms";
  const std::string ulpOption="ulps=";
  const std::string imgOption="image=";
  const std::string minOption="minf=";
  const std::string maxOption="maxf=";
  const std::string relOption="relf=";
  const std::string absOption="absf=";
  size_t idx;
  float fLimit;
  // preset to default "ulps=0.5", then overwrite as per option(s) found
  ComparisonMethod fMethod(CM_ULPS);
  Value fPrecision(MV_DOUBLE, D(0.5));

  idx = options.find(ulpOption);
  if(idx != std::string::npos) {
    double maxError = std::stod(options.substr(idx + ulpOption.length()));
    fPrecision = Value(MV_DOUBLE, D(maxError));
  }

  idx = options.find(absOption);
  if(idx != std::string::npos) {
    double maxError = std::stod(options.substr(idx + absOption.length()));
    fMethod = CM_DECIMAL;
    fPrecision = Value(MV_DOUBLE, D(maxError));
  }

  idx = options.find(relOption);
  if(idx != std::string::npos) {
    double maxError = std::stod(options.substr(idx + relOption.length()));
    fMethod = CM_RELATIVE;
    fPrecision = Value(MV_DOUBLE, D(maxError));
  }

  idx = options.find(imgOption);
  if(idx != std::string::npos) {
    double maxError = std::stod(options.substr(idx + imgOption.length()));
    fMethod = CM_IMAGE;
    fPrecision = Value(MV_DOUBLE, D(maxError));
  }

  if(options.find(std::string("legacy_default")) != std::string::npos) {
    result = new Comparison(); // method = default
    result->SetDefaultPrecision(type);
  } else {
    switch (type) {
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16: // fall
#endif
    case MV_FLOAT16:
    case MV_FLOAT:
    case MV_DOUBLE:
      result = new Comparison(fMethod, fPrecision);
      break;
    default:
      result = new Comparison(CM_DECIMAL, Value(MV_UINT64, U64(0)));
      break;
    }
  }

  idx = options.find(minOption);
  if(idx != std::string::npos) {
    fLimit = std::stof(options.substr(idx + minOption.length()));
    result->SetMinLimit(fLimit);
  }

  idx = options.find(maxOption);
  if(idx != std::string::npos) {
    fLimit = std::stof(options.substr(idx + maxOption.length()));
    result->SetMaxLimit(fLimit);
  }

  idx = options.find(flushOption);
  result->SetFlushDenorms(idx != std::string::npos);
  
  return result;
}

void MemorySetup::Print(std::ostream& out) const
{
  for (const std::unique_ptr<MObject>& mo : mos) {
    mo->Print(out); out << std::endl;
  }
}

void MemorySetup::PrintWithBuffers(std::ostream& out) const
{
  for (const std::unique_ptr<MObject>& mo : mos) {
    mo->PrintWithBuffer(out); out << std::endl;
  }
}

void MemorySetup::Serialize(std::ostream& out) const
{
  uint32_t size = (uint32_t) mos.size();
  WriteData(out, size);
  for (const std::unique_ptr<MObject>& mo : mos) {
    Serializer<MObject*>::Write(out, mo.get());
  }
}

void MemorySetup::Deserialize(std::istream& in) {
  uint32_t size;
  ReadData(in, size);
  mos.resize(size);
  for (uint32_t i = 0; i < size; ++i) {
    MObject* mo;
    Serializer<MObject*>::Read(in, mo);
    mos[i].reset(mo);
  }
}

} // namespace hexl

/// \todo copy-paste from HSAILTestGenEmulatorTypes
namespace HSAIL_X { // experimental

//==============================================================================
//==============================================================================
//==============================================================================
// Implementation of U128 type

bool uint128::operator > (const uint128& x) const {
  if (U64H() < x.U64H()) {
    return false;
  } else if (U64H() == x.U64H()) {
    return U64L() > x.U64L();
  } else {
    return true;
  }
}

bool uint128::operator < (const uint128& x) const {
  if (U64H() > x.U64H()) {
    return false;
  } else if (U64H() == x.U64H()) {
    return U64L() < x.U64L();
  } else {
    return true;
  }
}

bool uint128::operator >= (const uint128& x) const {
  return !(*this < x);
}

bool uint128::operator <= (const uint128& x) const {
  return !(*this > x);
}

bool uint128::operator == (const uint128& x) const {
  return U64H() == x.U64H() && U64L() == x.U64L();
}

//==============================================================================
//==============================================================================
//==============================================================================

template<typename T> static T explicit_static_cast(uint64_t); // undefined - should not be called
template<> inline uint64_t explicit_static_cast<uint64_t>(uint64_t x) { return x; }
template<> inline uint32_t explicit_static_cast<uint32_t>(uint64_t x) { return static_cast<uint32_t>(x); }
template<> inline uint16_t explicit_static_cast<uint16_t>(uint64_t x) { return static_cast<uint16_t>(x); }

/// \brief Represents numeric value splitted to sign/exponent/mantissa.
///
/// Able to hold any numeric value of any supported type (integer or floating-point)
/// Mantissa is stored with hidden bit, if it is set. Bit 0 is LSB of mantissa.
/// Exponent is stored in decoded (unbiased) format.
///
/// Manupulations with mantissa, exponent and sign are obviouous,
/// so these members are exposed to outsize world.
template <typename T>
struct DecodedFpValueImpl {
    typedef T mant_t;
    mant_t mant; // with hidden bit
    int64_t exp; // powers of 2
    bool is_negative;
    int mant_width; // not counting hidden bit
    enum AddOmicronKind {
        ADD_OMICRON_NEGATIVE,
        ADD_OMICRON_ZERO,
        ADD_OMICRON_POSITIVE
    } add_omicron;

    template<typename BT, int mantissa_width>
    DecodedFpValueImpl(const Ieee754Props<BT,mantissa_width>& props)
    : mant(0), exp(0), is_negative(props.isNegative()), mant_width(mantissa_width), add_omicron(ADD_OMICRON_ZERO)
    {
        if (props.isInf() || props.isNan()) {
          assert(!"Input number must represent a numeric value here");
          return;
        }
        mant = mant_t(props.getMantissa());
        if (!props.isSubnormal() && !props.isZero()) mant |= Ieee754Props<BT,mantissa_width>::MANT_HIDDEN_MSB_MASK;
        exp = props.getExponent();
    }
    DecodedFpValueImpl() : mant(0), exp(0), is_negative(false), mant_width(0), add_omicron(ADD_OMICRON_ZERO) {}
private:
    template<typename TT> DecodedFpValueImpl(TT); // = delete;
public:
    /// \todo use enable_if<is_signed<T>>::value* p = NULL and is_integral<> to extend to other integral types
    explicit DecodedFpValueImpl(uint64_t val)
    : mant(val)
    , exp((int64_t)sizeof(val)*8-1) //
    , is_negative(false) // unsigned
    , mant_width((int)sizeof(val)*8-1) // hidden bit is MSB of input integer
    , add_omicron(ADD_OMICRON_ZERO)
    {}
    explicit DecodedFpValueImpl(uint32_t val)
    : mant(val)
    , exp((int64_t)sizeof(val)*8-1) //
    , is_negative(false) // unsigned
    , mant_width((int)sizeof(val)*8-1) // hidden bit is MSB of input integer
    , add_omicron(ADD_OMICRON_ZERO)
    {}
    explicit DecodedFpValueImpl(int64_t val)
    : mant(val < 0
      ? ((uint64_t)val == 0x8000000000000000ULL // Negation of this value yields undefined behavior.
        // Just keeping it as is gives valid result. Note that literal is used instead of INT_MIN.
        // The reason is that INT_MIN can be (-INT_MAX) on some systems.
        // Assumption: 2's complement arithmetic.
        ? val
        : -val)
      : val)
    , exp((int64_t)sizeof(val)*8-1) //
    , is_negative(val < 0)
    , mant_width((int)sizeof(val)*8-1) // hidden bit is MSB of input integer
    , add_omicron(ADD_OMICRON_ZERO)
    {}
    explicit DecodedFpValueImpl(int32_t val)
    : mant(val < 0 ? -int64_t(val) : val)
    , exp((int64_t)sizeof(val)*8-1) //
    , is_negative(val < 0)
    , mant_width((int)sizeof(val)*8-1) // hidden bit is MSB of input integer
    , add_omicron(ADD_OMICRON_ZERO)
    {}

public:
    template <typename P> // construct Ieee754Props
    P toProps() const
    {
        typedef typename P::Type P_Type; // shortcut
        assert((mant & ~(P::MANT_HIDDEN_MSB_MASK | P::MANT_MASK)) == 0);
        assert(mant_width == P::MANT_WIDTH);
        if (exp > P::DECODED_EXP_NORM_MAX) { // INF or NAN
            // By design, DecodedFpValue is unable to represent NANs
            return is_negative ? P::getNegativeInf() : P::getPositiveInf();
        }
        assert(exp < P::DECODED_EXP_NORM_MIN ? exp == P::DECODED_EXP_SUBNORMAL_OR_ZERO : true);
        assert(((mant & P::MANT_HIDDEN_MSB_MASK) == 0) ? exp == P::DECODED_EXP_SUBNORMAL_OR_ZERO : true);
        const P_Type exp_bits = (P_Type)(exp + P::EXP_BIAS) << P::MANT_WIDTH;
        assert((exp_bits & ~P::EXP_MASK) == 0);

        const P_Type bits =
            (!is_negative ? 0 : P::SIGN_MASK)
            | (exp_bits & P::EXP_MASK)
            | explicit_static_cast<P_Type>(mant & P::MANT_MASK); // throw away hidden bit
        return P(bits);
    }
};

typedef DecodedFpValueImpl<uint64_t> DecodedFpValue; // for conversions with rounding

//=============================================================================
//=============================================================================
//=============================================================================

/// Transforms mantissa of DecodedFpValue to the Ieee754Props<> format and normalizes it.
/// Adjusts exponent if needed. Transformation of mantissa includes shrinking/widening,
/// IEEE754-compliant rounding and normalization. Adjusting of exponent may be required
/// due to rounding and normalization. Mantissa as always normalized at the end.
/// \todo Exponent range is not guaranteed to be valid at the end.
template<typename T, typename T2>
class FpProcessorImpl
{
    typedef DecodedFpValueImpl<T2> decoded_t;
    typedef T2 mant_t;

    decoded_t &v;
    int shrMant;
    enum TieKind {
        TIE_UNKNOWN = 0,
        TIE_LIST_BEGIN_,
        TIE_IS_ZERO = TIE_LIST_BEGIN_, // exact match, no rounding required, just truncate
        TIE_LT_HALF, // tie bits ~= 0bb..bb, where at least one b == 1
        TIE_IS_HALF, // tie bits are exactly 100..00
        TIE_GT_HALF, // tie bits ~= 1bb..bb, where at least one b == 1
        TIE_LT_ZERO, // for add/sub
        TIE_LIST_END_
    } tieKind;
    // For manipulations with DecodedFpValueImpl we use MANT_ bitfields/masks from Ieee754Props<>.
    static_assert((T::MANT_MASK & 0x1), "LSB of mantissa in Ieee754Props<> must be is at bit 0");
public:
    explicit FpProcessorImpl(decoded_t &v_)
    : v(v_), shrMant(v.mant_width - T::MANT_WIDTH), tieKind(TIE_UNKNOWN)
    { assert(v.mant_width > 0); }
private:
    void IncrementMantAdjustExp() const;
    void DecrementMantAdjustExp() const;
    void CalculateTieKind();
    void RoundMantAdjustExp(unsigned rounding);
    void NormalizeMantAdjustExp();
public:
    void NormalizeInputMantAdjustExp();
    void NormalizeRound(unsigned rounding)
    {
        NormalizeInputMantAdjustExp();
        CalculateTieKind();
        RoundMantAdjustExp(rounding);
        NormalizeMantAdjustExp();
    }
    int getShrMant() const { return shrMant; }
};


template<typename T, typename T2>
void FpProcessorImpl<T,T2>::IncrementMantAdjustExp() const // does not care about exponent limits.
{
    assert(v.mant <= T::MANT_HIDDEN_MSB_MASK + T::MANT_MASK);
    ++v.mant;
    if (v.mant > T::MANT_HIDDEN_MSB_MASK + T::MANT_MASK) { // overflow 1.111...111 -> 10.000...000
        v.mant >>= 1; // ok to shift zero bit out (no precision loss)
        ++v.exp;
    }
}

template<typename T, typename T2>
void FpProcessorImpl<T,T2>::DecrementMantAdjustExp() const // does not care about exponent limits.
{
    assert(v.mant <= T::MANT_HIDDEN_MSB_MASK + T::MANT_MASK);
    --v.mant;
    if (v.mant < T::MANT_HIDDEN_MSB_MASK) { // underflow 1.000...000 -> 0.111.111
        v.mant <<= 1;
        --v.exp;
    }
}

template<typename T, typename T2>
void FpProcessorImpl<T,T2>::NormalizeInputMantAdjustExp()
{
    if(v.mant == 0) {
        return; // no need (and impossible) to normalize zero.
    }
    const mant_t INPUT_MANT_HIDDEN_MSB_MASK = mant_t(1) << v.mant_width;
    while ((v.mant & INPUT_MANT_HIDDEN_MSB_MASK) == 0 ) { // denorm on input
        v.mant <<= 1;
        --v.exp;
    }
    if (v.exp < T::DECODED_EXP_NORM_MIN) {
        // we expect denorm as output, therefore we will need to shift some extra bits out from mantissa.
        shrMant += (T::DECODED_EXP_NORM_MIN - v.exp);
        v.exp = T::DECODED_EXP_NORM_MIN;
    }
}

template<typename T, typename T2>
void FpProcessorImpl<T,T2>::CalculateTieKind()
{
    if (shrMant <= 0) {
        assert(v.add_omicron == decoded_t::ADD_OMICRON_ZERO);
        tieKind = TIE_IS_ZERO; // widths are equal or source is narrower
        return;
    }
    if (shrMant >= (sizeof(v.mant)*8 - 1)) {
        // Unable to compute and use MASK/HALF values - difference of widths is too big.
        // Correct behavior is as if MASK == all ones and tie is always < HALF.
        assert(v.add_omicron == decoded_t::ADD_OMICRON_ZERO);
        if (v.mant == 0 ) { tieKind = TIE_IS_ZERO; }
        else { tieKind = TIE_LT_HALF; }
        return;
    }
    assert(v.add_omicron == decoded_t::ADD_OMICRON_ZERO || v.add_omicron == decoded_t::ADD_OMICRON_POSITIVE || v.add_omicron == decoded_t::ADD_OMICRON_NEGATIVE);
    assert(v.add_omicron == decoded_t::ADD_OMICRON_ZERO || shrMant >= 2 || !"At least two-bit tail is required to interpret omicron correctly.");
    const mant_t SRC_MANT_TIE_MASK = (mant_t(1) << shrMant) - mant_t(1);
    const mant_t TIE_VALUE_HALF = mant_t(1) << (shrMant - 1); // 100..00B
    const mant_t tie = v.mant & SRC_MANT_TIE_MASK;
    if (tie == 0) {
        tieKind = TIE_IS_ZERO;
        if (v.add_omicron == decoded_t::ADD_OMICRON_NEGATIVE) { tieKind = TIE_LT_ZERO; }
        if (v.add_omicron == decoded_t::ADD_OMICRON_POSITIVE) { tieKind = TIE_LT_HALF; }
        return;
    }
    if (tie == TIE_VALUE_HALF) {
        tieKind = TIE_IS_HALF;
        if (v.add_omicron == decoded_t::ADD_OMICRON_NEGATIVE) { tieKind = TIE_LT_HALF; }
        if (v.add_omicron == decoded_t::ADD_OMICRON_POSITIVE) { tieKind = TIE_GT_HALF; }
        return;
    }
    if (tie <  TIE_VALUE_HALF) {
        tieKind = TIE_LT_HALF;
        return;
    }
    /*(tie > TIE_VALUE_HALF)*/ {
        tieKind = TIE_GT_HALF;
        return;
    }
}

template<typename T, typename T2>
void FpProcessorImpl<T,T2>::RoundMantAdjustExp(unsigned rounding) // does not care about exponent limits.
{
    assert(TIE_LIST_BEGIN_ <= tieKind && tieKind < TIE_LIST_END_);
    if (shrMant > 0) {
        // truncate mant, then adjust depending on (pre-calculated) tail kind:
        if (shrMant >= sizeof(v.mant)*8) { // we need this since C++0x
          v.mant = mant_t(0);
        } else {
          v.mant >>= shrMant;
        }
        switch (rounding) {
        default: assert(0); // fall
        case RND_NEAR:
            switch(tieKind) {
            case TIE_LT_ZERO: case TIE_IS_ZERO: case TIE_LT_HALF: default:
                break;
            case TIE_IS_HALF:
                if ((v.mant & 1) != 0) IncrementMantAdjustExp(); // if mant is odd, increment to even
                break;
            case TIE_GT_HALF:
                IncrementMantAdjustExp();
                break;
            }
            break;
        case RND_ZERO:
            switch(tieKind) {
            case TIE_LT_ZERO:
                DecrementMantAdjustExp();
                break;
            case TIE_IS_ZERO: case TIE_LT_HALF: case TIE_IS_HALF: case TIE_GT_HALF: default:
                break;
            }
            break;
        case RND_PINF:
            switch(tieKind) {
            case TIE_LT_ZERO:
                if (v.is_negative) DecrementMantAdjustExp();
                break;
            case TIE_IS_ZERO: default:
                break;
            case TIE_LT_HALF: case TIE_IS_HALF: case TIE_GT_HALF:
                if (!v.is_negative) IncrementMantAdjustExp();
                break;
            }
            break;
        case RND_MINF:
            switch(tieKind) {
            case TIE_LT_ZERO:
                if (!v.is_negative) DecrementMantAdjustExp();
                break;
            case TIE_IS_ZERO: default:
                break;
            case TIE_LT_HALF: case TIE_IS_HALF: case TIE_GT_HALF:
                if (v.is_negative) IncrementMantAdjustExp();
                break;
            }
            break;
        }
    } else { // destination mantisa is wider or has the same width.
        // this conversion is always exact
        assert((0 - shrMant) < sizeof(v.mant)*8);
        v.mant <<= (0 - shrMant); // fill by 0's from the left
    }
    v.mant_width = T::MANT_WIDTH;
    shrMant = 0;
}

template<typename T, typename T2>
void FpProcessorImpl<T,T2>::NormalizeMantAdjustExp() /// \todo does not care about upper exp limit.
{
    assert (v.mant_width == T::MANT_WIDTH && shrMant == 0 && "Mantissa must be in destination format here.");
    assert((v.mant & ~(T::MANT_HIDDEN_MSB_MASK | T::MANT_MASK)) == 0 && "Wrong mantissa, unused upper bits must be 0.");
    if(v.mant == 0) {
        v.exp = T::DECODED_EXP_SUBNORMAL_OR_ZERO;
        return; // no need (and impossible) to normalize zero.
    }
    if (v.exp >= T::DECODED_EXP_NORM_MIN) {
        if ((v.mant & T::MANT_HIDDEN_MSB_MASK) == 0) { // try to normalize
            while (v.exp > T::DECODED_EXP_NORM_MIN && ((v.mant & T::MANT_HIDDEN_MSB_MASK) == 0)) {
                v.mant <<= 1;
                --v.exp;
            }
            // end up with (1) either exp >= minimal or (2) hidden bit is 1 or (3) both
            if (v.exp > T::DECODED_EXP_NORM_MIN) {
                assert((v.mant & T::MANT_HIDDEN_MSB_MASK) != 0);
                // exp is above min limit, mant is normalized
                // nothing to do
            } else {
                assert(v.exp == T::DECODED_EXP_NORM_MIN);
                if ((v.mant & T::MANT_HIDDEN_MSB_MASK) == 0) {
                    // we dropped exp down to minimal, but mant is still subnormal
                    v.exp = T::DECODED_EXP_SUBNORMAL_OR_ZERO;
                } else {
                    // mant is normalized
                    // nothing to do
                }
            }
        } else {
            // mant is normalized
            // nothing to do
        }
    } else {
        assert (0);
    }
}

template<typename T> // shortcut to simplify code
struct FpProcessor : public FpProcessorImpl<T, uint64_t> {
    explicit FpProcessor(DecodedFpValue& v_) : FpProcessorImpl<T, uint64_t>(v_) {}
};

//=============================================================================
//=============================================================================
//=============================================================================

template<typename TO, typename T> static
void convertProp2RawBits(typename TO::bits_t &bits, const typename T::props_t& input, unsigned rounding)
{
    if (!input.isRegular()) {
        bits = TO::props_t::mapSpecialValues(input).rawBits();
        return;
    }
    DecodedFpValue val(input);
    FpProcessor<typename TO::props_t>(val).NormalizeRound(rounding);
    bits = val.toProps<typename TO::props_t>().rawBits();
}

template<typename TO, typename T> static
void convertRawBits2RawBits(typename TO::bits_t &bits, typename T::bits_t x, unsigned rounding)
{
    const typename T::props_t input(x);
    convertProp2RawBits<TO,T>(bits, input, rounding);
}


template<typename TO, typename T> static
void convertF2RawBits(typename TO::bits_t &bits, typename T::floating_t x, unsigned rounding)
{
    const typename T::props_t input(*reinterpret_cast<typename T::bits_t*>(&x));
    convertProp2RawBits<TO,T>(bits, input, rounding);
}

template<typename TO, typename INTEGER> static
void convertInteger2RawBits(typename TO::bits_t &bits, INTEGER x, unsigned rounding)
{
    DecodedFpValue val(x);
    FpProcessor<typename TO::props_t>(val).NormalizeRound(rounding);
    bits = val.toProps<typename TO::props_t>().rawBits();
}

f16_t::f16_t(double x, unsigned rounding) { convertF2RawBits<f16_t,f64_t>(m_bits, x, rounding); }
f16_t::f16_t(float x, unsigned rounding) { convertF2RawBits<f16_t,f32_t>(m_bits, x, rounding); }
f16_t::f16_t(f64_t x, unsigned rounding) { convertRawBits2RawBits<f16_t,f64_t>(m_bits, x.rawBits(), rounding); }
f16_t::f16_t(f32_t x, unsigned rounding) { convertRawBits2RawBits<f16_t,f32_t>(m_bits, x.rawBits(), rounding); }
f16_t::f16_t(int32_t x, unsigned rounding) { convertInteger2RawBits<f16_t>(m_bits, x, rounding); }
f16_t::f16_t(uint32_t x, unsigned rounding) { convertInteger2RawBits<f16_t>(m_bits, x, rounding); }
f16_t::f16_t(int64_t x, unsigned rounding) { convertInteger2RawBits<f16_t>(m_bits, x, rounding); }
f16_t::f16_t(uint64_t x, unsigned rounding) { convertInteger2RawBits<f16_t>(m_bits, x, rounding); }

f32_t::f32_t(double x, unsigned rounding) { convertF2RawBits<f32_t,f64_t>(m_bits, x, rounding); }
f32_t::f32_t(f64_t x, unsigned rounding) { convertRawBits2RawBits<f32_t,f64_t>(m_bits, x.rawBits(), rounding); }
f32_t::f32_t(f16_t x) { convertRawBits2RawBits<f32_t,f16_t>(m_bits, x.rawBits(), RND_NEAR); }
f32_t::f32_t(int32_t x, unsigned rounding) { convertInteger2RawBits<f32_t>(m_bits, x, rounding); }
f32_t::f32_t(uint32_t x, unsigned rounding) { convertInteger2RawBits<f32_t>(m_bits, x, rounding); }
f32_t::f32_t(int64_t x, unsigned rounding) { convertInteger2RawBits<f32_t>(m_bits, x, rounding); }
f32_t::f32_t(uint64_t x, unsigned rounding) { convertInteger2RawBits<f32_t>(m_bits, x, rounding); }

f64_t::f64_t(float x) { convertF2RawBits<f64_t,f32_t>(m_bits, x, RND_NEAR); }
f64_t::f64_t(f32_t x) { convertRawBits2RawBits<f64_t,f32_t>(m_bits, x.rawBits(), RND_NEAR); }
f64_t::f64_t(f16_t x) { convertRawBits2RawBits<f64_t,f16_t>(m_bits, x.rawBits(), RND_NEAR); }
f64_t::f64_t(int32_t x, unsigned rounding) { convertInteger2RawBits<f64_t>(m_bits, x, rounding); }
f64_t::f64_t(uint32_t x, unsigned rounding) { convertInteger2RawBits<f64_t>(m_bits, x, rounding); }
f64_t::f64_t(int64_t x, unsigned rounding) { convertInteger2RawBits<f64_t>(m_bits, x, rounding); }
f64_t::f64_t(uint64_t x, unsigned rounding) { convertInteger2RawBits<f64_t>(m_bits, x, rounding); }

} // namespace HSAIL_X
