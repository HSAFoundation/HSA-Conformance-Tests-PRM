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

#ifndef MOBJECT_HPP
#define MOBJECT_HPP

#include <stdint.h>
#include <cstddef>
#include <ostream>
#include <iostream>
#include <vector>
#include <cassert>
#include <limits>
#include <string>
#include <map>
#include <memory>

/// For now, f16 i/o values are passed in elements of u32 arrays, in lower 16 bits.
/// f16x2 values occupy the whole 32 bits. To differientiate, use:
/// - MV_FLOAT16_MBUFFER: for plain f16 vars, size = 4 bytes
/// - MV_FLOAT16: for elemants of f16xN vars, size = 2 bytes
#define MBUFFER_KEEP_F16_AS_U32

namespace hexl {

enum ValueType { MV_INT8, MV_UINT8, MV_INT16, MV_UINT16, MV_INT32, MV_UINT32, MV_INT64, MV_UINT64, MV_FLOAT16, MV_FLOAT, MV_DOUBLE, MV_REF, MV_POINTER, MV_IMAGE, MV_SAMPLER, MV_IMAGEREF, MV_EXPR, MV_STRING, 
#ifdef MBUFFER_KEEP_F16_AS_U32
                 MV_FLOAT16_MBUFFER,
#endif
                 MV_INT8X4, MV_INT8X8, MV_UINT8X4, MV_UINT8X8, MV_INT16X2, MV_INT16X4, MV_UINT16X2, MV_UINT16X4, MV_INT32X2, MV_UINT32X2, MV_FLOAT16X2, MV_FLOAT16X4, MV_FLOATX2, MV_LAST};

enum MObjectType {
  MUNDEF = 0,
  MBUFFER,
  MRBUFFER,
  MIMAGE,
  MRIMAGE,
  MSAMPLER,
  MRSAMPLER,
  MLAST,
};

enum MObjectMem {
  MEM_NONE = 0,
  MEM_KERNARG,
  MEM_GLOBAL,
  MEM_IMAGE,
  MEM_GROUP,
  MEM_LAST,
};

enum SpecialValues {
  RV_QUEUEID = 101,
  RV_QUEUEPTR = 102,
};

class MObject {
public:
  MObject(unsigned id_, MObjectType type_, const std::string& name_) : id(id_), type(type_), name(name_) { }
  virtual ~MObject() { }

  unsigned Id() const { return id; }
  MObjectType Type() const { return type; }
  const std::string& Name() const { return name; }

  virtual void Print(std::ostream& out) const { out << id << ": '" << name << "'"; }
  virtual void SerializeData(std::ostream& out) const = 0;
private:
  MObject(const MObject&) {}

