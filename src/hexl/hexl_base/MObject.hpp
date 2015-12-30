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

#ifndef MOBJECT_HPP
#define MOBJECT_HPP

#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

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
#include <HSAILb128_t.h>

/// For now, plain f16 i/o values are passed to kernels in elements of u32 arrays, in lower 16 bits.
/// f16x2 values occupy the whole 32 bits. To differientiate, use:
/// - MV_PLAIN_FLOAT16: for plain f16 vars, size = 4 bytes
/// - MV_FLOAT16: for elements of f16xN vars, size = 2 bytes
#define MBUFFER_PASS_PLAIN_F16_AS_U32

namespace hexl {

enum ValueType { MV_INT8, MV_UINT8, MV_INT16, MV_UINT16, MV_INT32, MV_UINT32, MV_INT64, MV_UINT64, MV_FLOAT16, MV_FLOAT, MV_DOUBLE, MV_REF, MV_POINTER, MV_IMAGE, MV_SAMPLER, MV_IMAGEREF, MV_SAMPLERREF, MV_EXPR, MV_STRING, 
#ifdef MBUFFER_PASS_PLAIN_F16_AS_U32
                 MV_PLAIN_FLOAT16,
#endif
                 MV_INT8X4, MV_INT8X8, MV_UINT8X4, MV_UINT8X8, MV_INT16X2, MV_INT16X4, MV_UINT16X2, MV_UINT16X4, MV_INT32X2, MV_UINT32X2, MV_UINT128, MV_FLOAT16X2, MV_FLOAT16X4, MV_FLOATX2, MV_LAST};

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

enum MObjectImageQuery {
    IMG_QUERY_WIDTH = 0,
    IMG_QUERY_HEIGHT,
    IMG_QUERY_DEPTH,
    IMG_QUERY_ARRAY,
    IMG_QUERY_CHANNELORDER,
    IMG_QUERY_CHANNELTYPE
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
const char *ImageQueryString(MObjectImageQuery mem);
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
#include <cmath>

namespace HSAIL_X { // eXperimental

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

/// ASSUMPTIONS:
/// Order of fields (msb-to-lsb) is fixed to: sign, exponent, mantissa.
/// Fields always occupy all bits of the underlying Type (which carries binary representation).
/// Sign is always at MSB in Type, exponent occupies all bits in the middle.
/// NAN is quiet when MSB of mantissa is set.
template<typename T, int mantissa_width>
class Ieee754Props
{
public:
    typedef T Type;

    static const Type SIGN_MASK  = (Type)((Type)1 << ((int)sizeof(Type)*8 - 1));
    static const int  EXP_WIDTH  = (int)sizeof(Type)*8 - 1 - mantissa_width;
    static const int  EXP_BIAS   = ~((-1) << EXP_WIDTH) / 2;
    static const Type EXP_MASK   = (Type)(~(Type(-1) << EXP_WIDTH) << mantissa_width);
    static const int  DECODED_EXP_SUBNORMAL_OR_ZERO = 0-EXP_BIAS; // by definition, sum with EXP_BIAS should yield 0
    static const int  DECODED_EXP_NORM_MIN  = 1-EXP_BIAS; // if less, then subnormal
    static const int  DECODED_EXP_NORM_MAX  = EXP_BIAS;   // if greater, then INF or NAN
    static const int  MANT_WIDTH = mantissa_width;
    static const Type MANT_MASK  = (Type)~((Type)(-1) << mantissa_width);
    static const Type MANT_HIDDEN_MSB_MASK = (Type)((Type)(1) << mantissa_width); // '1' at MSB of mantissa implied in normalized numbers
    static const Type NAN_IS_QUIET_MASK = (Type)((Type)(1) << (mantissa_width - 1));
    static const Type NAN_PAYLOAD_MASK = (Type)~((Type)(-1) << (mantissa_width - 1));

private:
    static_assert(0 < mantissa_width && mantissa_width < sizeof(Type)*8 - 1 - 2, // leave sign + 2 bits for exp at least
        "Wrong mantissa width");
    static_assert(EXP_WIDTH <= (int)sizeof(int)*8 - 1,
        "Exponent field is too wide");
    static_assert( (1 << (sizeof(int)*8 - EXP_WIDTH)) > mantissa_width,
        "'int' type is too small for exponent calculations");
    static_assert(SIGN_MASK > 0, // simple "Type(-1) > 0" does not work. MSVC issue?
        "Underlying Type (for bits) must be unsigned.");

