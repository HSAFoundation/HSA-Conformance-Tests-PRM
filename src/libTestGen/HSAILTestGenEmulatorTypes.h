/*
   Copyright 2013-2015 Heterogeneous System Architecture (HSA) Foundation

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

#ifndef INCLUDED_HSAIL_TESTGEN_EMULATOR_TYPES_H
#define INCLUDED_HSAIL_TESTGEN_EMULATOR_TYPES_H

#include "Brig.h"
#include "HSAILUtilities.h"
#include "HSAILTestGenUtilities.h"
#include "HSAILItems.h"

#include <assert.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdint.h>
#include <cmath> // for std::modf

using std::string;
using std::ostringstream;
using std::setbase;
using std::setw;
using std::setfill;

namespace TESTGEN {

// ============================================================================
// ============================================================================
// ============================================================================
// HSAIL s/u/f Types

typedef int8_t   s8_t;
typedef int16_t  s16_t;
typedef int32_t  s32_t;
typedef int64_t  s64_t;

typedef uint8_t  u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

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

/// \brief Precise emulations of HSAIL floating-point operations.
template <typename T> T mul(const T &a, const T & b, unsigned rounding);
template <typename T> T add(const T &a, const T & b, unsigned rounding);
template <typename T> T fma(const T &a, const T & b, const T & c, unsigned rounding);

} // namespace TESTGEN

#include <limits>
#include <cmath>
namespace std {

/*using TESTGEN::f16_t;
using TESTGEN::f32_t;
using TESTGEN::f64_t;*/

template<> class numeric_limits<TESTGEN::f16_t> {
public:
    static TESTGEN::f16_t quiet_NaN()  { return TESTGEN::f16_t(TESTGEN::f16_t::props_t::makeQuietNan(false, 0)); }
    static TESTGEN::f16_t infinity()   { return TESTGEN::f16_t(TESTGEN::f16_t::props_t::getPositiveInf()); }
    static TESTGEN::f16_t denorm_min() { return TESTGEN::f16_t(TESTGEN::f16_t::props_t::assemble(false, TESTGEN::f16_t::props_t::DECODED_EXP_SUBNORMAL_OR_ZERO, 0x01)); }
    static TESTGEN::f16_t min()        { return TESTGEN::f16_t(TESTGEN::f16_t::props_t::assemble(false, TESTGEN::f16_t::props_t::DECODED_EXP_NORM_MIN, 0)); }
    static TESTGEN::f16_t max()        { return TESTGEN::f16_t(TESTGEN::f16_t::props_t::assemble(false, TESTGEN::f16_t::props_t::DECODED_EXP_NORM_MAX, TESTGEN::f16_t::props_t::MANT_MASK)); }
};
template<> class numeric_limits<TESTGEN::f64_t> {
public:
    static TESTGEN::f64_t quiet_NaN()  { return TESTGEN::f64_t(TESTGEN::f64_t::props_t::makeQuietNan(false, 0)); }
    static TESTGEN::f64_t infinity()   { return TESTGEN::f64_t(TESTGEN::f64_t::props_t::getPositiveInf()); }
    static TESTGEN::f64_t denorm_min() { return TESTGEN::f64_t(TESTGEN::f64_t::props_t::assemble(false, TESTGEN::f64_t::props_t::DECODED_EXP_SUBNORMAL_OR_ZERO, 0x01)); }
    static TESTGEN::f64_t min()        { const TESTGEN::f64_t::floating_t x = numeric_limits<TESTGEN::f64_t::floating_t>::min(); return TESTGEN::f64_t(&x); }
    static TESTGEN::f64_t max()        { const TESTGEN::f64_t::floating_t x = numeric_limits<TESTGEN::f64_t::floating_t>::max(); return TESTGEN::f64_t(&x); }
};
template<> class numeric_limits<TESTGEN::f32_t> {
public:
    static TESTGEN::f32_t quiet_NaN()  { return TESTGEN::f32_t(TESTGEN::f32_t::props_t::makeQuietNan(false, 0)); }
    static TESTGEN::f32_t infinity()   { return TESTGEN::f32_t(TESTGEN::f32_t::props_t::getPositiveInf()); }
    static TESTGEN::f32_t denorm_min() { return TESTGEN::f32_t(TESTGEN::f32_t::props_t::assemble(false, TESTGEN::f32_t::props_t::DECODED_EXP_SUBNORMAL_OR_ZERO, 0x01)); }
    static TESTGEN::f32_t min()        { const TESTGEN::f32_t::floating_t x = numeric_limits<TESTGEN::f32_t::floating_t>::min(); return TESTGEN::f32_t(&x); }
    static TESTGEN::f32_t max()        { const TESTGEN::f32_t::floating_t x = numeric_limits<TESTGEN::f32_t::floating_t>::max(); return TESTGEN::f32_t(&x); }
};

} // namespace std