  unsigned id;
  MObjectType type;
  std::string name;
};

inline std::ostream& operator<<(std::ostream& out, MObject* mo) { mo->Print(out); return out; }

// Rudimentary type to represent f16. To be extended as needed.
typedef int16_t half;
/// \todo
/// static_cast<double>(half)
/// static_cast<float>(half)
/// static_cast<half>(double)
/// static_cast<half>(float)

typedef union {
  uint8_t u8;
  int8_t s8;
  uint16_t u16;
  int16_t s16;
  uint32_t u32;
  int32_t s32;
  uint64_t u64;
  int64_t s64;
  half h;
  float f;
  double d;
  std::ptrdiff_t o;
  void *p;
  const char *s;
  const std::string* str;
} ValueData;

size_t ValueTypeSize(ValueType type);

inline ValueData U8(uint8_t u8) { ValueData data; data.u8 = u8;  return data; }
inline ValueData S8(int8_t s8) { ValueData data; data.s8 = s8;  return data; }
inline ValueData U16(uint16_t u16) { ValueData data; data.u16 = u16;  return data; }
inline ValueData S16(int16_t s16) { ValueData data; data.s16 = s16;  return data; }
inline ValueData U32(uint32_t u32) { ValueData data; data.u32 = u32;  return data; }
inline ValueData S32(int32_t s32) { ValueData data; data.s32 = s32;  return data; }
inline ValueData U64(uint64_t u64) { ValueData data; data.u64 = u64;  return data; }
inline ValueData S64(int64_t s64) { ValueData data; data.s64 = s64;  return data; }
inline ValueData F(float f) { ValueData data; data.f = f;  return data; }
inline ValueData D(double d) { ValueData data; data.d = d;  return data; }
inline ValueData O(std::ptrdiff_t o) { ValueData data; data.o = o; return data; }
inline ValueData P(void *p) { ValueData data; data.p = p; return data; }
inline ValueData R(unsigned id) { ValueData data; data.u32 = id; return data; }
inline ValueData S(const char *s) { ValueData data; data.s = s; return data; }
inline ValueData Str(const std::string* str) { ValueData data; data.str = str; return data; }
ValueData U8X4(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
ValueData U8X8(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g, uint8_t h);
ValueData S8X4(int8_t a, int8_t b, int8_t c, int8_t d);
ValueData S8X8(int8_t a, int8_t b, int8_t c, int8_t d, int8_t e, int8_t f, int8_t g, int8_t h);
ValueData U16X2(uint16_t a, uint16_t b);
ValueData U16X4(uint16_t a, uint16_t b, uint16_t c, uint16_t d);
ValueData S16X2(int16_t a, int16_t b);
ValueData S16X4(int16_t a, int16_t b, int16_t c, int16_t d);
ValueData U32X2(uint32_t a, uint32_t b);
ValueData S32X2(int32_t a, int32_t b);
ValueData FX2(float a, float b);

class Comparison;

class Value {
public:
  Value(ValueType type_, ValueData data_) : type(type_), data(data_) {}
  Value() : type(MV_UINT64) { data.u64 = 0; }
  Value(const Value& v) : type(v.type), data(v.data) { }
  Value& operator=(const Value& v) { type = v.type; data = v.data; return *this; }
  Value(ValueType _type, uint64_t _value) : type(_type) { data.u64 = _value; }
  Value(float _value) : type(MV_FLOAT)  { data.f = _value; }
  Value(double _value) : type(MV_DOUBLE) { data.d = _value; }
  ~Value();

  ValueType Type() const { return type; }
  ValueData Data() const { return data; }
  void Print(std::ostream& out) const;

  int8_t S8() const { return data.s8; }
  uint8_t U8() const { return data.u8; }
  int16_t S16() const { return data.s16; }
  uint16_t U16() const { return data.u16; }
  int32_t S32() const { return data.s32; }
  uint32_t U32() const { return data.u32; }
  int64_t S64() const { return data.s64; }
  uint64_t U64() const { return data.u64; }
  half H() const { return data.h; }
  float F() const { return data.f; }
  double D() const { return data.d; }
  std::ptrdiff_t O() const { return data.o; }
  void *P() const { return data.p; }
  const char *S() const { return data.s; }
  const std::string& Str() const { return *data.str; }
  uint8_t U8X4(size_t index) const { assert(index < 4); return reinterpret_cast<const uint8_t *>(&(data.u32))[index]; }
  uint8_t U8X8(size_t index) const { assert(index < 8); return reinterpret_cast<const uint8_t *>(&(data.u64))[index]; }
  int8_t S8X4(size_t index) const { assert(index < 4); return reinterpret_cast<const int8_t *>(&(data.u32))[index]; }
  int8_t S8X8(size_t index) const { assert(index < 8); return reinterpret_cast<const int8_t *>(&(data.u64))[index]; }
  uint16_t U16X2(size_t index) const { assert(index < 2); return reinterpret_cast<const uint16_t *>(&(data.u32))[index]; }
  uint16_t U16X4(size_t index) const { assert(index < 4); return reinterpret_cast<const uint16_t *>(&(data.u64))[index]; }
  int16_t S16X2(size_t index) const { assert(index < 2); return reinterpret_cast<const int16_t *>(&(data.u32))[index]; }
  int16_t S16X4(size_t index) const { assert(index < 4); return reinterpret_cast<const int16_t *>(&(data.u64))[index]; }
  uint32_t U32X2(size_t index) const { assert(index < 2); return reinterpret_cast<const uint32_t *>(&(data.u64))[index]; }
  int32_t S32X2(size_t index) const { assert(index < 2); return reinterpret_cast<const int32_t *>(&(data.u64))[index]; }
  float FX2(size_t index) const { assert(index < 2); return reinterpret_cast<const float *>(&(data.u64))[index]; }
    
  size_t Size() const;
  size_t PrintWidth() const;

  void WriteTo(void *dest) const;
  void ReadFrom(const void *src, ValueType type);
  void Serialize(std::ostream& out) const;
  void Deserialize(std::istream& in);

private:
  ValueType type;
  ValueData data;
  friend class Comparison;
  friend std::ostream& operator<<(std::ostream& out, const Value& value);
  friend std::ostream& operator<<(std::ostream& out, const Comparison& comparison);

  static const unsigned PointerSize() { return sizeof(void *); }
};

typedef std::vector<Value> Values;

class EnvContext;
class ResourceManager;
class TestFactory;
class RuntimeContext;
class RuntimeContextState;
class Options;

class IndentStream : public std::streambuf {
private:
  std::streambuf*     dest;
  bool                isAtStartOfLine;
  std::string         indent;
  std::ostream*       owner;

protected:
  virtual int overflow(int ch) {
    if ( isAtStartOfLine && ch != '\n' ) {
      dest->sputn( indent.data(), indent.size() );
    }
    isAtStartOfLine = ch == '\n';
    return dest->sputc( ch );
  }

public:
  explicit IndentStream(std::streambuf* dest, int indent = 2)
    : dest(dest), isAtStartOfLine(true),
      indent(indent, ' '), owner(NULL) { }

  explicit IndentStream(std::ostream& dest, int indent = 2)
    : dest(dest.rdbuf()), isAtStartOfLine(true),
      indent(indent, ' '), owner(&dest)
    {
        owner->rdbuf(this);
    }

  virtual ~IndentStream() {
    if ( owner != NULL ) {
      owner->rdbuf(dest);
    }
  }
};

class Context {
private:
  std::map<std::string, Value> vmap;
  Context* parent;

  void *GetPointer(const std::string& key);
  const void *GetPointer(const std::string& key) const;
  void *RemovePointer(const std::string& key);

public:
  Context(Context* parent_ = 0) : parent(parent_) { }
  ~Context() { Clear(); }

  bool IsLarge() const { return sizeof(void *) == 8; }
  bool IsSmall() const { return sizeof(void *) == 4; }

  void Print(std::ostream& out) const;

  void SetParent(Context* parent) { this->parent = parent; }

  void Clear();
  bool Contains(const std::string& key) const;

  bool Has(const std::string& key) const;

  void PutPointer(const std::string& key, void *value);
  void PutValue(const std::string& key, Value value);
  void PutString(const std::string& key, const std::string& value);

  template<class T>
  void Put(const std::string& key, T* value) { PutPointer(key, value); }

  void Put(const std::string& key, Value value) { PutValue(key, value); }

  void Put(const std::string& key, const std::string& value) { PutString(key, value); }

  template<class T>
  T Get(const std::string& key) { return static_cast<T>(GetPointer(key)); }

  Value RemoveValue(const std::string& key);

  template<class T>
  T Remove(const std::string& key) { return static_cast<T>(RemovePointer(key)); }

  template<class T>
  void RemoveAndDelete(const std::string& key) { T res = Remove<T>(key); delete res; }

  template<class T>
  const T* GetConst(const std::string& key) const { return static_cast<const T*>(GetPointer(key)); }

  Value GetValue(const std::string& key);
  std::string GetString(const std::string& key);

  // Helper methods. Include HexlTest.hpp to use them.
  EnvContext* Env() { return Get<EnvContext*>("hexl.env"); }
  ResourceManager* RM() { return Get<ResourceManager*>("hexl.rm"); }
  TestFactory* Factory() { return Get<TestFactory*>("hexl.testFactory"); }
  RuntimeContextState* State() { return Get<RuntimeContextState*>("hexl.runtimestate"); }
  RuntimeContext* Runtime() { return Get<RuntimeContext*>("hexl.runtime"); }
  const Options* Opts() const { return GetConst<Options>("hexl.options"); }

  // Logging helpers.
  std::ostream& Debug() { return *Get<std::ostream*>("hexl.log.stream.debug"); }
  std::ostream& Info() { return *Get<std::ostream*>("hexl.log.stream.info"); }
  std::ostream& Error() { return *Get<std::ostream*>("hexl.log.stream.error"); }

  // Dumping helpers.
  bool IsVerbose(const std::string& what) const;
  bool IsDumpEnabled(const std::string& what, bool dumpAll = true) const;
  void SetOutputPath(const std::string& path) { Put("hexl.outputPath", path); }
  std::string GetOutputName(const std::string& name, const std::string& what);
  bool DumpTextIfEnabled(const std::string& name, const std::string& what, const std::string& text);
  bool DumpBinaryIfEnabled(const std::string& name, const std::string& what, const void *buffer, size_t bufferSize);
  void DumpBrigIfEnabled(const std::string& name, void* brig);
};

void WriteTo(void *dest, const Values& values);
void ReadFrom(void *dest, ValueType type, size_t count, Values& values);
void SerializeValues(std::ostream& out, const Values& values);
void DeserializeValues(std::istream& in, Values& values);

class MBuffer : public MObject {
public:
  static const uint32_t default_size[3];
  MBuffer(unsigned id, const std::string& name, MObjectMem mtype_, ValueType vtype_, unsigned dim_ = 1, const uint32_t *size_ = default_size) : MObject(id, MBUFFER, name), mtype(mtype_), vtype(vtype_), dim(dim_) {
    for (unsigned i = 0; i < 3; ++i) { size[i] = (i < dim) ? size_[i] : 1; }
  }
  MBuffer(unsigned id, const std::string& name, std::istream& in) : MObject(id, MBUFFER, name) { DeserializeData(in); }

  MObjectMem MType() const { return mtype; }
  ValueType VType() const { return vtype; }
  void Print(std::ostream& out) const;
  unsigned Dim() const { return dim; }
  size_t Count() const { return size[0] * size[1] * size[2]; }
  size_t Size(Context* context) const;
  Value GetRaw(size_t i) { return data[i]; }

  Values& Data() { return data; }
  const Values& Data() const { return data; }

  std::string GetPosStr(size_t pos) const;
  void PrintComparisonInfo(std::ostream& out, size_t pos, Comparison& comparison, bool detailed = false) const;
  void PrintComparisonSummary(std::ostream& out, Comparison& comparison) const;
  virtual void SerializeData(std::ostream& out) const;

private:
  MObjectMem mtype;
  ValueType vtype;
  unsigned dim;
  uint32_t size[3];
  Values data;

  size_t GetDim(size_t pos, unsigned d) const;
  void DeserializeData(std::istream& in);
};

inline MBuffer* NewMValue(unsigned id, const std::string& name, MObjectMem mtype, ValueType vtype, ValueData data) {
  uint32_t size[3] = { 1, 1, 1 };
  MBuffer* mb = new MBuffer(id, name, mtype, vtype, 1, size);
  mb->Data().push_back(Value(vtype, data));
  return mb;
}

std::ostream& operator<<(std::ostream& out, const Value& value);

enum ComparisonMethod {
  CM_DECIMAL,  // precision = decimal points, default
  CM_ULPS,     // precision = max ULPS, minumum is 1
  CM_RELATIVE  // (simulated_value - expected_value) / expected_value <= precision
};

class Comparison {
public:
  static const int F64_DEFAULT_DECIMAL_PRECISION = 14; // Default F64 arithmetic precision (if not set in results) 
  static const int F32_DEFAULT_DECIMAL_PRECISION = 5;  // Default F32 arithmetic precision (if not set in results)
  static const int F16_DEFAULT_DECIMAL_PRECISION = 4;  // Default F16 arithmetic precision (if not set in results)
#if defined(LINUX)
  static const int F64_MAX_DECIMAL_PRECISION = 17; // Max F64 arithmetic precision
  static const int F32_MAX_DECIMAL_PRECISION = 9; // Max F32 arithmetic precision
  static const int F32_MAX_DECIMAL_PRECISION = 5; // Max F16 arithmetic precision
#else
  static const int F64_MAX_DECIMAL_PRECISION = std::numeric_limits<double>::max_digits10; // Max F64 arithmetic precision
  static const int F32_MAX_DECIMAL_PRECISION = std::numeric_limits<float>::max_digits10; // Max F32 arithmetic precision
  static const int F16_MAX_DECIMAL_PRECISION = std::numeric_limits<float>::max_digits10 - 4; // 23 bit vs 10 bit mantissa (f32 vs f16)
#endif
  static const uint32_t F_DEFAULT_ULPS_PRECISION;
  static const double F_DEFAULT_RELATIVE_PRECISION;
  Comparison() : method(CM_DECIMAL), result(false) { }
  Comparison(ComparisonMethod method_, const Value& precision_) : method(method_), precision(precision_), result(false) { }
  void Reset(ValueType type);
  void SetDefaultPrecision(ValueType type);
  bool GetResult() const { return result; }
  bool IsFailed() const { return failed != 0; }
  unsigned GetFailed() const { return failed; }
  unsigned GetChecks() const { return checks; }
  std::string GetMethodDescription() const;
  Value GetMaxError() const { return maxError; }
  size_t GetMaxErrorIndex() const { return maxErrorIndex; }
  Value GetError() const { return error; }
  void PrintDesc(std::ostream& out) const;
  void Print(std::ostream& out) const;
  void PrintShort(std::ostream& out) const;
  void PrintLong(std::ostream& out) const;

  bool Compare(const Value& evalue, const Value& rvalue);

private:
  ComparisonMethod method;
  Value precision;
  bool result;
  Value error;
  Value evalue, rvalue;

  unsigned checks, failed;
  Value maxError;
  size_t maxErrorIndex;
};

std::ostream& operator<<(std::ostream& out, const Comparison& comparison);

class MRBuffer : public MObject {
public:
  MRBuffer(unsigned id, const std::string& name, ValueType vtype_, unsigned refid_, const Values& data_ = Values(), const Comparison& comparison_ = Comparison()) :
    MObject(id, MRBUFFER, name), vtype(vtype_), refid(refid_), data(data_), comparison(comparison_) { }
  MRBuffer(unsigned id, const std::string& name, std::istream& in) : MObject(id, MRBUFFER, name) { DeserializeData(in); }

  ValueType VType() const { return vtype; }
  void Print(std::ostream& out) const;
  unsigned RefId() const { return refid; }
  Values& Data() { return data; }
  const Values& Data() const { return data; }
  Comparison& GetComparison() { return comparison; }
  virtual void SerializeData(std::ostream& out) const;

private:
  ValueType vtype;
  unsigned refid;
  Values data;
  Comparison comparison;

  void DeserializeData(std::istream& in);
};

inline MRBuffer* NewMRValue(unsigned id, MBuffer* mb, Value data, const Comparison& comparison = Comparison()) {
  MRBuffer* mr = new MRBuffer(id, mb->Name() + " (check)", mb->VType(), mb->Id(), Values(), comparison);
  mr->Data().push_back(data);
  return mr;
}

inline MRBuffer* NewMRValue(unsigned id, MBuffer* mb, ValueData data, const Comparison& comparison = Comparison()) {
  return NewMRValue(id, mb, Value(mb->VType(), data), comparison);
}

ValueType ImageValueType(unsigned geometry);

class MImage : public MObject {
public:
  MImage(unsigned id, const std::string& name, unsigned geometry_, unsigned chanel_order_, unsigned channel_type_, unsigned access_, size_t width_, size_t height_, size_t depth_, size_t rowPitch_, size_t slicePitch_)
    : MObject(id, MIMAGE, name),
      geometry(geometry_), channelOrder(chanel_order_), channelType(channel_type_), accessPermission(access_), width(width_), height(height_),
      depth(depth_), rowPitch(rowPitch_), slicePitch(slicePitch_) { }
  MImage(unsigned id, const std::string& name, std::istream& in) : MObject(id, MIMAGE, name) { DeserializeData(in); }

  unsigned Geometry() const { return geometry; }
  unsigned ChannelOrder() const { return channelOrder; }
  unsigned ChannelType() const { return channelType; }
  unsigned AccessPermission() const { return accessPermission; } 
    
  size_t Width() const { return width; }
  size_t Height() const { return height; }
  size_t Depth() const { return depth; }
  size_t RowPitch() const { return rowPitch; }
  size_t SlicePitch() const { return slicePitch; }
  size_t Size() const { return height * width * depth; }
  Value GetRaw(size_t i) { assert(false); }

  Values& Data() { return data; }
  const Values& Data() const { return data; }

  ValueType GetValueType() const { return ImageValueType(geometry); }

  void PrintComparisonInfo(std::ostream& out, size_t pos, Comparison& comparison) const;
  void PrintComparisonSummary(std::ostream& out, Comparison& comparison) const;
  virtual void SerializeData(std::ostream& out) const;

private:
  unsigned accessPermission;
  unsigned geometry;
  unsigned channelOrder;
  unsigned channelType;
  size_t width, height, depth, rowPitch, slicePitch;
  Values data;

  void DeserializeData(std::istream& in);
};

class MRImage : public MObject {
public:
  MRImage(unsigned id, const std::string& name, unsigned geometry_, unsigned refid_, const Comparison& comparison_) :
    MObject(id, MRIMAGE, name), geometry(geometry_), refid(refid_), comparison(comparison_) { }
  MRImage(unsigned id, const std::string& name, std::istream& in) : MObject(id, MRIMAGE, name) { DeserializeData(in); }

  unsigned Geometry() const { return geometry; }
  unsigned RefId() const { return refid; }
  Values& Data() { return data; }
  const Values& Data() const { return data; }
  Comparison& GetComparison() { return comparison; }
  virtual void SerializeData(std::ostream& out) const;

private:
  unsigned geometry;
  unsigned refid;
  Values data;
  Comparison comparison;
  void DeserializeData(std::istream& in);
};

inline MImage* NewMValue(unsigned id, const std::string& name, unsigned geometry, unsigned chanel_order, unsigned channel_type, unsigned access, 
                         size_t width, size_t height, size_t depth, size_t rowPitch, size_t slicePitch, ValueData data) {
  MImage* mi = new MImage(id, name, geometry, chanel_order, channel_type, access, 
                         width, height, depth, rowPitch, slicePitch);
  for (int i = 0; i < mi->Size(); i++)
  {
    mi->Data().push_back(Value(MV_UINT8, data));
  }
  return mi;
}

inline MRImage* NewMRValue(unsigned id, MImage* mi, Value data, const Comparison& comparison = Comparison()) {
  MRImage* mr = new MRImage(id, mi->Name() + " (check)", mi->Geometry(), mi->Id(), comparison);
  mr->Data().push_back(data);
  return mr;
}

inline MRImage* NewMRValue(unsigned id, MImage* mi, ValueData data, const Comparison& comparison = Comparison()) {
  return NewMRValue(id, mi, Value(mi->GetValueType(), data), comparison);
}

class MSampler : public MObject {
public:
  MSampler(unsigned id, const std::string& name, unsigned coords_, unsigned filter_, unsigned addressing_)
    : MObject(id, MSAMPLER, name),
      coords(coords_), filter(filter_), addressing(addressing_) { }
  MSampler(unsigned id, const std::string& name, std::istream& in) : MObject(id, MSAMPLER, name) { DeserializeData(in); }

  unsigned Coords() const { return coords; }
  unsigned Filter() const { return filter; }
  unsigned Addressing() const { return addressing; }
  size_t Size() const { return sizeof(void*); }
  Value GetRaw(size_t i) { assert(false); }

  ValueType VType() const { return MV_SAMPLER; }

  Values& Data() { assert(false); }
  const Values& Data() const { assert(false); }

  void PrintComparisonInfo(std::ostream& out, size_t pos, Comparison& comparison) const;
  void PrintComparisonSummary(std::ostream& out, Comparison& comparison) const;
  virtual void SerializeData(std::ostream& out) const;

private:
  unsigned coords;
  unsigned filter;
  unsigned addressing;

  void DeserializeData(std::istream& in);
};

class MRSampler : public MObject {
public:
  MRSampler(unsigned id, const std::string& name, unsigned refid_, const Comparison& comparison_) :
    MObject(id, MRSAMPLER, name), refid(refid_), comparison(comparison_) { }
  MRSampler(unsigned id, const std::string& name, std::istream& in) : MObject(id, MRSAMPLER, name) { DeserializeData(in); }

  unsigned RefId() const { return refid; }
  Comparison& GetComparison() { return comparison; }
  virtual void SerializeData(std::ostream& out) const;

private:
  unsigned refid;
  Comparison comparison;
  void DeserializeData(std::istream& in);
};

inline MSampler* NewMValue(unsigned id, const std::string& name, unsigned coords_, unsigned filter_, unsigned addressing_) {
  MSampler* ms = new MSampler(id, name, coords_, filter_, addressing_);
  return ms;
}

inline MRSampler* NewMRValue(unsigned id, MSampler* ms, const Comparison& comparison = Comparison()) {
  MRSampler* mr = new MRSampler(id, ms->Name() + " (check)", ms->Id(), comparison);
  return mr;
}

class MemorySetup {
private:
  std::vector<std::unique_ptr<MObject>> mos;

public:
  unsigned Count() const { return (unsigned) mos.size(); }
  MObject* Get(size_t i) const { return mos[i].get(); }
  void Add(MObject* mo) { mos.push_back(std::move(std::unique_ptr<MObject>(mo))); }
  void Print(std::ostream& out) const;
  void Serialize(std::ostream& out) const;
  void Deserialize(std::istream& in);
};

typedef std::vector<MObject*> MemState;

class DispatchSetup {
public:
  DispatchSetup()
    : dimensions(0) {
    for (unsigned i = 0; i < 3; ++i) {
      gridSize[i] = 0;
      workgroupSize[i] = 0;
      globalOffset[i] = 0;
    }
  }

  void Print(std::ostream& out) const;
      
  void SetDimensions(uint32_t dimensions) { this->dimensions = dimensions; }
  uint32_t Dimensions() const { return dimensions; }

  void SetGridSize(const uint32_t* size) {
    gridSize[0] = size[0];
    gridSize[1] = size[1];
    gridSize[2] = size[2];
  }

  void SetGridSize(uint32_t x, uint32_t y = 1, uint32_t z = 1) {
    gridSize[0] = x;
    gridSize[1] = y;
    gridSize[2] = z;
  }

  uint32_t GridSize(unsigned dim) const { assert(dim < 3); return gridSize[dim]; }
  const uint32_t* GridSize() const { return gridSize; }

  void SetWorkgroupSize(const uint16_t* size) {
    workgroupSize[0] = size[0];
    workgroupSize[1] = size[1];
    workgroupSize[2] = size[2];
  }
  void SetWorkgroupSize(uint16_t x, uint16_t y = 1, uint16_t z = 1) {
    workgroupSize[0] = x;
    workgroupSize[1] = y;
    workgroupSize[2] = z;
  }
  uint16_t WorkgroupSize(unsigned dim) const { assert(dim < 3); return workgroupSize[dim]; }
  const uint16_t* WorkgroupSize() const { return workgroupSize; }

  void SetGlobalOffset(const uint64_t* size) {
    globalOffset[0] = size[0];
    globalOffset[1] = size[1];
    globalOffset[2] = size[2];
  }
  void SetGlobalOffset(uint64_t x, uint64_t y = 0, uint64_t z = 0) {
    globalOffset[0] = x;
    globalOffset[1] = y;
    globalOffset[2] = z;
  }

  uint64_t GlobalOffset(unsigned dim) const { assert(dim < 3); return globalOffset[dim]; }
  const uint64_t* GlobalOffset() const { return globalOffset; }

  const MemorySetup& MSetup() const { return msetup; }
  MemorySetup& MSetup() { return msetup; }

  void Serialize(std::ostream& out) const;
  void Deserialize(std::istream& in);

private:
  uint32_t dimensions;
  uint32_t gridSize[3];
  uint16_t workgroupSize[3];
  uint64_t globalOffset[3];
  MemorySetup msetup;
};

// Serialization helpers
template <typename T>
class Serializer;

template <typename T>
inline void WriteData(std::ostream& out, const T* value) { Serializer<T>::Write(out, value); }

template <typename T>
inline void WriteData(std::ostream& out, const T& value) { Serializer<T>::Write(out, value); }

template <typename T>
inline void ReadData(std::istream& in, T& value)
{
  Serializer<T>::Read(in, value);
}

#define RAW_SERIALIZER(T) \
template <> \
struct Serializer<T> { \
  static void Write(std::ostream& out, const T& value) { out.write(reinterpret_cast<const char *>(&value), sizeof(T));  } \
  static void Read(std::istream& in, T& value) { in.read(reinterpret_cast<char *>(&value), sizeof(T)); } \
};

#define ENUM_SERIALIZER(T) \
template <> \
struct Serializer<T> { \
  static void Write(std::ostream& out, const T& value) { WriteData(out, (uint32_t) value); } \
  static void Read(std::istream& in, T& value) { uint32_t v; ReadData(in, v); value = (T) v; } \
};

#define MEMBER_SERIALIZER(T) \
template <> \
struct Serializer<T> { \
  static void Write(std::ostream& out, const T& value) { value.Serialize(out); } \
  static void Read(std::istream& in, T& value) { value.Deserialize(in); } \
};

template <typename T>
struct Serializer<std::vector<T>> {
  static void Write(std::ostream& out, const std::vector<T>& value) {
    uint64_t size = value.size();
    WriteData(out, size);
    for (unsigned i = 0; i < size; ++i) { WriteData(out, value[i]); }
  }
  static void Read(std::istream& in, std::vector<T>& value) {
    uint64_t size;
    ReadData(in, size);
    value.resize((size_t) size);
    for (unsigned i = 0; i < size; ++i) { ReadData(in, value[i]); }
  }
};

RAW_SERIALIZER(uint16_t);
RAW_SERIALIZER(uint32_t);
RAW_SERIALIZER(uint64_t);
ENUM_SERIALIZER(MObjectType);
ENUM_SERIALIZER(MObjectMem);
ENUM_SERIALIZER(ValueType);

template <>
struct Serializer<std::string> {
  static void Write(std::ostream& out, const std::string& value) {
    uint32_t length = (uint32_t) value.length();
    WriteData(out, length);
    out << value;
  }

  static void Read(std::istream& in, std::string& value) {
    uint32_t length;
    ReadData(in, length);
    char *buffer = new char[length];
    in.read(buffer, length);
    value = std::string(buffer, length);
    delete buffer;
  }
};

template <>
struct Serializer<MObject*> {
  static void Write(std::ostream& out, const MObject* mo) {
    WriteData(out, mo->Id());
    WriteData(out, mo->Type());
    WriteData(out, mo->Name());
    mo->SerializeData(out);
  }

  static void Read(std::istream& in, MObject*& mo) {
    unsigned id;
    MObjectType type;
    std::string name;
    ReadData(in, id);
    ReadData(in, type);
    ReadData(in, name);
    switch (type) {
    case MBUFFER: mo = new MBuffer(id, name, in); break;
    case MRBUFFER: mo = new MRBuffer(id, name, in); break;
    case MIMAGE: mo = new MImage(id, name, in); break;
    case MRIMAGE: mo = new MRImage(id, name, in); break;
    default: assert(false); mo = 0; break;
    }
  }
};

MEMBER_SERIALIZER(MemorySetup);
MEMBER_SERIALIZER(DispatchSetup);
MEMBER_SERIALIZER(Value);

}

#endif // MOBJECT_HPP
