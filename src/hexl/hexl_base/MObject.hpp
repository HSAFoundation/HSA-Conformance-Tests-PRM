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

//#include "HSAILTestGenEmulatorTypes.h" // for f16_t & related

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

/// For now, plain f16 i/o values are passed to kernels in elements of u32 arrays, in lower 16 bits.
/// f16x2 values occupy the whole 32 bits. To differientiate, use:
/// - MV_PLAIN_FLOAT16: for plain f16 vars, size = 4 bytes
/// - MV_FLOAT16: for elemants of f16xN vars, size = 2 bytes
#define MBUFFER_PASS_PLAIN_F16_AS_U32

namespace hexl {

enum ValueType { MV_INT8, MV_UINT8, MV_INT16, MV_UINT16, MV_INT32, MV_UINT32, MV_INT64, MV_UINT64, MV_FLOAT16, MV_FLOAT, MV_DOUBLE, MV_REF, MV_POINTER, MV_IMAGE, MV_SAMPLER, MV_IMAGEREF, MV_SAMPLERREF, MV_EXPR, MV_STRING, 
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
                 MV_PLAIN_FLOAT16,
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

enum MObjectImageGeometry {
  IMG_1D = 0,
  IMG_2D,
  IMG_3D,
  IMG_1DA,
  IMG_2DA,
  IMG_1DB,
  IMG_2DDEPTH,
  IMG_2DADEPTH
};

enum MObjectImageChannelType {
  IMG_SNORM_INT8 = 0,
  IMG_SNORM_INT16,
  IMG_UNORM_INT8,
  IMG_UNORM_INT16,
  IMG_UNORM_INT24,
  IMG_UNORM_SHORT_555,
  IMG_UNORM_SHORT_565,
  IMG_UNORM_INT_101010,
  IMG_SIGNED_INT8,
  IMG_SIGNED_INT16,
  IMG_SIGNED_INT32,
  IMG_UNSIGNED_INT8,
  IMG_UNSIGNED_INT16,
  IMG_UNSIGNED_INT32,
  IMG_HALF_FLOAT,
  IMG_FLOAT
};

enum MObjectImageChannelOrder {
  IMG_ORDER_A = 0,
  IMG_ORDER_R,
  IMG_ORDER_RX,
  IMG_ORDER_RG,
  IMG_ORDER_RGX,
  IMG_ORDER_RA,
  IMG_ORDER_RGB,
  IMG_ORDER_RGBX,
  IMG_ORDER_RGBA,
  IMG_ORDER_BRGA,
  IMG_ORDER_ARGB,
  IMG_ORDER_ABGR,
  IMG_ORDER_SRGB,
  IMG_ORDER_SRGBX,
  IMG_ORDER_SRGBA,
  IMG_ORDER_SBGRA,
  IMG_ORDER_INTENSITY,
  IMG_ORDER_LUMINANCE,
  IMG_ORDER_DEPTH,
  IMG_ORDER_DEPTH_STENCIL
};

enum MObjectImageAccess {
  IMG_ACCESS_NOT_SUPPORTED = 0x0,
  IMG_ACCESS_READ_ONLY,
  IMG_ACCESS_WRITE_ONLY,
  IMG_ACCESS_READ_WRITE,
  IMG_ACCESS_READ_MODIFY_WRITE = 0x8
};

enum MObjectImageQuery {
    IMG_QUERY_WIDTH = 0,
    IMG_QUERY_HEIGHT,
    IMG_QUERY_DEPTH,
    IMG_QUERY_ARRAY,
    IMG_QUERY_CHANNELORDER,
    IMG_QUERY_CHANNELTYPE
};

enum MObjectSamplerFilter {
  SMP_NEAREST = 0,
  SMP_LINEAR
};

enum MObjectSamplerCoords {
  SMP_UNNORMALIZED = 0,
  SMP_NORMALIZED,
};

enum MObjectSamplerAddressing {
  SMP_UNDEFINED = 0,
  SMP_CLAMP_TO_EDGE,
  SMP_CLAMP_TO_BORDER,
  SMP_MODE_REPEAT,
  SMP_MIRRORED_REPEAT
};

enum MObjectSamplerQuery {
  SMP_QUERY_ADDRESSING = 0,
  SMP_QUERY_COORD,
  SMP_QUERY_FILTER
};

enum SpecialValues {
  RV_QUEUEID = 101,
  RV_QUEUEPTR = 102,
};

const char *ImageGeometryString(MObjectImageGeometry mem);
const char *ImageChannelTypeString(MObjectImageChannelType mem);
const char *ImageChannelOrderString(MObjectImageChannelOrder mem);
const char *ImageAccessString(MObjectImageAccess mem);
const char *ImageQueryString(MObjectImageQuery mem);
const char *SamplerFilterString(MObjectSamplerFilter mem);
const char *SamplerCoordsString(MObjectSamplerCoords mem);
const char *SamplerAddressingString(MObjectSamplerAddressing mem);
const char *SamplerQueryString(MObjectSamplerQuery mem);

class MObject {
public:
  MObject(unsigned id_, MObjectType type_, const std::string& name_) : id(id_), type(type_), name(name_) { }
  virtual ~MObject() { }