namespace TESTGEN {

// ============================================================================
// ============================================================================
// ============================================================================
// HSAIL Type Helpers

class b128_t
{
private:
    u64_t  data[2];

public:
    void clear() { set<u64_t>(0, 0); set<u64_t>(0, 1); }
    template<typename T> void init(T val) { clear(); set(val); }
    template<typename T> T get(unsigned idx = 0) const;
    template<typename T> void set(T val, unsigned idx = 0);

public:
    u64_t getElement(unsigned type, unsigned idx = 0) const; // Get value with sign-extension
    void setElement(u64_t val, unsigned type, unsigned idx = 0);

public:
    bool operator==(const b128_t& rhs) const { return (get<u64_t>(0) == rhs.get<u64_t>(0)) && (get<u64_t>(1) == rhs.get<u64_t>(1)); }
    string hexDump() const;
};

template <typename OS> inline OS& operator <<(OS& os, const b128_t& v) { os << v.hexDump(); return os; }

template<typename T> inline
T b128_t::get(unsigned idx) const
{
    assert(idx < 16 / sizeof(T));
    return reinterpret_cast<const T*>(this)[idx];
}

template<> inline
f16_t b128_t::get<f16_t>(unsigned idx) const
{
    assert(idx < 16 / sizeof(f16_t::bits_t));
    return f16_t::fromRawBits(reinterpret_cast<const f16_t::bits_t*>(this)[idx]);
}

template<> inline
f32_t b128_t::get<f32_t>(unsigned idx) const
{
    assert(idx < 16 / sizeof(f32_t::bits_t));
    return f32_t::fromRawBits(reinterpret_cast<const f32_t::bits_t*>(this)[idx]);
}

template<typename T> inline
void b128_t::set(T val, unsigned idx)
{
    assert(idx < 16 / sizeof(T));
    reinterpret_cast<T*>(this)[idx] = val;
}

template<> inline
void b128_t::set<f16_t>(f16_t val, unsigned idx)
{
    assert(idx < 16 / sizeof(f16_t::bits_t));
    reinterpret_cast<f16_t::bits_t*>(this)[idx] = val.rawBits();
}

template<> inline
void b128_t::set<f32_t>(f32_t val, unsigned idx)
{
    assert(idx < 16 / sizeof(f32_t::bits_t));
    reinterpret_cast<f32_t::bits_t*>(this)[idx] = val.rawBits();
}

// ============================================================================
// ============================================================================
// ============================================================================
// HSAIL Type Class

template<typename T, typename E, unsigned HsailTypeId>
struct HsailType
{
public:
    typedef T BaseType;
    typedef E ElemType;
    static const unsigned typeId = HsailTypeId;

private:
    BaseType val;

public:
    HsailType() {}
    HsailType(const BaseType v) : val(v) {}
    bool operator==(const HsailType& rhs) const { return val == rhs.val; }
    operator BaseType() const { return val; }
    BaseType get() const { return val; }
};

// ============================================================================
// ============================================================================
// ============================================================================
// HSAIL Bit Types

typedef HsailType<u8_t,  u8_t,  BRIG_TYPE_B1>  b1_t;
typedef HsailType<u8_t,  u8_t,  BRIG_TYPE_B8>  b8_t;
typedef HsailType<u16_t, u16_t, BRIG_TYPE_B16> b16_t;
typedef HsailType<u32_t, u32_t, BRIG_TYPE_B32> b32_t;
typedef HsailType<u64_t, u64_t, BRIG_TYPE_B64> b64_t;

//=============================================================================
//=============================================================================
//=============================================================================
// HSAIL Packed Types

typedef HsailType<u32_t,  u8_t,  BRIG_TYPE_U8X4 > u8x4_t;
typedef HsailType<u32_t,  u16_t, BRIG_TYPE_U16X2> u16x2_t;
typedef HsailType<u64_t,  u8_t,  BRIG_TYPE_U8X8 > u8x8_t;
typedef HsailType<u64_t,  u16_t, BRIG_TYPE_U16X4> u16x4_t;
typedef HsailType<u64_t,  u32_t, BRIG_TYPE_U32X2> u32x2_t;
typedef HsailType<b128_t, u8_t,  BRIG_TYPE_U8X16> u8x16_t;
typedef HsailType<b128_t, u16_t, BRIG_TYPE_U16X8> u16x8_t;
typedef HsailType<b128_t, u32_t, BRIG_TYPE_U32X4> u32x4_t;
typedef HsailType<b128_t, u64_t, BRIG_TYPE_U64X2> u64x2_t;

typedef HsailType<u32_t,  s8_t,  BRIG_TYPE_S8X4 > s8x4_t;
typedef HsailType<u32_t,  s16_t, BRIG_TYPE_S16X2> s16x2_t;
typedef HsailType<u64_t,  s8_t,  BRIG_TYPE_S8X8 > s8x8_t;
typedef HsailType<u64_t,  s16_t, BRIG_TYPE_S16X4> s16x4_t;
typedef HsailType<u64_t,  s32_t, BRIG_TYPE_S32X2> s32x2_t;
typedef HsailType<b128_t, s8_t,  BRIG_TYPE_S8X16> s8x16_t;
typedef HsailType<b128_t, s16_t, BRIG_TYPE_S16X8> s16x8_t;
typedef HsailType<b128_t, s32_t, BRIG_TYPE_S32X4> s32x4_t;
typedef HsailType<b128_t, s64_t, BRIG_TYPE_S64X2> s64x2_t;

typedef HsailType<u32_t,  f16_t, BRIG_TYPE_F16X2> f16x2_t;
typedef HsailType<u64_t,  f16_t, BRIG_TYPE_F16X4> f16x4_t;
typedef HsailType<u64_t,  f32_t, BRIG_TYPE_F32X2> f32x2_t;
typedef HsailType<b128_t, f16_t, BRIG_TYPE_F16X8> f16x8_t;
typedef HsailType<b128_t, f32_t, BRIG_TYPE_F32X4> f32x4_t;
typedef HsailType<b128_t, f64_t, BRIG_TYPE_F64X2> f64x2_t;

// ============================================================================
// ============================================================================
// ============================================================================
// Packing routines for packed data

inline b128_t b128(u64_t lo, u64_t hi) { b128_t res; res.set(lo, 0); res.set(hi, 1); return res; }

template<typename T, typename E>
inline T pack(E x0,        E x1,        E x2 = E(0),  E x3 = E(0),  E x4 = E(0),  E x5 = E(0),  E x6 = E(0),  E x7 = E(0),
              E x8 = E(0), E x9 = E(0), E x10 = E(0), E x11 = E(0), E x12 = E(0), E x13 = E(0), E x14 = E(0), E x15 = E(0))
{
    b128_t res;
    unsigned dim = sizeof(T) / sizeof(E);
    E data[16] = {x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15};

    for (unsigned i = 0; i < dim; ++i) res.set(data[i], dim - i - 1);
    for (unsigned i = dim; i < 16; ++i) { assert(data[i] == (E)0); }

    return res.get<T>();
}


template<typename T, typename E>
inline T fillBits(E x, unsigned mask, unsigned nelem)
{
    b128_t res;
    for (unsigned i = 0; i < nelem; ++i, mask >>= 1) res.set(E(x * (i + 1) * (mask & 0x1)), i);
    return res.get<T>();
}
template<typename T, typename E> inline T fillBits(E x, unsigned mask = 0xFFFFFFFF) { return fillBits<T,E>(x, mask, unsigned(sizeof(T) / sizeof(E))); }
template<typename T> inline T fillBits(f16_t x, unsigned mask = 0xFFFFFFFF) { return fillBits<T,f16_t>(x, mask, unsigned(sizeof(T) / sizeof(typename f16_t::bits_t))); }
template<typename T> inline T fillBits(f32_t x, unsigned mask = 0xFFFFFFFF) { return fillBits<T,f32_t>(x, mask, unsigned(sizeof(T) / sizeof(typename f32_t::bits_t))); }
template<typename T> inline T fillBits(f64_t x, unsigned mask = 0xFFFFFFFF) { return fillBits<T,f64_t>(x, mask, unsigned(sizeof(T) / sizeof(typename f64_t::bits_t))); }

#define u8x4  pack<u8x4_t,  u8_t>
#define u8x8  pack<u8x8_t,  u8_t>
#define u8x16 pack<u8x16_t, u8_t>
#define u16x2 pack<u16x2_t, u16_t>
#define u16x4 pack<u16x4_t, u16_t>
#define u16x8 pack<u16x8_t, u16_t>
#define u32x2 pack<u32x2_t, u32_t>
#define u32x4 pack<u32x4_t, u32_t>
#define u64x2 pack<u64x2_t, u64_t>

#define s8x4  pack<s8x4_t,  s8_t>
#define s8x8  pack<s8x8_t,  s8_t>
#define s8x16 pack<s8x16_t, s8_t>
#define s16x2 pack<s16x2_t, s16_t>
#define s16x4 pack<s16x4_t, s16_t>
#define s16x8 pack<s16x8_t, s16_t>
#define s32x2 pack<s32x2_t, s32_t>
#define s32x4 pack<s32x4_t, s32_t>
#define s64x2 pack<s64x2_t, s64_t>

#define f16x2 pack<f16x2_t, f16_t>
#define f16x4 pack<f16x4_t, f16_t>
#define f16x8 pack<f16x8_t, f16_t>
#define f32x2 pack<f32x2_t, f32_t>
#define f32x4 pack<f32x4_t, f32_t>
#define f64x2 pack<f64x2_t, f64_t>


#define fill_u8x4  fillBits<u8x4_t,  u8_t>
#define fill_u8x8  fillBits<u8x8_t,  u8_t>
#define fill_u8x16 fillBits<u8x16_t, u8_t>
#define fill_u16x2 fillBits<u16x2_t, u16_t>
#define fill_u16x4 fillBits<u16x4_t, u16_t>
#define fill_u16x8 fillBits<u16x8_t, u16_t>
#define fill_u32x2 fillBits<u32x2_t, u32_t>
#define fill_u32x4 fillBits<u32x4_t, u32_t>
#define fill_u64x2 fillBits<u64x2_t, u64_t>

#define fill_s8x4  fillBits<s8x4_t,  s8_t>
#define fill_s8x8  fillBits<s8x8_t,  s8_t>
#define fill_s8x16 fillBits<s8x16_t, s8_t>
#define fill_s16x2 fillBits<s16x2_t, s16_t>
#define fill_s16x4 fillBits<s16x4_t, s16_t>
#define fill_s16x8 fillBits<s16x8_t, s16_t>
#define fill_s32x2 fillBits<s32x2_t, s32_t>
#define fill_s32x4 fillBits<s32x4_t, s32_t>
#define fill_s64x2 fillBits<s64x2_t, s64_t>

#define fill_f16x2 fillBits<f16x2_t>
#define fill_f16x4 fillBits<f16x4_t>
#define fill_f16x8 fillBits<f16x8_t>
#define fill_f32x2 fillBits<f32x2_t, f32_t>
#define fill_f32x4 fillBits<f32x4_t, f32_t>
#define fill_f64x2 fillBits<f64x2_t, f64_t>

//=============================================================================
//=============================================================================
//=============================================================================
// Properties of integer types

template<typename T>
struct NumProps
{
    static unsigned width()     { return sizeof(T) * 8; }
    static unsigned shiftMask() { return width() - 1; }
};

template<typename T> bool isSigned(T val)   { return ((T)-1 < (T)0); }

// Compute number of bits required to represent 'range' values
inline u32_t range2width(unsigned range) // log2
{
    switch(range)
    {
    case 2:     return 1;
    case 4:     return 2;
    case 8:     return 3;
    case 16:    return 4;
    case 32:    return 5;
    case 64:    return 6;
    default:
        assert(false);
        return 0;
    }
}

inline u64_t getSignMask(unsigned width)  { return 1ULL << (width - 1); }
inline u64_t getWidthMask(unsigned width) { return ((width == 64)? 0 : (1ULL << width)) - 1ULL; }
inline u64_t getRangeMask(unsigned range) { return getWidthMask(range2width(range)); }

//==============================================================================
//==============================================================================
//==============================================================================
// Type Boundaries

u64_t getIntBoundary(unsigned type, bool low);

/// Depening on getMin, returns fp value of type T, which:
/// * is <= max allowed for respective INTEGER type, or
/// * is >= min allowed for respective INTEGER type.
template<typename T>
T getTypeBoundary(unsigned type, bool getMin);

//=============================================================================
//=============================================================================
//=============================================================================
// Helpers for generation of tests for rounding modes

unsigned getRoundingTestsNum(unsigned dstType);

const f16_t* getF16RoundingTestsData(unsigned dstType, AluMod aluMod);
const f32_t* getF32RoundingTestsData(unsigned dstType, AluMod aluMod);
const f64_t* getF64RoundingTestsData(unsigned dstType, AluMod aluMod);

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_EMULATOR_TYPES_H
