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

#include "MObject.hpp"
#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#define isnan _isnan
#endif // _WIN32

namespace hexl {

template <typename T>
static int is_inf(T const& x)
{ /// \todo too narrow impl... Rework this
  if (x == std::numeric_limits<T>::infinity()) return 1;
  if (x == -std::numeric_limits<T>::infinity()) return -1;
  return 0;
}

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
  vector[0] = a.getBits(); vector[1] = b.getBits();
  ValueData data;
  data.u32 = *reinterpret_cast<uint32_t *>(vector.data());
  return data;
}

ValueData HX4(half a, half b, half c, half d) {
  std::vector<half::bits_t> vector(4);
  vector[0] = a.getBits(); vector[1] = b.getBits(); vector[2] = c.getBits(); vector[3] = d.getBits();
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

const char *ImageAccessString(MObjectImageAccess mem)
{
  switch (mem) {
  case IMG_ACCESS_READ_ONLY: return "ro";
  case IMG_ACCESS_WRITE_ONLY: return "wo";
  case IMG_ACCESS_READ_WRITE: return "rw";
  case IMG_ACCESS_NOT_SUPPORTED:
  case IMG_ACCESS_READ_MODIFY_WRITE:
  default: assert(false); return "<unknown access type>";
  }
};

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

const char *SamplerFilterString(MObjectSamplerFilter mem)
{
  switch (mem) {
  case SMP_NEAREST: return "nearest";
  case SMP_LINEAR: return "linear";
  default: assert(false); return "<unknown filter>";
  }
}

const char *SamplerCoordsString(MObjectSamplerCoords mem)
{
  switch (mem) {
  case SMP_NORMALIZED: return "normalized";
  case SMP_UNNORMALIZED: return "unnormalized";
  default: assert(false); return "<unknown coords>";
  }
}

const char *SamplerAddressingString(MObjectSamplerAddressing mem)
{
  switch (mem) {
  case SMP_UNDEFINED: return "undefined";
  case SMP_CLAMP_TO_EDGE: return "clamp_to_edge";
  case SMP_CLAMP_TO_BORDER: return "clamp_to_border";
  case SMP_MODE_REPEAT: return "repeat";
  case SMP_MIRRORED_REPEAT: return "mirrored_repeat";
  default: assert(false); return "<unknown addressing>";
  }
}

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

const char *ValueTypeString(ValueType type)
{
  switch (type) {
  case MV_INT8: return "int8";
  case MV_UINT8: return "uint8";
  case MV_INT16: return "int16";
  case MV_UINT16: return "uint16";
  case MV_INT32: return "int32";
  case MV_UINT32: return "uint32";
  case MV_INT64: return "int64";
  case MV_UINT64: return "uint64";
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16: return "half";
  case MV_FLOAT: return "float";
  case MV_DOUBLE: return "double";
  case MV_INT8X4: return "int8x4";
  case MV_INT8X8: return "int8x8";
  case MV_UINT8X4: return "uint8x4";
  case MV_UINT8X8: return "uint8x8";
  case MV_INT16X2: return "int16x2";
  case MV_INT16X4: return "int16x4";
  case MV_UINT16X2: return "uint16x2";
  case MV_UINT16X4: return "uint16x4";
  case MV_INT32X2: return "int32x2";
  case MV_UINT32X2: return "uint32x2";
  case MV_UINT128: return "uint128";
  case MV_FLOAT16X2: return "halfx2";
  case MV_FLOAT16X4: return "halfx4";
  case MV_FLOATX2: return "floatx2";
  case MV_IMAGE: return "image";
  case MV_REF: return "ref";
  case MV_IMAGEREF: return "imageref";
  case MV_SAMPLERREF: return "samplerref";
  case MV_POINTER: return "pointer";
  case MV_EXPR: return "expr";
  case MV_STRING: return "string";
  default:
    assert(false); return "<unknown type>";
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
  case MV_UINT128: return ValueTypePrintWidth(MV_UINT64)*2+1;
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

static bool isnan_half(const half h) {
  return HSAIL_X::FloatProp16(h.getBits()).isNan();
}

static bool isinf_half(const half h) {
  return HSAIL_X::FloatProp16(h.getBits()).isInf();
}

static
void PrintExtraHex(std::ostream& out, uint64_t bits /* zero-extended */, size_t sizeof_bits) {
  const std::streamsize wSave = out.width(); /// \todo get rid of width save/restore? --artem
  out << " (0x" << std::hex << std::setw(sizeof_bits * 2) << std::setfill('0') << bits << ")";
  out << std::setfill(' ') << std::dec << std::setw(wSave);
}

static
std::ostream& PrintHalf(const half h, const half::bits_t bits, std::ostream& out, const bool extraHex){
  if (isnan_half(h) || isinf_half(h)) {
    out << (isnan_half(h) ? "NAN" : "INF");
  } else {
    out << std::setprecision(Comparison::F16_MAX_DECIMAL_PRECISION) << (float)h;
  }
  if (extraHex) PrintExtraHex(out, bits, sizeof(bits));
  return out;
};

static
std::ostream& PrintFloat(const float f, const uint32_t bits, std::ostream& out, const bool extraHex){
  if (isnan(f) || is_inf(f)) {
    out << (isnan(f) ? "NAN" : "INF");
  } else {
    out << std::setprecision(Comparison::F32_MAX_DECIMAL_PRECISION) << f;
  }
  if (extraHex) PrintExtraHex(out, bits, sizeof(bits));
  return out;
};

static
std::ostream& PrintDouble(const double d, const uint64_t bits, std::ostream& out, const bool extraHex){
  if (isnan(d) || is_inf(d)) {
    out << (isnan(d) ? "NAN" : "INF");
  } else {
    out << std::setprecision(Comparison::F64_MAX_DECIMAL_PRECISION) << d;
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
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16:
    PrintHalf(H(), U16(), out, printExtraHex);
    break;
  case MV_FLOAT:
    PrintFloat(F(), U32(), out, printExtraHex);
    break;
  case MV_DOUBLE:
    PrintDouble(D(), U64(), out, printExtraHex);
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
    out << "(";  PrintFloat(FX2(0), U32X2(0), out, printExtraHex);
    out << ", ";
    PrintFloat(FX2(1), U32X2(1), out, printExtraHex);
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
  case MV_UINT128: ((uint64_t *) dest)[0] = data.u128.h; ((uint64_t *) dest)[1] = data.u128.l; break;
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
  case MV_UINT128: data.u128.l = *((uint64_t *) src); data.u128.h = ((uint64_t *)src)[1]; break;
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

void MBuffer::Print(std::ostream& out) const
{
  MObject::Print(out);
  out << ", MBuffer in " << MemString(mtype) << ", type " << ValueTypeString(vtype) << ", ";
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

unsigned MImage::AccessPermission() const 
{
  switch(imageType) {
  case BRIG_TYPE_ROIMG: return 1;
  case BRIG_TYPE_WOIMG: return 2;
  case BRIG_TYPE_RWIMG: return 3;
  default: assert(false); return 0;
  }
}

size_t MImage::GetDim(size_t pos, unsigned d) const
{
  switch (d) {
  case 0: return pos % width;
  case 1: return (pos / width) % height;
  case 2: return ((pos / width) / height) % depth;
  default:
    assert(false); return 0;
  }
}

void MImage::Print(std::ostream& out) const
{
  MObject::Print(out);
  out << ", MImage details: " << ImageGeometryString(MObjectImageGeometry(geometry)) << \
    ", " << ImageChannelTypeString(MObjectImageChannelType(channelType)) <<  ", " << ImageChannelOrderString(MObjectImageChannelOrder(channelOrder)) << ", " << ImageAccessString(MObjectImageAccess(AccessPermission()));
  out << " (" << "Image dim: [" << Width() << "x" << Height() << "x" << Depth() <<  "x" << ArraySize() <<"])";
}

std::string MImage::GetPosStr(size_t pos) const
{
  std::stringstream ss;
  ss << "[" << std::setw(2) << pos << "]";
  if (height > 1)
  {
    ss << "[" << GetDim(pos, 0) << "," << GetDim(pos, 1) << "]";
  }
  if (depth > 1)
  {
    ss << "[" << GetDim(pos, 0) << "," << GetDim(pos, 1) << "," << GetDim(pos, 2) << "]";
  }

  return ss.str();
}

void MImage::PrintComparisonInfo(std::ostream& out, size_t pos, Comparison& comparison) const
{
  out << "Failure at " << pos << ": ";
  comparison.PrintLong(out);
  out << std::endl;
}

void MImage::PrintComparisonSummary(std::ostream& out, Comparison& comparison) const
{
  if (comparison.IsFailed()) {
    out << "Error: failed " << comparison.GetFailed() << " / " << comparison.GetChecks() << " comparisons, "
      << "max " << comparison.GetMethodDescription() << " error " << comparison.GetMaxError() << " at index " << comparison.GetMaxErrorIndex() << "." << std::endl;
  } else {
    out << "Successful " << comparison.GetChecks() << " comparisons." << std::endl;
  }
}


void MImage::SerializeData(std::ostream& out) const
{
  assert(false);
}

void MImage::DeserializeData(std::istream& in)
{
  assert(false);
}

void MRImage::SerializeData(std::ostream& out) const
{
  assert(false);
}

void MRImage::DeserializeData(std::istream& in)
{
  assert(false);
}

void MSampler::Print(std::ostream& out) const
{
  MObject::Print(out);
  out << ", MSampler details: " << SamplerFilterString(MObjectSamplerFilter(filter)) << \
   ", " << SamplerCoordsString(MObjectSamplerCoords(coords)) << ", " << SamplerAddressingString(MObjectSamplerAddressing(addressing));
}

void MSampler::SerializeData(std::ostream& out) const
{
  assert(false);
}

void MSampler::DeserializeData(std::istream& in)
{
  assert(false);
}

void MRSampler::SerializeData(std::ostream& out) const
{
  assert(false);
}

void MRSampler::DeserializeData(std::istream& in)
{
  assert(false);
}

ValueType ImageValueType(unsigned geometry)
{
  assert(false);
  return MV_LAST;
}

const uint32_t Comparison::F_DEFAULT_ULPS_PRECISION = 0;
const double Comparison::F_DEFAULT_RELATIVE_PRECISION = 0.01;

void Comparison::Reset(ValueType type)
{
  result = false;
  checks = 0;
  failed = 0;
  switch (method) {
  case CM_RELATIVE:
  case CM_DECIMAL:
    switch (type) {
    case MV_INT8: maxError = Value(MV_UINT8, U8(0)); break;
    case MV_INT16: maxError = Value(MV_UINT16, U16(0)); break;
    case MV_INT32: maxError = Value(MV_UINT32, U32(0)); break;
    case MV_INT64: maxError = Value(MV_UINT64, U64(0)); break;
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16:
#endif
    case MV_FLOAT16: maxError = Value(MV_DOUBLE, D((double) 0)); break;
    case MV_FLOAT: maxError = Value(MV_DOUBLE, D((double) 0)); break;
    case MV_DOUBLE: maxError = Value(MV_DOUBLE, D((double) 0)); break;
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
    case MV_FLOAT16X2: maxError = Value(MV_FLOAT16X2, HX2(half(0), half(0))); break;
    case MV_FLOAT16X4: maxError = Value(MV_FLOAT16X4, HX4(half(0), half(0), half(0), half(0))); break;
    default: maxError = Value(type, U64((uint64_t) 0)); break;
    }
    break;
  case CM_ULPS:
    switch (type) {
    case MV_FLOAT: maxError = Value(MV_UINT32, U32(0)); break;
    case MV_DOUBLE: maxError = Value(MV_UINT64, U64(0)); break;
    case MV_FLOATX2: maxError = Value(MV_UINT32X2, U32X2(0, 0)); break;
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16:
#endif
    case MV_FLOAT16: maxError = Value(MV_UINT16, U16(0)); break;
    case MV_FLOAT16X2: maxError = Value(MV_UINT16X2, U16X2(0, 0)); break;
    case MV_FLOAT16X4: maxError = Value(MV_UINT16X4, U16X4(0, 0, 0, 0)); break;
    default: assert(false);
    }
    break;
  default: assert(false);
  }
  maxErrorIndex = 0;
  SetDefaultPrecision(type);
}

void Comparison::SetDefaultPrecision(ValueType type)
{
  switch (method) {
  case CM_DECIMAL:
    switch (type) {
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16:
#endif
    case MV_FLOAT16: case MV_FLOAT16X2: case MV_FLOAT16X4:
      if (precision.U64() == 0) { precision = Value(MV_DOUBLE, D(pow((double) 10, -F16_DEFAULT_DECIMAL_PRECISION))); }
      break;
    case MV_FLOAT:
    case MV_FLOATX2:
      if (precision.U64() == 0) { precision = Value(MV_DOUBLE, D(pow((double) 10, -F32_DEFAULT_DECIMAL_PRECISION))); }
      break;
    case MV_DOUBLE:
      if (precision.U64() == 0) { precision = Value(MV_DOUBLE, D(pow((double) 10, -F64_DEFAULT_DECIMAL_PRECISION))); }
      break;
    default: break;
    }
    break;
  case CM_ULPS:
    switch (type) {
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
    case MV_PLAIN_FLOAT16:
#endif
    case MV_FLOAT16: case MV_FLOAT16X2: case MV_FLOAT16X4:
    case MV_FLOAT: case MV_FLOATX2:
    case MV_DOUBLE:
      if (precision.U64() == 0) { precision = Value(MV_UINT64, U64(F_DEFAULT_ULPS_PRECISION)); }
      break;
    default:
      break;
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
      if (precision.D() == 0.0) { precision = Value(MV_DOUBLE, D(F_DEFAULT_RELATIVE_PRECISION)); }
      break;
    default:
      break;
    }
  }
}

bool CompareHalf(const Value& v1, const Value& v2, ComparisonMethod method, const Value& precision, Value& error) {
  bool res;
  bool compared = false;
  if (isnan_half(v1.H()) || isnan_half(v2.H())) {
    res = isnan_half(v1.H()) && isnan_half(v2.H());
    compared = true;
  } else if (isinf_half(v1.H()) || isinf_half(v2.H())) {
    res = isinf_half(v1.H()) == isinf_half(v2.H());
    compared = true;
  } else if (v1.H() == half(0) && v2.H() == half(0)) { // ignore sign of 0
    res = true;
    compared = true;
  }
  if (compared) {
    switch (method) {
    case CM_ULPS:
      error = Value(MV_UINT16, U16(res ? 0 : 1));
      return res;
    case CM_DECIMAL:
    case CM_RELATIVE:
      error = Value(MV_DOUBLE, D(res ? 0.0 : 1.0));
      return res;
    default:
      assert(!"Unsupported compare method in CompareValues"); return false;
    }
  }
  switch (method) {
  case CM_DECIMAL: {
    error = Value(MV_DOUBLE, D(std::fabs((double) v1.H() - (double) v2.H())));
    return error.D() < (double) precision.D();
  }
  case CM_ULPS: {
    error = Value(MV_UINT16, U16((std::max)(v1.U16(), v2.U16()) - (std::min)(v1.U16(), v2.U16())));
    return error.U16() <= precision.U16();
  }
  case CM_RELATIVE: {
    double eps = precision.D();
    if (v1.H() == half(0)) {
      error = Value(MV_DOUBLE, D((double) v2.H()));
      return error.D() < eps;
    } else {
      error = Value(MV_DOUBLE, D(std::fabs((double) v1.H() - (double) v2.H()) / (double) v1.H()));
      return error.D() < eps;
    }
  }    
  default:
    assert(!"Unsupported compare method in absdifference"); return false;
  }
}

bool CompareFloat(const Value& v1, const Value& v2, ComparisonMethod method, const Value& precision, Value& error) {
  bool res;
  bool compared = false;
  if (isnan(v1.F()) || isnan(v2.F())) {
    res = isnan(v1.F()) && isnan(v2.F());
    compared = true;
  } else if (is_inf(v1.F()) || is_inf(v2.F())) {
    res = is_inf(v1.F()) == is_inf(v2.F());
    compared = true;
  } else if (v1.F() == 0.0f && v2.F() == 0.0f) { // ignore sign of 0
    res = true;
    compared = true;
  }
  if (compared) {
    switch (method) {
    case CM_ULPS:
      error = Value(MV_UINT32, U32(res ? 0 : 1));
      return res;
    case CM_DECIMAL:
    case CM_RELATIVE:
      error = Value(MV_DOUBLE, D(res ? 0.0f : 1.0f));
      return res;
    default:
      assert(!"Unsupported compare method in CompareValues"); return false;
    }
  }
  switch (method) {
  case CM_DECIMAL: {
    error = Value(MV_DOUBLE, D(std::fabs((double) v1.F() - (double) v2.F())));
    return error.D() < (double) precision.D();
  }
  case CM_ULPS: {
    error = Value(MV_UINT32, U32((std::max)(v1.U32(), v2.U32()) - (std::min)(v1.U32(), v2.U32())));
    return error.U32() <= precision.U32();
  }
  case CM_RELATIVE: {
    double eps = precision.D();
    if (v1.F() == 0.0f) {
      error = Value(MV_DOUBLE, D((double) v2.F()));
      return error.D() < eps;
    } else {
      error = Value(MV_DOUBLE, D(std::fabs((double) v1.F() - (double) v2.F()) / (double) v1.F()));
      return error.D() < eps;
    }
  }    
  default:
    assert(!"Unsupported compare method in absdifference"); return false;
  }
}

bool CompareValues(const Value& v1, const Value& v2, ComparisonMethod method, const Value& precision, Value& error)
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
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
  case MV_PLAIN_FLOAT16:
#endif
  case MV_FLOAT16: {
    auto res = CompareHalf(v1, v2, method, precision, error);
    return res;
  }
  case MV_FLOAT: {
    auto res = CompareFloat(v1, v2, method, precision, error);
    return res;
  }
  case MV_DOUBLE: {
    bool res;
    bool compared = false;
    if (isnan(v1.D()) || isnan(v2.D())) {
      res = isnan(v1.D()) && isnan(v2.D());
      compared = true;
    } else if (is_inf(v1.D()) || is_inf(v2.D())) {
      res = is_inf(v1.D()) == is_inf(v2.D());
      compared = true;
    } else if (v1.D() == 0.0 && v2.D() == 0.0) { // ignore sign of 0
      res = true;
      compared = true;
    }
    if (compared) {
      switch (method) {
      case CM_ULPS:
        error = Value(MV_UINT64, U64(res ? 0 : 1));
        return res;
      case CM_DECIMAL:
      case CM_RELATIVE:
        error = Value(MV_DOUBLE, D(res ? 0.0 : 1.0));
        return res;
      default:
        assert(!"Unsupported compare method in CompareValues"); return false;
      }
    }
    switch (method) {
    case CM_DECIMAL:
      error = Value(MV_DOUBLE, D(v1.D() - v2.D()));
      return error.D() < precision.D();
    case CM_ULPS:
      error = Value(MV_UINT64, U64((std::max)(v1.U64(), v2.U64()) - (std::min)(v1.U64(), v2.U64())));
      return error.U64() <= precision.U64();
    case CM_RELATIVE: {
      double eps = precision.D();
      if (v1.D() == 0.0f) {
        error = Value(MV_DOUBLE, D(v2.D()));
      } else {
        error = Value(MV_DOUBLE, D(std::fabs(v1.D() - v2.D()) / v1.D()));
      }
      return error.D() < eps;
    }
    default:
      assert(!"Unsupported compare method in absdifference"); return false;
    }
  }
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
    bool res1 = CompareFloat(fv1, fv2, method, precision, ferror1);
    fv1 = Value(MV_FLOAT, F(v1.FX2(1)));
    fv2 = Value(MV_FLOAT, F(v2.FX2(1)));
    Value ferror2(error);
    bool res2 = CompareFloat(fv1, fv2, method, precision, ferror2);
    error = Value(MV_FLOATX2, FX2(ferror1.F(), ferror2.F()));
    return res1 && res2;
  }
  default:
    assert(!"Unsupported type in absdifference"); return false;
  }
}

bool Comparison::Compare(const Value& evalue, const Value& rvalue)
{
  this->evalue = evalue;
  this->rvalue = rvalue;
  //assert(evalue.Type() == rvalue.Type());
  result = CompareValues(evalue, rvalue, method, precision, error);
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
  out << std::setw(rvalue.PrintWidth()) << rvalue;
}

void Comparison::PrintLong(std::ostream& out)
{
  /// \todo We need to restore print width after each intermediate extraHex printout (X2/4/8 cases).
  /// Add new attribute to Value saying that we want fixed-width printing? and get rid of
  /// Value::PrintWidth() and save-restore of width in PrintExtraHex? --Artem
  rvalue.SetPrintExtraHex(true);
  evalue.SetPrintExtraHex(true);
  Print(out);
  out << " (exp. " << std::setw(evalue.PrintWidth()) << evalue;
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

Comparison* NewComparison(const std::string& c, ValueType type)
{
  std::string comparison(c);
  if (comparison.empty()) { comparison = "0ulp"; }
  if (comparison.length() >= 4 && (comparison.compare(comparison.length() - 3, 3, "ulp") == 0)) {
    unsigned ulps = str2u(comparison.substr(0, comparison.length() - 3));
    switch (type) {
    case MV_FLOAT16:
    case MV_PLAIN_FLOAT16:
      return new Comparison(CM_ULPS, Value(MV_UINT16, U16(ulps)));
    case MV_FLOAT:
      return new Comparison(CM_ULPS, Value(MV_UINT32, U32(ulps)));
    case MV_DOUBLE:
      return new Comparison(CM_ULPS, Value(MV_UINT64, U64(ulps)));
    default:
      return new Comparison(CM_DECIMAL, Value(MV_UINT64, U32(0)));
    }
  } else {
    assert(false);
    return 0;
  }
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

}

/// \todo copy-paste from HSAILTestGenEmulatorTypes
namespace HSAIL_X { // experimental

// Typesafe impl of re-interpretation of floats as unsigned integers and vice versa
namespace impl {
  template<typename T1, typename T2> union Unionier { // provides ctor for const union
    T1 val; T2 reinterpreted;
    explicit Unionier(T1 x) : val(x) {}
  };
  template<typename T> struct AsBitsTraits; // allowed pairs of types:
  template<> struct AsBitsTraits<float> { typedef uint32_t DestType; };
  template<> struct AsBitsTraits<double> { typedef uint64_t DestType; };
  template<typename T> struct AsBits {
    typedef typename AsBitsTraits<T>::DestType DestType;
    const Unionier<T,DestType> u;
    explicit AsBits(T x) : u(x) { };
    DestType Get() const { return u.reinterpreted; }
  };
  template<typename T> struct AsFloatingTraits;
  template<> struct AsFloatingTraits<uint32_t> { typedef float DestType; };
  template<> struct AsFloatingTraits<uint64_t> { typedef double DestType; };
  template<typename T> struct AsFloating {
    typedef typename AsFloatingTraits<T>::DestType DestType;
    const Unionier<T,DestType> u;
    explicit AsFloating(T x) : u(x) {}
    DestType Get() const { return u.reinterpreted; }
  };
}

template <typename T> // select T and instantiate specialized class
typename impl::AsBits<T>::DestType asBits(T f) { return impl::AsBits<T>(f).Get(); }
template <typename T>
typename impl::AsFloating<T>::DestType asFloating(T x) { return impl::AsFloating<T>(x).Get(); }


#if 0
// Typesafe impl of re-interpretation of floats as unsigned integers and vice versa
namespace impl {
  template<typename T1, typename T2> union Unionier { // provides ctor for const union
    T1 val; T2 reinterpreted;
    explicit Unionier(T1 x) : val(x) {}
  };
  template<typename T> struct ReinterpreterTraits; // allowed pairs of types
  template<> struct ReinterpreterTraits<float> { typedef uint32_t DestType; };
  template<> struct ReinterpreterTraits<double> { typedef uint64_t DestType; };
  template<> struct ReinterpreterTraits<uint32_t> { typedef float DestType; };
  template<> struct ReinterpreterTraits<uint64_t> { typedef double DestType; };
  template<typename T> struct Reinterpreter { // reinterprets within allowed pairs only
    typedef typename ReinterpreterTraits<T>::DestType DestType;
    const Unionier<T,DestType> u;
    explicit Reinterpreter(T x) : u(x) { };
    DestType Get() const { return u.reinterpreted; }
  };
}
template <typename T> // select T and instantiate specialized class
typename impl::Reinterpreter<T>::DestType reinterpret(T f) { return impl::Reinterpreter<T>(f).Get(); }
#endif

//==============================================================================
//==============================================================================
//==============================================================================
// Implementation of F16 type

f16_t::f16_t(double x, unsigned rounding /*=RND_NEAR*/) //TODO: rounding
{ 
    const FloatProp64 input(asBits(x));

    if (!input.isRegular())
    {
        bits = (f16_t::bits_t)input.mapSpecialValues<FloatProp16>();
    }
    else if (input.isSubnormal()) // f64 subnormal should be mapped to (f16)0
    {
        FloatProp16 f16(input.isPositive(), 0, FloatProp16::decodedSubnormalExponent());
        bits = f16.getBits();
    }
    else
    {
        int64_t exponent = input.decodeExponent();
        uint64_t mantissa = input.mapNormalizedMantissa<FloatProp16>();

        if (!FloatProp16::isValidExponent(exponent))
        {
            if (exponent > 0)
            {
                bits = input.isPositive()? FloatProp16::getPositiveInf() : FloatProp16::getNegativeInf();
            }
            else
            {
                uint64_t mantissa16 = input.normalizeMantissa<FloatProp16>(exponent);
                FloatProp16 f16(input.isPositive(), mantissa16, exponent);
                bits = f16.getBits();
            }
        }
        else
        {
            FloatProp16 f16(input.isPositive(), mantissa, exponent);
            bits = f16.getBits();
        }
    }
}

f16_t::f16_t(float x, unsigned rounding /*=RND_NEAR*/) //TODO: rounding
{ 
    const FloatProp32 input(asBits(x));

    if (!input.isRegular())
    {
        bits = (f16_t::bits_t)input.mapSpecialValues<FloatProp16>();
    }
    else if (input.isSubnormal()) // subnormal -> (f16)0
    {
        FloatProp16 f16(input.isPositive(), 0, FloatProp16::decodedSubnormalExponent());
        bits = f16.getBits();
    }
    else
    {
        int64_t exponent = input.decodeExponent();
        uint64_t mantissa = input.mapNormalizedMantissa<FloatProp16>();

        if (!FloatProp16::isValidExponent(exponent))
        {
            if (exponent > 0)
            {
                bits = input.isPositive()? FloatProp16::getPositiveInf() : FloatProp16::getNegativeInf();
            }
            else
            {
                uint64_t mantissa16 = input.normalizeMantissa<FloatProp16>(exponent);
                FloatProp16 f16(input.isPositive(), mantissa16, exponent);
                bits = f16.getBits();
            }
        }
        else
        {
            FloatProp16 f16(input.isPositive(), mantissa, exponent);
            bits = f16.getBits();
        }
    }
}

f64_t f16_t::f64() const 
{ 
    FloatProp16 f16(bits);
    uint64_t bits64;

    if (!f16.isRegular())
    {
        bits64 = f16.mapSpecialValues<FloatProp64>();
    }
    else if (f16.isSubnormal())
    {
        int64_t exponent = f16.decodeExponent();
        assert(exponent == FloatProp16::decodedSubnormalExponent());
        exponent = FloatProp16::actualSubnormalExponent();
        uint64_t mantissa = f16.normalizeMantissa<FloatProp64>(exponent);
        FloatProp64 f64(f16.isPositive(), mantissa, exponent);
        bits64 = f64.getBits();
    }
    else
    {
        int64_t exponent = f16.decodeExponent();
        uint64_t mantissa = f16.mapNormalizedMantissa<FloatProp64>();
        FloatProp64 f64(f16.isPositive(), mantissa, exponent);
        bits64 = f64.getBits();
    }

    return asFloating(bits64);
}

f32_t f16_t::f32() const 
{ 
    FloatProp16 f16(bits);
    uint32_t outbits;

    if (!f16.isRegular())
    {
        outbits = f16.mapSpecialValues<FloatProp32>();
    }
    else if (f16.isSubnormal())
    {
        int64_t exponent = f16.decodeExponent();
        assert(exponent == FloatProp16::decodedSubnormalExponent());
        exponent = FloatProp16::actualSubnormalExponent();
        uint64_t mantissa = f16.normalizeMantissa<FloatProp32>(exponent);
        FloatProp32 outprop(f16.isPositive(), mantissa, exponent);
        outbits = outprop.getBits();
    }
    else
    {
        int64_t exponent = f16.decodeExponent();
        uint64_t mantissa = f16.mapNormalizedMantissa<FloatProp32>();
        FloatProp32 outprop(f16.isPositive(), mantissa, exponent);
        outbits = outprop.getBits();
    }

    return asFloating(outbits);
}


} // namespace