  unsigned Id() const { return id; }
  MObjectType Type() const { return type; }
  const std::string& Name() const { return name; }

  virtual void Print(std::ostream& out) const { out << id << ": '" << name << "'"; }
  virtual void PrintWithBuffer(std::ostream& out) const { Print(out); } // type-specific, so nothing by default
  virtual void SerializeData(std::ostream& out) const = 0;
private:
  MObject(const MObject&) {}

  unsigned id;
  MObjectType type;
  std::string name;
};

inline std::ostream& operator<<(std::ostream& out, MObject* mo) { mo->Print(out); return out; }

}
/// \todo copy-paste from HSAILTestGenEmulatorTypes
#include "Brig.h"
namespace HSAIL_X { // eXperimental

// ============================================================================
// ============================================================================
// ============================================================================
// Types representing HSAIL values

typedef float    f32_t;
typedef double   f64_t;

//=============================================================================
//=============================================================================
//=============================================================================
// Decoder of IEEE 754 float properties

// IEEE 754 (32-bit)
//
//         S   (E-127)       23
// F = (-1) * 2        (1+M/2  )
//
// S - sign
// E - exponent
// M - mantissa
//
// For regular numbers, normalized exponent (E-127) must be in range [-126, 127]
// Value -127 is reserved for subnormals
// Value +128 is reserved for infinities and NaNs
//
// Special cases:
//  ----------------------------------------------
//  | S EXP MANTISSA  | MEANING         | NOTES  |
//  ----------------------------------------------
//  | 0 000 00.....0  | +0              |        |
//  | 1 000 00.....0  | -0              |        |
//  | 0 111 00.....0  | +INF            |        |
//  | 1 111 00.....0  | -INF            |        |
//  | X 111 1?.....?  | NAN (quiet)     |        |
//  | X 111 0?.....?  | NAN (signaling) | M != 0 |
//  | X 000 ??.....?  | SUBNORMAL       | M != 0 |
//  ----------------------------------------------
//

template<typename T, T sign_mask, T exponent_mask, T mantissa_mask, T mantissa_msb_mask, int exponent_bias, int mantissa_width>
class IEEE754
{
public:
    typedef T Type;

    static const Type SIGN_MASK         = sign_mask;
    static const Type EXPONENT_MASK     = exponent_mask;
    static const Type MANTISSA_MASK     = mantissa_mask;
    static const Type MANTISSA_MSB_MASK = mantissa_msb_mask;
    static const Type NAN_TYPE_MASK     = mantissa_msb_mask;