    Type bits;

    template<typename TT> Ieee754Props(TT); // = delete;
public:
    explicit Ieee754Props(Type val) : bits(val) {}

public:
    static Ieee754Props getNegativeZero() { return Ieee754Props(SIGN_MASK); }
    static Ieee754Props getPositiveZero() { return Ieee754Props(Type(0)); }
    static Ieee754Props getNegativeInf()  { return Ieee754Props(Type(EXP_MASK | SIGN_MASK)); }
    static Ieee754Props getPositiveInf()  { return Ieee754Props(EXP_MASK); }

    static Ieee754Props makeQuietNan(bool negative, Type payload) {
        assert((payload & ~NAN_PAYLOAD_MASK) == 0);
        payload &= NAN_PAYLOAD_MASK;
        return Ieee754Props(Type((negative ? SIGN_MASK : 0) | EXP_MASK | NAN_IS_QUIET_MASK | payload));
    }
    static Ieee754Props makeSignalingNan(bool negative, Type payload) {
        assert((payload & ~NAN_PAYLOAD_MASK) == 0);
        payload &= NAN_PAYLOAD_MASK;
        if (payload == 0) { payload++; } // sNAN with zero payload is invalid (have same bits as INF).
        return Ieee754Props(Type((negative ? SIGN_MASK : 0) | EXP_MASK | payload));
    }
    static Ieee754Props assemble(bool negative, int64_t exponent, uint64_t mantissa) {
        const Type exp_bits = (Type)(exponent + EXP_BIAS) << MANT_WIDTH;
        assert((exp_bits & ~EXP_MASK) == 0);
        assert((mantissa & ~MANT_MASK) == 0);
        return Ieee754Props(Type((negative ? SIGN_MASK : 0) | (exp_bits & EXP_MASK) | (mantissa & MANT_MASK)));
    }

public:
    Type rawBits() const { return bits; }
    Type getSignBit() const { return bits & SIGN_MASK; }
    Type getNanIsQuietBit() const { return bits & NAN_IS_QUIET_MASK; }
    Type getExponentBits() const { return bits & EXP_MASK; }
    Type getMantissa()  const { return bits & MANT_MASK; }
    Type getNanPayload() const { assert(isNan()); return bits & MANT_MASK & ~NAN_IS_QUIET_MASK; }

    int64_t getExponent() const
    {
        if (isSubnormal()) {
            return DECODED_EXP_NORM_MIN; // hidden bit is zero, real exp value is fixed to this.
        }
        return (int64_t)(getExponentBits() >> MANT_WIDTH) - EXP_BIAS;
    }

    bool isPositive()          const { return getSignBit() == 0; }
    bool isNegative()          const { return getSignBit() != 0; }
    bool isZero()              const { return getExponentBits() == 0 && getMantissa() == 0; }
    bool isPositiveZero()      const { return isZero() && isPositive();  }
    bool isNegativeZero()      const { return isZero() && !isPositive(); }
    bool isInf()               const { return getExponentBits() == EXP_MASK && getMantissa() == 0; }
    bool isPositiveInf()       const { return isInf() && isPositive();  }
    bool isNegativeInf()       const { return isInf() && !isPositive(); }
    bool isNan()               const { return getExponentBits() == EXP_MASK && getMantissa() != 0; }
    bool isQuietNan()          const { return isNan() && getNanIsQuietBit() != 0; }
    bool isSignalingNan()      const { return isNan() && getNanIsQuietBit() == 0; }
    bool isSubnormal()         const { return getExponentBits() == 0 && getMantissa() != 0; }
    bool isPositiveSubnormal() const { return isSubnormal() && isPositive(); }
    bool isNegativeSubnormal() const { return isSubnormal() && !isPositive(); }
    bool isRegular()           const { return !isZero() && !isNan() && !isInf(); }
    bool isRegularPositive()   const { return isPositive() && isRegular(); }
    bool isRegularNegative()   const { return isNegative() && isRegular(); }
    bool isIntegral()          const { return isZero() || (getFractionalOfNormalized() == 0 /*may return 0 for small values, so =>*/ && getExponent() >= 0); }

