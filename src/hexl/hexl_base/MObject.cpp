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
#include "HSAILFloats.h"
#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cmath>

#ifdef _WIN32
#define isnan _isnan
#endif // _WIN32

namespace hexl {

template <typename T>
static int is_inf(T const& x)
{
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
#ifdef MBUFFER_KEEP_F16_AS_U32
  case MV_FLOAT16_MBUFFER: return 4;
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
  case MV_IMAGE:
    return sizeof(void *);
  case MV_REF:
    return 4;
  case MV_IMAGEREF:
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

ValueData FX2(float a, float b) { 
  std::vector<float> vector(2);   
  vector[0] = a; vector[1] = b;
  ValueData data; 
  data.u64 = *reinterpret_cast<uint64_t *>(vector.data());
  return data;
}

ValueData HX2(half a, half b) {
  std::vector<half> vector(2);
  vector[0] = a; vector[1] = b;
  ValueData data;
  data.u32 = *reinterpret_cast<uint32_t *>(vector.data());
  return data;
}

ValueData HX4(half a, half b, half c, half d) {
  std::vector<half> vector(4);
  vector[0] = a; vector[1] = b; vector[2] = c; vector[3] = d;
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
#ifdef MBUFFER_KEEP_F16_AS_U32
  case MV_FLOAT16_MBUFFER:
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
  case MV_FLOAT16X2: return "halfx2";
  case MV_FLOAT16X4: return "halfx4";
  case MV_FLOATX2: return "floatx2";
  case MV_IMAGE: return "image";
  case MV_REF: return "ref";
  case MV_IMAGEREF: return "imageref";
  case MV_POINTER: return "pointer";
  case MV_EXPR: return "expr";
  case MV_STRING: return "string";
  default:
    assert(false); return "<unknown type>";
  }
}

size_t ValueTypePrintWidth(ValueType type)
{
  switch (type) {
  case MV_INT8: return 3;
  case MV_UINT8: return 3;
  case MV_INT16: return 5;
  case MV_UINT16: return 5;
  case MV_INT32: return 10;
  case MV_UINT32: return 10;
  case MV_INT64: return 18;
  case MV_UINT64: return 18;
#ifdef MBUFFER_KEEP_F16_AS_U32
  case MV_FLOAT16_MBUFFER:
#endif
  case MV_FLOAT16: return 10;
  case MV_FLOAT: return 10;
  case MV_DOUBLE: return 18;
  case MV_INT8X4: return 8 + 4 * ValueTypePrintWidth(MV_INT8);
  case MV_INT8X8: return 16 + 8 * ValueTypePrintWidth(MV_INT8);
  case MV_UINT8X4: return 8 + 4 * ValueTypePrintWidth(MV_UINT8);
  case MV_UINT8X8: return 16 + 8 * ValueTypePrintWidth(MV_UINT8);
  case MV_INT16X2: return 4 + 2 * ValueTypePrintWidth(MV_INT16);
  case MV_INT16X4: return 8 + 4 * ValueTypePrintWidth(MV_INT16);
  case MV_UINT16X2: return 4 + 2 * ValueTypePrintWidth(MV_UINT16);
  case MV_UINT16X4: return 8 + 4 * ValueTypePrintWidth(MV_UINT16);
  case MV_INT32X2: return 4 + 2 * ValueTypePrintWidth(MV_INT32);
  case MV_UINT32X2: return 4 + 2 * ValueTypePrintWidth(MV_UINT32);
  case MV_FLOAT16X2: return 4 + 2 * ValueTypePrintWidth(MV_FLOAT16);
  case MV_FLOAT16X4: return 8 + 4 * ValueTypePrintWidth(MV_FLOAT16);
  case MV_FLOATX2: return 4 + 2 * ValueTypePrintWidth(MV_FLOAT);
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

static bool isnan_half(const half) { return false; } /// \todo
static bool is_inf_half(const half) { return false; } /// \todo

std::ostream& PrintHalf(half h, uint16_t bits, std::ostream& out){
  if (isnan_half(h) || is_inf_half(h)) {
    out << (isnan_half(h) ? "NAN" : "INF");
  } else {
    out << std::setprecision(Comparison::F16_MAX_DECIMAL_PRECISION) << (float)h;
  }
  out << " (0x" << std::hex << bits << ")" << std::dec;
  return out;
};

std::ostream& PrintFloat(float f, uint32_t bits, std::ostream& out){
  if (isnan(f) || is_inf(f)) {
    out << (isnan(f) ? "NAN" : "INF");
  } else {
    out << std::setprecision(Comparison::F32_MAX_DECIMAL_PRECISION) << f;
  }
  out << " (0x" << std::hex << bits << ")" << std::dec;
  return out;
};

std::ostream& PrintDouble(double d, uint64_t bits, std::ostream& out){
  if (isnan(d) || is_inf(d)) {
    out << (isnan(d) ? "NAN" : "INF");
  } else {
    out << std::setprecision(Comparison::F64_MAX_DECIMAL_PRECISION) << d;
  }
  out << " (0x" << std::hex << bits << ")" << std::dec;
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
#ifdef MBUFFER_KEEP_F16_AS_U32
  case MV_FLOAT16_MBUFFER:
#endif
  case MV_FLOAT16:
    PrintHalf(H(), U16(), out);
    break;
  case MV_FLOAT:
    PrintFloat(F(), U32(), out);
    break;
  case MV_DOUBLE:
    PrintDouble(D(), U64(), out);
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
    out << "(" << PrintFloat(FX2(0), U32X2(0), out) << ", " << PrintFloat(FX2(1), U32X2(1), out) << ")";
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
#ifdef MBUFFER_KEEP_F16_AS_U32
  case MV_FLOAT16_MBUFFER:
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
#ifdef MBUFFER_KEEP_F16_AS_U32
  case MV_FLOAT16_MBUFFER: ((half *) dest)[0] = data.h; ((uint16_t *) dest)[1] = 0; break;
#endif
  case MV_FLOAT16: *((half *) dest) = data.h; break;
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
#ifdef MBUFFER_KEEP_F16_AS_U32
  case MV_FLOAT16_MBUFFER:
#endif
  case MV_FLOAT16: data.h = *((half *) src); break;
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

std::string MBuffer::GetPosStr(size_t pos) const
{
  std::stringstream ss;
  switch (dim) {
  case 1: ss << "[" << pos << "]"; break;
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


ValueType ImageValueType(unsigned geometry)
{
  assert(false);
  return MV_LAST;
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
#ifdef MBUFFER_KEEP_F16_AS_U32
    case MV_FLOAT16_MBUFFER:
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
    case MV_FLOAT16X2: maxError = Value(MV_FLOAT16X2, HX2(0, 0)); break;
    case MV_FLOAT16X4: maxError = Value(MV_FLOAT16X4, HX4(0, 0, 0, 0)); break;
    default: maxError = Value(type, U64((uint64_t) 0)); break;
    }
    break;
  case CM_ULPS:
    switch (type) {
    case MV_FLOAT: maxError = Value(MV_UINT32, U32(0)); break;
    case MV_DOUBLE: maxError = Value(MV_UINT64, U64(0)); break;
    case MV_FLOATX2: maxError = Value(MV_UINT32X2, U32X2(0, 0)); break;
#ifdef MBUFFER_KEEP_F16_AS_U32
    case MV_FLOAT16_MBUFFER:
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
#ifdef MBUFFER_KEEP_F16_AS_U32
    case MV_FLOAT16_MBUFFER:
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
#ifdef MBUFFER_KEEP_F16_AS_U32
    case MV_FLOAT16_MBUFFER:
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
#ifdef MBUFFER_KEEP_F16_AS_U32
    case MV_FLOAT16_MBUFFER:
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
  } else if (is_inf_half(v1.H()) || is_inf_half(v2.H())) {
    res = is_inf_half(v1.H()) == is_inf_half(v2.H());
    compared = true;
  } else if (v1.H() == 0 && v2.H() == 0) { // ignore sign of 0
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
    return error.U32() <= precision.D();
  }
  case CM_RELATIVE: {
    double eps = precision.D();
    if (v1.H() == 0) {
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
      error = Value(MV_DOUBLE, F(res ? 0.0f : 1.0f));
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
    return error.U32() <= precision.D();
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
#ifdef MBUFFER_KEEP_F16_AS_U32
  case MV_FLOAT16_MBUFFER:
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
      return error.U64() <= precision.D();
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

void Comparison::PrintLong(std::ostream& out) const
{
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

void MemorySetup::Print(std::ostream& out) const
{
  for (const std::unique_ptr<MObject>& mo : mos) {
    mo->Print(out); out << std::endl;
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

void DispatchSetup::Print(std::ostream& out) const
{
  {
    out << "Dispatch setup:" << std::endl;
    IndentStream indent(out);
    out << "Dimensions: " << dimensions << std::endl
        << "Grid:       " << "(" << gridSize[0] << ", " << gridSize[1] << ", " << gridSize[2] << ")" << std::endl
        << "Workgroup:  " << "(" << workgroupSize[0] << ", " << workgroupSize[1] << ", " << workgroupSize[2] << ")" << std::endl
        << "Offsets:    " << "(" << globalOffset[0] << ", " << globalOffset[1] << ", " << globalOffset[2] << ")" << std::endl;
  }
  {
    out << "Memory setup:" << std::endl;
    IndentStream indent(out);
    msetup.Print(out);
  }
}

void DispatchSetup::Serialize(std::ostream& out) const
{
  WriteData(out, dimensions);
  for (unsigned i = 0; i < 3; ++i) {
    WriteData(out, gridSize[i]);
    WriteData(out, workgroupSize[i]);
    WriteData(out, globalOffset[i]);
  }
  WriteData(out, msetup);
}

void DispatchSetup::Deserialize(std::istream& in)
{
  ReadData(in, dimensions);
  for (unsigned i = 0; i < 3; ++i) {
    ReadData(in, gridSize[i]);
    ReadData(in, workgroupSize[i]);
    ReadData(in, globalOffset[i]);
  }
  ReadData(in, msetup);
}

}