    static const int EXPONENT_BIAS  = exponent_bias;
    static const int MANTISSA_WIDTH = mantissa_width;

private:
    Type bits;

public:
    explicit IEEE754(Type val) : bits(val) {}
    IEEE754(bool isPositive, uint64_t mantissa, int64_t decodedExponent)
    {
        Type exponent = (Type)(decodedExponent + EXPONENT_BIAS) << MANTISSA_WIDTH;

        assert((exponent & ~EXPONENT_MASK) == 0);
        assert((mantissa & ~MANTISSA_MASK) == 0);
        assert((Type)(-1) > 0 && "Type must be unsigned integer");
        assert(EXPONENT_BIAS >= 0 && MANTISSA_WIDTH > 0 && MANTISSA_WIDTH <= sizeof(Type)*8); // rudementary checks

        bits = (isPositive? 0 : SIGN_MASK) | 
               (exponent & EXPONENT_MASK)  | 
               (mantissa & MANTISSA_MASK);
    }

public:
    Type getBits() const { return bits; }

public:
    Type getSign()      const { return bits & SIGN_MASK; }
    Type getMantissa()  const { return bits & MANTISSA_MASK; }
    Type getExponent()  const { return bits & EXPONENT_MASK; }
    Type getNanType()   const { return bits & NAN_TYPE_MASK; }

public:
    bool isPositive()          const { return getSign() == 0; }
    bool isNegative()          const { return getSign() != 0; }

    bool isZero()              const { return getExponent() == 0 && getMantissa() == 0; }
    bool isPositiveZero()      const { return isZero() && isPositive();  }
    bool isNegativeZero()      const { return isZero() && !isPositive(); }

    bool isInf()               const { return getExponent() == EXPONENT_MASK && getMantissa() == 0; }
    bool isPositiveInf()       const { return isInf() && isPositive();  }
    bool isNegativeInf()       const { return isInf() && !isPositive(); }

    bool isNan()               const { return getExponent() == EXPONENT_MASK && getMantissa() != 0; }
    bool isQuietNan()          const { return isNan() && getNanType() != 0; }
    bool isSignalingNan()      const { return isNan() && getNanType() == 0; }

    bool isSubnormal()         const { return getExponent() == 0 && getMantissa() != 0; }
    bool isPositiveSubnormal() const { return isSubnormal() && isPositive(); }
    bool isNegativeSubnormal() const { return isSubnormal() && !isPositive(); }

    bool isRegular()           const { return !isZero() && !isNan() && !isInf(); }
    bool isRegularPositive()   const { return isPositive() && isRegular(); }
    bool isRegularNegative()   const { return isNegative() && isRegular(); }

                                                // Natural = (fraction == 0)
                                                // getNormalizedFract() return 0 for small numbers so there is exponent check for this case
    bool isNatural()           const { return isZero() || (getNormalizedFract() == 0 && (getExponent() >> MANTISSA_WIDTH) >= EXPONENT_BIAS); }

public:
    static Type getQuietNan()     { return EXPONENT_MASK | NAN_TYPE_MASK; }
    static Type getNegativeZero() { return SIGN_MASK; }
    static Type getPositiveZero() { return 0; }
    static Type getNegativeInf()  { return SIGN_MASK | EXPONENT_MASK; }
    static Type getPositiveInf()  { return             EXPONENT_MASK; }

    static bool isValidExponent(int64_t exponenValue) // check _decoded_ exponent value
    {
        const int64_t minExp = -EXPONENT_BIAS;      // Reserved for subnormals
        const int64_t maxExp = EXPONENT_BIAS + 1;  // Reserved for Infs and NaNs
        return minExp < exponenValue && exponenValue < maxExp;
    }

    static int64_t decodedSubnormalExponent() { return 0 - EXPONENT_BIAS; } // Decoded exponent value which indicates a subnormal
    static int64_t actualSubnormalExponent()  { return 1 - EXPONENT_BIAS; } // Actual decoded exponent value for subnormals

public:
    template<typename TargetType> typename TargetType::Type mapSpecialValues() const
    {
        assert(!isRegular());

        if      (isPositiveZero()) { return TargetType::getPositiveZero();  }
        else if (isNegativeZero()) { return TargetType::getNegativeZero();  }
        else if (isPositiveInf())  { return TargetType::getPositiveInf();   }
        else if (isNegativeInf())  { return TargetType::getNegativeInf();   }
        else if (isQuietNan())     { return TargetType::getQuietNan();      }
        else if (isSignalingNan()) { assert(false); }

        assert(false);
        return 0;
    }