    Ieee754Props neg() const { return Ieee754Props(Type(bits ^ SIGN_MASK)); }
    Ieee754Props abs() const { return Ieee754Props(Type(bits & ~SIGN_MASK)); }
    Ieee754Props copySign(Ieee754Props v) const { return Ieee754Props(Type((v.rawBits() & SIGN_MASK) | getExponentBits() | getMantissa())); }
    Ieee754Props quietedSignalingNan() const { assert(isSignalingNan()); return Ieee754Props(Type(bits | NAN_IS_QUIET_MASK)); }

    Ieee754Props clearPayloadIfNan(bool discardNanSign) const // Clear NaN payload and sign
    {
        if (isNan()) {
          Type newBits = (discardNanSign ? 0 : getSignBit()) | getExponentBits() | getNanIsQuietBit()
            | (isSignalingNan() ? 1 : 0); // sNAN with zero payload are invalid (have same bits as INF).
          return Ieee754Props(newBits);
        }
        return *this;
    }

    template<typename T2, int mantissa_width2>
    static Ieee754Props mapSpecialValues(const Ieee754Props<T2,mantissa_width2>& val)
    {
        assert(!val.isRegular());
        if      (val.isPositiveZero()) { return getPositiveZero(); }
        else if (val.isNegativeZero()) { return getNegativeZero(); }
        else if (val.isPositiveInf())  { return getPositiveInf(); }
        else if (val.isNegativeInf())  { return getNegativeInf(); }
        else if (val.isQuietNan())     { return makeQuietNan(val.isNegative(), val.getMantissa() & NAN_PAYLOAD_MASK); } // just throw extra bits of payload away
        else if (val.isSignalingNan()) { return makeSignalingNan(val.isNegative(), val.getMantissa() & NAN_PAYLOAD_MASK); }
        assert(false);
        return getPositiveZero();
    }

    /// Return fractional part of fp number (normalized to +/-iii.fffE+0) so that
    /// the Nth bit of fractional, counting from the left, is at u64_t:(63-N-x) bit.
    /// For example, when x == 0, MSB of fractional it getting into MSB of output value.
    uint64_t getFractionalOfNormalized(int x = 0) const
    {
        if (isInf() || isNan()) {
          assert(!"Input number must represent a numeric value here");
          return 0;
        }

        uint64_t mantissa = getMantissa();
        if (!isSubnormal() && !isZero()) mantissa |= MANT_HIDDEN_MSB_MASK;

        // Compute shift required to place 1st fract bit at MSB bit of u64_t
        int width = static_cast<int>(sizeof(mantissa) * 8);
        int shift = static_cast<int>(getExponent()) + (width - MANT_WIDTH) + x;
        if (shift <= -width || width <= shift) return 0;
        return (shift >= 0)? mantissa << shift : mantissa >> (-shift);
    }