    template<typename TargetType> uint64_t mapNormalizedMantissa() const // map to target type
    {
        uint64_t mantissa = getMantissa();
        int targetMantissaWidth = TargetType::MANTISSA_WIDTH;

        assert(targetMantissaWidth != MANTISSA_WIDTH);

        if (targetMantissaWidth < MANTISSA_WIDTH) return mantissa >> (MANTISSA_WIDTH - targetMantissaWidth);
        else                                      return mantissa << (targetMantissaWidth - MANTISSA_WIDTH);
    }

    template<typename TargetType> uint64_t normalizeMantissa(int64_t& exponent) const
    {
        assert(sizeof(Type) != sizeof(typename TargetType::Type));

        if (sizeof(Type) < sizeof(typename TargetType::Type)) // Map subnormal to a larger type
        {
            assert(isSubnormal());

            // unused: unsigned targetTypeSize = (unsigned)sizeof(typename TargetType::Type) * 8;

            uint64_t mantissa = getMantissa();
            mantissa <<= sizeof(mantissa)*8 - MANTISSA_WIDTH;

            assert(mantissa != 0);

            bool found = false;
            for (; !found && TargetType::isValidExponent(exponent - 1); )
            {
                found = (mantissa & 0x8000000000000000ULL) != 0;
                mantissa <<= 1;
                exponent--;
            }
            if (!found) exponent = TargetType::decodedSubnormalExponent(); // subnormal -> subnormal

            return mantissa >> (sizeof(mantissa)*8 - TargetType::MANTISSA_WIDTH);
        }
        else // Map regular value with large negative mantissa to a smaller type (resulting in subnormal or 0)
        {
            assert(exponent < 0);
            assert(!TargetType::isValidExponent(exponent));
            assert(sizeof(T) > sizeof(typename TargetType::Type));

            uint64_t mantissa = getMantissa();

            // Add hidden bit of mantissa
            mantissa = MANTISSA_MSB_MASK | (mantissa >> 1);
            exponent++;

            for (; !TargetType::isValidExponent(exponent); )
            {
                mantissa >>= 1;
                exponent++;
            }

            exponent = TargetType::decodedSubnormalExponent();
            int delta = (MANTISSA_WIDTH - TargetType::MANTISSA_WIDTH);
            assert(delta >= 0);
            return mantissa >> delta;
        }
    }

    // Return exponent as a signed number
    int64_t decodeExponent() const
    {
        int64_t e = getExponent() >> MANTISSA_WIDTH;
        return e - EXPONENT_BIAS;
    }

    // Return fractional part of fp number
    // normalized so that x-th digit is at (63-x)-th bit of u64_t
    uint64_t getNormalizedFract(int x = 0) const
    {
        if (isZero() || isInf() || isNan()) return 0;

        uint64_t mantissa = getMantissa() | (MANTISSA_MASK + 1); // Highest bit of mantissa is not encoded but assumed
        int exponent = static_cast<int>(getExponent() >> MANTISSA_WIDTH);

        // Compute shift required to place 1st fract bit at 63d bit of u64_t
        int width = static_cast<int>(sizeof(mantissa) * 8);
        int shift = (exponent - EXPONENT_BIAS) + (width - MANTISSA_WIDTH) + x;
        if (shift <= -width || width <= shift) return 0;
        return (shift >= 0)? mantissa << shift : mantissa >> (-shift);
    }

    Type negate()      const { return (getSign()? 0 : SIGN_MASK) | getExponent() | getMantissa(); }
    Type copySign(Type v) const { return (v & SIGN_MASK) | getExponent() | getMantissa(); }

    Type normalize(bool discardNanSign) const // Clear NaN payload and sign
    {
        if (isQuietNan())
        {
            if (!discardNanSign) return getSign() | getExponent() | getNanType();
            else                 return             getExponent() | getNanType();
        }
        return bits;
    }
};

//=============================================================================
//=============================================================================
//=============================================================================
// Decoders for IEEE 754 numbers

typedef IEEE754
    <
        uint16_t,
        0x8000,  // Sign
        0x7C00,  // Exponent
        0x03ff,  // Mantissa
        0x0200,  // Mantissa MSB
        15,      // Exponent bias
        10       // Mantissa width
    > FloatProp16;

typedef IEEE754
    <
        uint32_t,
        0x80000000,  // Sign
        0x7f800000,  // Exponent
        0x007fffff,  // Mantissa
        0x00400000,  // Mantissa MSB
        127,         // Exponent bias
        23           // Mantissa width
    > FloatProp32;

typedef IEEE754
    <
        uint64_t,
        0x8000000000000000ULL,  // Sign
        0x7ff0000000000000ULL,  // Exponent
        0x000fffffffffffffULL,  // Mantissa
        0x0008000000000000ULL,  // Mantissa MSB
        1023,                   // Exponent bias
        52                      // Mantissa width
    > FloatProp64;

//==============================================================================
//==============================================================================
//==============================================================================
// F16 type

class f16_t
{
public:
    typedef FloatProp16::Type bits_t;
private:
    static const unsigned RND_NEAR = Brig::BRIG_ROUND_FLOAT_NEAR_EVEN;
    static const unsigned RND_ZERO = Brig::BRIG_ROUND_FLOAT_ZERO;
    static const unsigned RND_UP   = Brig::BRIG_ROUND_FLOAT_PLUS_INFINITY;
    static const unsigned RND_DOWN = Brig::BRIG_ROUND_FLOAT_MINUS_INFINITY;

private:
    bits_t bits;

public:
    f16_t() {}
    explicit f16_t(double x, unsigned rounding = RND_NEAR);
    explicit f16_t(float x, unsigned rounding = RND_NEAR);
    //explicit f16_t(f64_t x, unsigned rounding = RND_NEAR) : f16_t(double(x), rounding) {};
    //explicit f16_t(f32_t x, unsigned rounding = RND_NEAR) : f16_t(float(x), rounding) {};
    explicit f16_t(int32_t x) { bits = f16_t((f64_t)x).getBits(); }

public:
    bool operator>  (const f16_t& x) const { return f64() >  x.f64(); } /// \todo reimplement all via f32 at least
    bool operator<  (const f16_t& x) const { return f64() <  x.f64(); }
    bool operator>= (const f16_t& x) const { return f64() >= x.f64(); }
    bool operator<= (const f16_t& x) const { return f64() <= x.f64(); }
    bool operator== (const f16_t& x) const { return f64() == x.f64(); }

public:
    f16_t& operator+= (f16_t x) { bits = f16_t(f64() + x.f64()).bits; return *this; }

public:
    f32_t f32() const;
    f64_t f64() const;
    operator float() const { return f32(); }
    operator double() const { return f64(); }

public:
    f16_t neg() const { return make(FloatProp16(bits).negate()); }

public:
    static f16_t make(bits_t bits) { f16_t res; res.bits = bits; return res; }
    bits_t getBits() const { return bits; }

public:
/* FIXME
    static void sanityTests();
    string dump() { return FloatProp16(bits).dump(); }*/
};

/* FIXME
inline f16_t operator+ (f16_t l, f16_t r) { return f16_t(l.f64() + r.f64()); }
inline f16_t operator- (f16_t l, f16_t r) { return f16_t(l.f64() - r.f64()); } */

} // namespace

namespace hexl {

typedef HSAIL_X::f16_t half;

typedef union {
  uint8_t u8;
  int8_t s8;
  uint16_t u16;
  int16_t s16;
  uint32_t u32;
  int32_t s32;
  uint64_t u64;
  int64_t s64;
  half::bits_t h_bits;
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
inline ValueData H(half h) { ValueData data; data.h_bits = h.getBits();  return data; }
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
  /// Unsafe constructor, take care. Example: Value(MV_DOUBLE, F(expr)) yields totally invalid result.
  Value(ValueType type_, ValueData data_) : type(type_), data(data_), printExtraHex(false) {}
  Value() : type(MV_UINT64), printExtraHex(false) { data.u64 = 0; }
  Value(const Value& v) : type(v.type), data(v.data), printExtraHex(v.printExtraHex) { }
  Value& operator=(const Value& v) { type = v.type; data = v.data; printExtraHex = v.printExtraHex; return *this; }
  /// \todo HIGHLY unsafe, take care! Example: Value(MV_FLOAT, 1.5F) will cast 1.5F to uint64 1 (0x1), which means float denorm.
  Value(ValueType _type, uint64_t _value) : type(_type), printExtraHex(false) { data.u64 = _value; }
  explicit Value(float value_) : type(MV_FLOAT), printExtraHex(false)  { data.f = value_; }
  explicit Value(double value_) : type(MV_DOUBLE), printExtraHex(false) { data.d = value_; }
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
  half H() const { return half::make(data.h_bits); }
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
  void SetPrintExtraHex(bool m) { printExtraHex = m; }

  void WriteTo(void *dest) const;
  void ReadFrom(const void *src, ValueType type);
  void Serialize(std::ostream& out) const;
  void Deserialize(std::istream& in);

private:
  ValueType type;
  ValueData data;
  bool printExtraHex;
  friend class Comparison;
  friend std::ostream& operator<<(std::ostream& out, const Value& value);
  friend std::ostream& operator<<(std::ostream& out, const Comparison& comparison);

  static const unsigned PointerSize() { return sizeof(void *); }
};

typedef std::vector<Value> Values;

class ResourceManager;
class TestFactory;
class RuntimeContext;
class RuntimeContextState;
class Options;
class AllStats;

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
  ResourceManager* RM() { return Get<ResourceManager*>("hexl.rm"); }
  TestFactory* Factory() { return Get<TestFactory*>("hexl.testFactory"); }
  RuntimeContextState* State() { return Get<RuntimeContextState*>("hexl.runtimestate"); }
  RuntimeContext* Runtime() { return Get<RuntimeContext*>("hexl.runtime"); }
  const Options* Opts() const { return GetConst<Options>("hexl.options"); }