    /// Add delta ULPs.
    /// NB: This is not a generic code; it does not handle INF
    Ieee754Props ulp(int64_t delta) const
    {
        assert(delta == -1 || delta == 1);
        // Special values:
        if (isInf() || isNan()) return getPositiveZero();
        if (isZero() && delta == -1) return assemble(true , DECODED_EXP_SUBNORMAL_OR_ZERO, 0x01); // +/-0 -> -MIN_DENORM
        if (isZero() && delta ==  1) return assemble(false, DECODED_EXP_SUBNORMAL_OR_ZERO, 0x01); // +/-0 -> +MIN_DENORM
        if (isNegativeSubnormal() && getMantissa() == 0x01 && delta == 1) return getPositiveZero(); // -MIN_DENORM -> +0
        // The rest of values:
        return isNegative() ? Ieee754Props(Type(bits - delta)) : Ieee754Props(Type(bits + delta));
    }
};


//=============================================================================
//=============================================================================
//=============================================================================
// Floating-point types and rounding modes

/// \note "enum struct Round {}" is more type-safe, but requires a lot of reworking.
static const unsigned RND_NEAR = BRIG_ROUND_FLOAT_NEAR_EVEN;
static const unsigned RND_ZERO = BRIG_ROUND_FLOAT_ZERO;
static const unsigned RND_PINF = BRIG_ROUND_FLOAT_PLUS_INFINITY;
static const unsigned RND_MINF = BRIG_ROUND_FLOAT_MINUS_INFINITY;

typedef Ieee754Props<uint16_t, 10> FloatProp16;
typedef Ieee754Props<uint32_t, 23> FloatProp32;
typedef Ieee754Props<uint64_t, 52> FloatProp64;

class f16_t;
class f32_t;
class f64_t;

class f32_t
{
public:
    typedef FloatProp32 props_t;
    typedef props_t::Type bits_t;
    typedef float floating_t;

private:
    union { floating_t m_value; bits_t m_bits; };

public:
    f32_t() : m_bits(0) {}
    f32_t(floating_t x) : m_value(x) {} // non-explicit for initialization of arrays etc.
    explicit f32_t(double x, unsigned rounding = RND_NEAR);
    explicit f32_t(f64_t x, unsigned rounding = RND_NEAR);
    explicit f32_t(f16_t x);
    explicit f32_t(int32_t x, unsigned rounding = RND_NEAR);
    explicit f32_t(uint32_t x, unsigned rounding = RND_NEAR);
    explicit f32_t(int64_t x, unsigned rounding = RND_NEAR);
    explicit f32_t(uint64_t x, unsigned rounding = RND_NEAR);
    //
    explicit f32_t(props_t x) : m_bits(x.rawBits()) {}
    explicit f32_t(const floating_t* rhs) : m_bits(*reinterpret_cast<const bits_t*>(rhs)) {} // allows SNANs

public:
    // These operators work also for floating_T due to non-explicit ctor
    bool operator>  (f32_t x) const { return m_value >  x.m_value; }
    bool operator<  (f32_t x) const { return m_value <  x.m_value; }
    bool operator>= (f32_t x) const { return m_value >= x.m_value; }
    bool operator<= (f32_t x) const { return m_value <= x.m_value; }
    bool operator== (f32_t x) const { return m_value == x.m_value; }
    bool operator!= (f32_t x) const { return m_value != x.m_value; }
    // 32- and 64-bit integers can't be fit into f32's mantissa without rounding.
    // Therefore, precise implementation is non-trivial for the most of cases.
    // Only two kinds of precise compare can be easily implemented,
    // and only for a limited range of floating values:
    // * <= for positives
    // * >= for negatives
    // Other kinds of compare with 32/64-bit integers are disabled.
    // NOTE: Simple template implementation actually covers compare with ALL types
    // except floating types. Compare with all floating types is implemented above.
    template<typename T> bool operator<= (T x) const { assert((m_value >= 0.0)); return m_value <= x; }
    template<typename T> bool operator>= (T x) const { assert((m_value <= 0.0)); return m_value >= x; }
private:
    template<typename T> bool operator== (T x) const;
    template<typename T> bool operator!= (T x) const;
    template<typename T> bool operator>  (T x) const;
    template<typename T> bool operator<  (T x) const;

public:
    bool isSubnormal() const { return props().isSubnormal(); }
    bool isZero() const { return props().isZero(); }
    bool isNan() const { return props().isNan(); }
    bool isInf() const { return props().isInf(); }
    bool isNegative() const { return props().isNegative(); }
    f32_t copySign(f32_t x) const { return f32_t(props().copySign(x.props())); }
    f32_t neg() const { return f32_t(props().neg()); }
    f32_t abs() const { return f32_t(props().abs()); }
    f32_t inline modf(f32_t* iptr ) {
      assert(iptr);
      const floating_t a = std::modf(m_value, iptr->getFloatValuePtr());
      return f32_t(&a);
    }
    f32_t& operator+= (f32_t x) { m_value = m_value + x.m_value; return *this; }
    f32_t operator- () const { return neg(); }

public:
    static f32_t fromRawBits(bits_t bits_) { f32_t res; res.m_bits = bits_; return res; }
    bits_t rawBits() const { return m_bits; }
    props_t props() const { return props_t(m_bits); }
    floating_t floatValue() const { return m_value; }
    floating_t* getFloatValuePtr() { return &m_value; }
};

inline f32_t operator+ (f32_t l, f32_t  r) { const float rv = l.floatValue() + r.floatValue(); return f32_t(&rv); }
inline f32_t operator- (f32_t l, f32_t  r) { const float rv = l.floatValue() - r.floatValue(); return f32_t(&rv); }
inline f32_t operator* (f32_t l, float  r) { const float rv = l.floatValue() * r; return f32_t(&rv); }
inline f32_t operator/ (f32_t l, float  r) { const float rv = l.floatValue() / r; return f32_t(&rv); }
inline f32_t operator* (f32_t l, double r) { const float rv = static_cast<float>(l.floatValue() * r); return f32_t(&rv); }
inline f32_t operator/ (f32_t l, double r) { const float rv = static_cast<float>(l.floatValue() / r); return f32_t(&rv); }

inline f32_t operator* (f32_t l, f32_t    r) { return l * r.floatValue(); }
inline f32_t operator* (f32_t l, int      r) { return l * (double)r; }
inline f32_t operator* (f32_t l, unsigned r) { return l * (double)r; }
inline f32_t operator/ (f32_t l, f32_t    r) { return l / r.floatValue(); }
inline f32_t operator/ (f32_t l, int      r) { return l / (double)r; }

class f64_t
{
public:
    typedef FloatProp64 props_t;
    typedef props_t::Type bits_t;
    typedef double floating_t;

private:
    union { floating_t m_value; bits_t m_bits; };

public:
    f64_t() : m_bits(0) {}
    f64_t(floating_t x) : m_value(x) {} // non-explicit for initialization of arrays etc.
    explicit f64_t(float x);
    explicit f64_t(f32_t x);
    explicit f64_t(f16_t x);
    explicit f64_t(int32_t x, unsigned rounding = RND_NEAR);
    explicit f64_t(uint32_t x, unsigned rounding = RND_NEAR);
    explicit f64_t(int64_t x, unsigned rounding = RND_NEAR);
    explicit f64_t(uint64_t x, unsigned rounding = RND_NEAR);
    //
    explicit f64_t(props_t x) : m_bits(x.rawBits()) {}
    explicit f64_t(const floating_t* rhs) : m_bits(*reinterpret_cast<const bits_t*>(rhs)) {} // allows SNANs

public:
    bool operator>  (f64_t x) const { return m_value >  x.m_value; }
    bool operator<  (f64_t x) const { return m_value <  x.m_value; }
    bool operator>= (f64_t x) const { return m_value >= x.m_value; }
    bool operator<= (f64_t x) const { return m_value <= x.m_value; }
    bool operator== (f64_t x) const { return m_value == x.m_value; }
    bool operator!= (f64_t x) const { return m_value != x.m_value; }
    // 64-bit integers can't be fit into f64's mantissa without rounding.
    // Therefore, precise implementation is non-trivial for the most of cases.
    // Only two kinds of precise compare can be easily implemented,
    // and only for a limited range of floating values:
    // * <= for positives
    // * >= for negatives
    // Other kinds of compare with 64-bit integers are disabled.
    // NOTE: Simple template implementation actually covers compare with ALL types
    // except floating types. Compare with all floating types is implemented above.
    template<typename T> bool operator<= (T x) const { assert(m_value >= 0.0); return m_value <= x; }
    template<typename T> bool operator>= (T x) const { assert(m_value <= 0.0); return m_value >= x; }
private:
    template<typename T> bool operator== (T x) const;
    template<typename T> bool operator!= (T x) const;
    template<typename T> bool operator>  (T x) const;
    template<typename T> bool operator<  (T x) const;

public:
    bool isSubnormal() const { return props().isSubnormal(); }
    bool isZero() const { return props().isZero(); }
    bool isNan() const { return props().isNan(); }
    bool isInf() const { return props().isInf(); }
    bool isNegative() const { return props().isNegative(); }
    f64_t copySign(f64_t x) const { return f64_t(props().copySign(x.props())); }
    f64_t neg() const { return f64_t(props().neg()); }
    f64_t abs() const { return f64_t(props().abs()); }
    f64_t inline modf(f64_t* iptr ) {
      assert(iptr);
      const floating_t a = std::modf(m_value, iptr->getFloatValuePtr());
      return f64_t(&a);
    }
    f64_t& operator+= (f64_t x) { m_value = m_value + x.m_value; return *this; }
    f64_t operator- () const { return neg(); }

public:
    static f64_t fromRawBits(bits_t bits_) { f64_t res; res.m_bits = bits_; return res; }
    bits_t rawBits() const { return m_bits; }
    props_t props() const { return props_t(m_bits); }
    floating_t floatValue() const { return m_value; }
    floating_t* getFloatValuePtr() { return &m_value; }
};

inline f64_t operator+ (f64_t l, f64_t r) { const f64_t::floating_t rv = l.floatValue() + r.floatValue(); return f64_t(&rv); }
inline f64_t operator- (f64_t l, f64_t r) { const f64_t::floating_t rv = l.floatValue() - r.floatValue(); return f64_t(&rv); }
inline f64_t operator* (f64_t l, f64_t::floating_t r) { const f64_t::floating_t rv = l.floatValue() * r; return f64_t(&rv); }
inline f64_t operator/ (f64_t l, f64_t::floating_t r) { const f64_t::floating_t rv = l.floatValue() / r; return f64_t(&rv); }

inline f64_t operator* (f64_t l, f64_t    r) { return l * r.floatValue(); }
inline f64_t operator* (f64_t l, int      r) { return l * static_cast<f64_t::floating_t>(r); }
inline f64_t operator* (f64_t l, unsigned r) { return l * static_cast<f64_t::floating_t>(r); }
inline f64_t operator/ (f64_t l, f64_t    r) { return l / r.floatValue(); }
inline f64_t operator/ (f64_t l, int      r) { return l / static_cast<f64_t::floating_t>(r); }


class f16_t
{
public:
    typedef FloatProp16 props_t;
    typedef props_t::Type bits_t;
    typedef void floating_t;

private:
    bits_t m_bits;

public:
    f16_t() : m_bits(0) {}
    explicit f16_t(double x,unsigned rounding = RND_NEAR);
    explicit f16_t(float x, unsigned rounding = RND_NEAR);
    explicit f16_t(f64_t x, unsigned rounding = RND_NEAR);
    explicit f16_t(f32_t x, unsigned rounding = RND_NEAR);
    //
    explicit f16_t(props_t x) : m_bits(x.rawBits()) {}
    explicit f16_t(int32_t x, unsigned rounding = RND_NEAR);
    explicit f16_t(uint32_t x, unsigned rounding = RND_NEAR);
    explicit f16_t(int64_t x, unsigned rounding = RND_NEAR);
    explicit f16_t(uint64_t x, unsigned rounding = RND_NEAR);

public:
    bool operator>  (f16_t x) const { return floatValue() >  x.floatValue(); }
    bool operator<  (f16_t x) const { return floatValue() <  x.floatValue(); }
    bool operator>= (f16_t x) const { return floatValue() >= x.floatValue(); }
    bool operator<= (f16_t x) const { return floatValue() <= x.floatValue(); }
    bool operator== (f16_t x) const { return floatValue() == x.floatValue(); }
    // 16/32/64-bit integers can't be fit into f16's mantissa without rounding.
    // Therefore, precise implementation is non-trivial for the most of cases.
    // Only two kinds of precise compare can be easily implemented,
    // and only for a limited range of floating values:
    // * <= for positives
    // * >= for negatives
    // Other kinds of compare with 16/32/64-bit integers are disabled.
    // NOTE: Simple template implementation actually covers compare with ALL types
    // except floating types. Compare with all floating types is implemented above.
    template<typename T> bool operator<= (T x) const { assert(floatValue() >= 0.0); return floatValue() <= x; }
    template<typename T> bool operator>= (T x) const { assert(floatValue() <= 0.0); return floatValue() >= x; }
private:
    template<typename T> bool operator== (T x) const;
    template<typename T> bool operator!= (T x) const;
    template<typename T> bool operator>  (T x) const;
    template<typename T> bool operator<  (T x) const;

public:
    bool isSubnormal() const { return props().isSubnormal(); }
    bool isZero() const { return props().isZero(); }
    bool isNan() const { return props().isNan(); }
    bool isInf() const { return props().isInf(); }
    bool isNegative() const { return props().isNegative(); }
    f16_t copySign(f16_t x) const { return f16_t(props().copySign(x.props())); }
    f16_t neg() const { return f16_t(props().neg()); }
    f16_t abs() const { return f16_t(props().abs()); }
    f16_t& operator+= (f16_t x) { m_bits = f16_t(f64_t(*this) + f64_t(x)).m_bits; return *this; }
    f16_t operator- () const { return neg(); }

public:
    static f16_t fromRawBits(bits_t bits) { f16_t res; res.m_bits = bits; return res; }
    bits_t rawBits() const { return m_bits; }
    props_t props() const { return props_t(m_bits); }
    float floatValue() const { return f32_t(*this).floatValue(); }
};

inline f16_t operator+ (f16_t l, f16_t r) { return f16_t(f64_t(l).floatValue() + f64_t(r).floatValue()); }
inline f16_t operator- (f16_t l, f16_t r) { return f16_t(f64_t(l).floatValue() - f64_t(r).floatValue()); }
inline f16_t operator* (f16_t l, double   r) { return f16_t(l.floatValue() * r); }
inline f16_t operator* (f16_t l, f16_t    r) { return l * double(r.floatValue()); }
inline f16_t operator* (f16_t l, int      r) { return l * double(r); }
inline f16_t operator* (f16_t l, unsigned r) { return l * double(r); }
inline f16_t operator/ (f16_t l, double   r) { return f16_t(l.floatValue() / r); }
inline f16_t operator/ (f16_t l, f16_t    r) { return l / double(r.floatValue()); }

//==============================================================================
//==============================================================================
//==============================================================================
// U128 type
class uint128
{
public:
  typedef struct { uint64_t l; uint64_t h; } Type;
private:
    uint64_t val[2];

public:
    uint128() { val[0] = 0; val[1] = 0;}
    explicit uint128(int64_t l) { val[0] = l; val[1] = 0; }
    explicit uint128(int64_t h, int64_t l) { val[0] = l; val[1] = h; }

public:
    uint32_t U32() const { return val[0] & 0x00000000FFFFFFFF; }
    uint64_t U64L() const { return val[0]; }
    uint64_t U64H() const { return val[1]; }
    operator int() const { return U32(); }
    operator long long() const { return U64L(); }