  // Logging helpers.
  std::ostream& Debug() { return *Get<std::ostream*>("hexl.log.stream.debug"); }
  std::ostream& Info() { return *Get<std::ostream*>("hexl.log.stream.info"); }
  std::ostream& Error() { return *Get<std::ostream*>("hexl.log.stream.error"); }
  void Error(const char *format, ...);
  void vError(const char *format, va_list ap);
  AllStats& Stats() { return *Get<AllStats*>("hexl.stats"); }

  // Dumping helpers.
  bool IsVerbose(const std::string& what) const;
  bool IsDumpEnabled(const std::string& what, bool enableWithPlainDumpOption = true) const;
  void SetOutputPath(const std::string& path) { Put("hexl.outputPath", path); }
  std::string GetOutputName(const std::string& name, const std::string& what);
  bool DumpTextIfEnabled(const std::string& name, const std::string& what, const std::string& text);
  bool DumpBinaryIfEnabled(const std::string& name, const std::string& what, const void *buffer, size_t bufferSize);
  void DumpBrigIfEnabled(const std::string& name, void* brig);
  void DumpDispatchsetupIfEnabled(const std::string& name, const void* dsetup);
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
  virtual void Print(std::ostream& out) const;
  virtual void PrintWithBuffer(std::ostream& out) const;
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
  void PrintLong(std::ostream& out);

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
  virtual void Print(std::ostream& out) const;
  virtual void PrintWithBuffer(std::ostream& out) const;
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
  MImage(unsigned id, const std::string& name, unsigned segment_, unsigned geometry_, unsigned chanel_order_, unsigned channel_type_, unsigned access_,
    size_t width_, size_t height_, size_t depth_, size_t rowPitch_, size_t slicePitch_)
    : MObject(id, MIMAGE, name), segment(segment_), geometry(geometry_), channelOrder(chanel_order_), channelType(channel_type_), accessPermission(access_),
    width(width_), height(height_), depth(depth_), rowPitch(rowPitch_), slicePitch(slicePitch_), vtype(MV_UINT32)
      { }
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
  Value GetRaw(size_t i) {return contentData[i]; }
  size_t GetDim(size_t pos, unsigned d) const;

  //For Handle 
  Value& Data() { return data; }
  const Value& Data() const { return data; }
  //for image content
  ValueType& VType() { return vtype; }
  const ValueType& VType() const { return vtype; }
  Values& ContentData() { return contentData; }
  const Values& ContentData() const { return contentData; }

  ValueType GetValueType() const { return ImageValueType(geometry); }
  void Print(std::ostream& out) const;
  std::string GetPosStr(size_t pos) const;
  void PrintComparisonInfo(std::ostream& out, size_t pos, Comparison& comparison) const;
  void PrintComparisonSummary(std::ostream& out, Comparison& comparison) const;
  virtual void SerializeData(std::ostream& out) const;

private:
  unsigned segment;
  unsigned geometry;
  unsigned channelOrder;
  unsigned channelType;
  unsigned accessPermission;
  size_t width, height, depth, rowPitch, slicePitch;
  Value data;
  Values contentData;
  ValueType vtype;
  void DeserializeData(std::istream& in);
};

class MRImage : public MObject {
public:
  MRImage(unsigned id, const std::string& name, unsigned geometry_, unsigned refid_, ValueType vtype_, const Values& data_ = Values(), const Comparison& comparison_ = Comparison()) :
    MObject(id, MRIMAGE, name), geometry(geometry_), refid(refid_), vtype(vtype_), data(data_), comparison(comparison_) { }
  MRImage(unsigned id, const std::string& name, std::istream& in) : MObject(id, MRIMAGE, name) { DeserializeData(in); }

  ValueType VType() const { return vtype; }
  unsigned Geometry() const { return geometry; }
  unsigned RefId() const { return refid; }
  Values& Data() { return data; }
  const Values& Data() const { return data; }
  Comparison& GetComparison() { return comparison; }
  virtual void SerializeData(std::ostream& out) const;

private:
  unsigned geometry;
  unsigned refid;
  ValueType vtype;
  Values data;
  Comparison comparison;
  void DeserializeData(std::istream& in);
};

inline MImage* NewMValue(unsigned id, const std::string& name, unsigned segment, unsigned geometry, unsigned chanel_order, unsigned channel_type, unsigned access, 
                         size_t width, size_t height, size_t depth, size_t rowPitch, size_t slicePitch) {
  MImage* mi = new MImage(id, name, segment, geometry, chanel_order, channel_type, access, 
                         width, height, depth, rowPitch, slicePitch);
  return mi;
}

inline MRImage* NewMRValue(unsigned id, MImage* mi, Value data, const Comparison& comparison = Comparison()) {
  MRImage* mr = new MRImage(id, mi->Name() + " (check)", mi->Geometry(), mi->Id(), mi->VType(), Values(), comparison);
  mr->Data().push_back(data);
  return mr;
}

inline MRImage* NewMRValue(unsigned id, MImage* mi, ValueData data, const Comparison& comparison = Comparison()) {
  return NewMRValue(id, mi, Value(mi->VType(), data), comparison);
}

class MSampler : public MObject {
public:
  MSampler(unsigned id, const std::string& name, unsigned segment_, unsigned coords_, unsigned filter_, unsigned addressing_)
    : MObject(id, MSAMPLER, name), segment(segment_),
      coords(coords_), filter(filter_), addressing(addressing_) { }
  MSampler(unsigned id, const std::string& name, std::istream& in) : MObject(id, MSAMPLER, name) { DeserializeData(in); }

  unsigned Coords() const { return coords; }
  unsigned Filter() const { return filter; }
  unsigned Addressing() const { return addressing; }
  size_t Size() const { return sizeof(void*); }
  Value GetRaw(size_t i) { assert(false); }

  ValueType VType() const { return MV_SAMPLER; }

  Value& Data() { return data; }
  const Value& Data() const { return data; }
  void Print(std::ostream& out) const;
  void PrintComparisonInfo(std::ostream& out, size_t pos, Comparison& comparison) const;
  void PrintComparisonSummary(std::ostream& out, Comparison& comparison) const;
  virtual void SerializeData(std::ostream& out) const;

private:
  unsigned segment;
  unsigned coords;
  unsigned filter;
  unsigned addressing;
  Value data;
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

inline MSampler* NewMValue(unsigned id, const std::string& name, unsigned segment, unsigned coords_, unsigned filter_, unsigned addressing_) {
  MSampler* ms = new MSampler(id, name, segment, coords_, filter_, addressing_);
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
  void PrintWithBuffers(std::ostream& out) const;
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
  void PrintWithBuffers(std::ostream& out) const;
      
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

  void PrintImpl(std::ostream& out, bool withBuffers) const;
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