    static uint128 make(Type bits) { uint128 res; res.val[1] = bits.h; res.val[0] = bits.l; return res; }

    bool operator >  (const uint128& x) const;
    bool operator <  (const uint128& x) const;
    bool operator >= (const uint128& x) const;
    bool operator <= (const uint128& x) const;
    bool operator == (const uint128& x) const;
};

} // namespace HSAIL_X

#include <limits>
namespace std {

using HSAIL_X::f16_t;

template<> class numeric_limits<HSAIL_X::f16_t> {
public:
    static f16_t infinity() { return f16_t(f16_t::props_t::getPositiveInf()); }
};

} // namespace std

namespace hexl {

typedef HSAIL_X::f16_t half;
using HSAIL_X::f32_t;
using HSAIL_X::f64_t;
typedef HSAIL_X::uint128 uint128_t;

typedef union {
  uint8_t u8;
  int8_t s8;
  uint16_t u16;
  int16_t s16;
  uint32_t u32;
  int32_t s32;
  uint64_t u64;
  int64_t s64;
  uint128_t::Type u128;
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
inline ValueData U128(uint128_t u128) { ValueData data; data.u128.h = u128.U64H(); data.u128.l = u128.U64L(); return data; }
inline ValueData H(half h) { ValueData data; data.h_bits = h.rawBits();  return data; }
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
ValueData U64X2(uint64_t a, uint64_t b);
ValueData FX2(float a, float b);

class Comparison;

class Value {
public:
  /// Unsafe constructor, take care. Example: Value(MV_DOUBLE, F(expr)) yields totally invalid result.
  Value(ValueType type_, ValueData data_) : type(type_), data(data_), printExtraHex(false) { }
  Value() : type(MV_UINT64), printExtraHex(false) { data.u64 = 0; data.u128.h = 0;}
  Value(const Value& v) : type(v.type), data(v.data), printExtraHex(v.printExtraHex) { }
  Value& operator=(const Value& v) { type = v.type; data = v.data; printExtraHex = v.printExtraHex; return *this; }
  /// \todo HIGHLY unsafe, take care! Example: Value(MV_FLOAT, 1.5F) will cast 1.5F to uint64 1 (0x1), which means float denorm.
  Value(ValueType _type, uint64_t _value) : type(_type), printExtraHex(false) { data.u64 = _value;  data.u128.h = 0;}
  Value(uint128_t _value) : type(MV_UINT128), printExtraHex(false) { data.u128.h = _value.U64H(); data.u128.l = _value.U64L();}
  Value(uint128_t::Type _value) : type(MV_UINT128), printExtraHex(false) { data.u128.h = _value.h; data.u128.l = _value.l;}
  explicit Value(float value_) : type(MV_FLOAT), printExtraHex(false)  { data.f = value_;  data.u128.h = 0;}
  explicit Value(double value_) : type(MV_DOUBLE), printExtraHex(false) { data.d = value_;  data.u128.h = 0;}
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
  uint128_t U128() const { return uint128_t::make(data.u128); }
  half H() const { return half::fromRawBits(data.h_bits); }
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
  uint64_t U64X2(size_t index) const { assert(index < 2); return reinterpret_cast<const uint64_t *>(&(data.u128))[index]; }
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
namespace runtime { class RuntimeContext; class RuntimeState; }
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

void WriteTo(void *dest, const Values& values);
void ReadFrom(void *dest, ValueType type, size_t count, Values& values);
void SerializeValues(std::ostream& out, const Values& values);
void DeserializeValues(std::istream& in, Values& values);
uint32_t SizeOf(const Values& values);

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
//  size_t Size(Context* context) const;
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
  CM_ULPS,     // precision = max ULPS
  CM_IMAGE,    // precision = max ulps with convet by PRM
  CM_RELATIVE  // (simulated_value - expected_value) / expected_value <= precision
};
/// \note CM_ULPS is an approximation of the ulp(x) function defined in PRM section 4.19.6.
/// Precision in CM_ULPS is more like a number of representable float numbers between
/// expected and actual results. Thus it gives slightly different results.

class Comparison {
public:
  static const int F64_DEFAULT_DECIMAL_PRECISION = 7; // Default F64 arithmetic precision (if not set in results) 
  static const int F32_DEFAULT_DECIMAL_PRECISION = 3;  // Default F32 arithmetic precision (if not set in results)
  static const int F16_DEFAULT_DECIMAL_PRECISION = 2;  // Default F16 arithmetic precision (if not set in results)
#if defined(LINUX)
  static const int F64_MAX_DECIMAL_PRECISION = 17; // Max F64 arithmetic precision
  static const int F32_MAX_DECIMAL_PRECISION = 9; // Max F32 arithmetic precision
  static const int F16_MAX_DECIMAL_PRECISION = 5; // Max F16 arithmetic precision
#else
  static const int F64_MAX_DECIMAL_PRECISION = std::numeric_limits<double>::max_digits10; // Max F64 arithmetic precision
  static const int F32_MAX_DECIMAL_PRECISION = std::numeric_limits<float>::max_digits10; // Max F32 arithmetic precision
  static const int F16_MAX_DECIMAL_PRECISION = std::numeric_limits<float>::max_digits10 - 4; // 23 bit vs 10 bit mantissa (f32 vs f16)
#endif
  static const double F_DEFAULT_ULPS_PRECISION;
  static const double F_DEFAULT_RELATIVE_PRECISION;
  Comparison(ComparisonMethod method_ = CM_DECIMAL, const Value& precision_ = Value(MV_UINT64, U64(-1)))
    : method(method_)
    , precision(precision_)
    , minLimit(-std::numeric_limits<float>::infinity())
    , maxLimit( std::numeric_limits<float>::infinity())
    , flushDenorms(false)
    , result(false)
  { }
  void SetMinLimit(float limit); //Adds additional check for floating expected and actual results to be >= minLimit, NaNs are ignored
  void SetMaxLimit(float limit); //Adds additional check for floating expected and actual results to be <= maxLimit, NaNs are ignored
  void SetFlushDenorms(bool flushDenorms); //If set, Compare() will also compute error_ftz (when denorms are flushed to 0) and return min(error, error_ftz)
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

  bool Compare(const Value& expected, const Value& actual);

private:
  ComparisonMethod method;
  Value precision;
  float minLimit, maxLimit;
  bool flushDenorms;
  bool result;
  Value error;
  Value expected, actual;

  unsigned checks, failed;
  Value maxError;
  size_t maxErrorIndex;

  bool CompareValues(const Value& v1, const Value& v2);
  bool CompareHalf(const Value& v1, const Value& v2);
  bool CompareFloat(const Value& v1, const Value& v2);
  bool CompareDouble(const Value& v1, const Value& v2);
  float ConvertToStandard(float f) const;
};

std::ostream& operator<<(std::ostream& out, const Comparison& comparison);

Comparison* NewComparison(const std::string& comparison, ValueType type);

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
    default: assert(false); mo = 0; break;
    }
  }
};

MEMBER_SERIALIZER(MemorySetup);
MEMBER_SERIALIZER(Value);

}

#endif // MOBJECT_HPP
