//===-- HSAILTestGenEmulatorTypes.h - HSAIL Test Generator  ===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

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

using std::string;
using std::ostringstream;
using std::setbase;
using std::setw;
using std::setfill;

using HSAIL_ASM::getBrigTypeNumBits;

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
    bool isNatural()           const { return isZero() || (getNormalizedFract() == 0 && (getExponent() >> MANTISSA_WIDTH) >= EXPONENT_BIAS); } //F1.0 optimize old code using decodeExponent() etc

public:
    static Type getQuietNan()     { return EXPONENT_MASK | NAN_TYPE_MASK; }
    static Type getNegativeZero() { return SIGN_MASK; }
    static Type getPositiveZero() { return 0; }
    static Type getNegativeInf()  { return SIGN_MASK | EXPONENT_MASK; }
    static Type getPositiveInf()  { return             EXPONENT_MASK; }

    static bool isValidExponent(int64_t exponenValue) // check _decoded_ exponent value
    {
        const int64_t minExp = -EXPONENT_BIAS;     // Reserved for subnormals
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

        //F1.0 "MANTISSA_MASK + 1" looks like a bug because there is no hidden bit for subnormals.
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

    // Add or substract 1 ULP
    // NB: This is not a generic code; it does not handle INF
    u64_t ulp(int64_t delta) const
    {
        assert(delta == -1 || delta == 1);

        if (isInf() || isNan()) return 0;

        // Handling of special values
        if (isZero() && delta == -1) return SIGN_MASK | 1;     // 0 -> -MIN_DENORM
        if (isZero() && delta ==  1) return             1;     // 0 -> +MIN_DENORM
        if (bits == (SIGN_MASK | 1) && delta == 1) return 0;   // -MIN_DENORM -> 0

        return getSign()? (bits - delta) : (bits + delta);
    }

    string hexDump()
    {
        ostringstream s;
        s << setbase(16) << setfill('0') << "0x" << setw(sizeof(T) * 2) << bits;
        return s.str();
    }

    static string dumpAsBin(u64_t x, unsigned width)
    {
        ostringstream s;
        x <<= (64 - width);
        for (unsigned i = 0; i < width; ++i) 
        {
            s << ((x & 0x8000000000000000ULL)? 1 : 0);
            x <<= 1;
        }
        return s.str();
    }

    string dump()
    {
        ostringstream s;

        s << getName() << ": ";

        if      (isPositiveZero()) s << "+0";
        else if (isNegativeZero()) s << "-0";
        else if (isPositiveInf())  s << "+Inf";
        else if (isNegativeInf())  s << "-Inf";
        else if (isQuietNan())     s << "QNaN";
        else if (isSignalingNan()) s << "SNan";
        else if (isSubnormal())    s << "Subnormal";
        else                       s << "Normal";

        s << " ("
          << (isPositive()? "0 " : "1 ")
          << dumpAsBin(getExponent() >> MANTISSA_WIDTH, sizeof(T) * 8 - MANTISSA_WIDTH - 1) << " "
          << dumpAsBin(getMantissa(), MANTISSA_WIDTH) << ") ["
          << hexDump() << "], EXP=" 
          << decodeExponent();

        return s.str();
    }

    const char* getName()
    {
        switch(sizeof(T) * 8)
        {
        case 16: return "f16";
        case 32: return "f32";
        case 64: return "f64";
        default: 
            assert(false);
            return 0;
        }
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
    static const unsigned RND_NEAR = BRIG_ROUND_FLOAT_NEAR_EVEN;
    static const unsigned RND_ZERO = BRIG_ROUND_FLOAT_ZERO;
    static const unsigned RND_UP   = BRIG_ROUND_FLOAT_PLUS_INFINITY;
    static const unsigned RND_DOWN = BRIG_ROUND_FLOAT_MINUS_INFINITY;

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
    static void sanityTests();
    string dump() { return FloatProp16(bits).dump(); }
};

inline f16_t operator+ (f16_t l, f16_t r) { return f16_t(l.f64() + r.f64()); }
inline f16_t operator- (f16_t l, f16_t r) { return f16_t(l.f64() - r.f64()); }

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

    template<typename T>
    T get(unsigned idx = 0) const
    {
        assert(idx < 16 / sizeof(T));
        return reinterpret_cast<const T*>(this)[idx];
    }

    f16_t get(unsigned idx = 0) const
    {
        assert(idx < 16 / sizeof(f16_t::bits_t));
        return f16_t::make(reinterpret_cast<const f16_t::bits_t*>(this)[idx]);
    }

    template<typename T>
    void set(T val, unsigned idx = 0)
    {
        assert(idx < 16 / sizeof(T));
        reinterpret_cast<T*>(this)[idx] = val;
    }

    void set(f16_t val, unsigned idx = 0)
    {
        assert(idx < 16 / sizeof(f16_t::bits_t));
        reinterpret_cast<f16_t::bits_t*>(this)[idx] = val.getBits();
    }

public:
    // Get value with sign-extension
    u64_t getElement(unsigned type, unsigned idx = 0) const
    {
        assert(idx < 128 / static_cast<unsigned>(getBrigTypeNumBits(type)));

        switch(type)
        {
        case BRIG_TYPE_S8:  return static_cast<s64_t>(get<s8_t> (idx));
        case BRIG_TYPE_S16: return static_cast<s64_t>(get<s16_t>(idx));
        case BRIG_TYPE_S32: return static_cast<s64_t>(get<s32_t>(idx));
        case BRIG_TYPE_S64: return static_cast<s64_t>(get<s64_t>(idx));

        case BRIG_TYPE_U8:  return get<u8_t>(idx);
        case BRIG_TYPE_U16: return get<u16_t>(idx);
        case BRIG_TYPE_U32: return get<u32_t>(idx);
        case BRIG_TYPE_U64: return get<u64_t>(idx);

        case BRIG_TYPE_F16: return get<u16_t>(idx);
        case BRIG_TYPE_F32: return get<u32_t>(idx);
        case BRIG_TYPE_F64: return get<u64_t>(idx);

        default:
            assert(false);
            return 0;
        }
    }

    void setElement(u64_t val, unsigned type, unsigned idx = 0)
    {
        assert(idx < 128 / static_cast<unsigned>(getBrigTypeNumBits(type)));

        switch(type)
        {
        case BRIG_TYPE_S8:  set(static_cast<s8_t> (val), idx); return;
        case BRIG_TYPE_S16: set(static_cast<s16_t>(val), idx); return;
        case BRIG_TYPE_S32: set(static_cast<s32_t>(val), idx); return;
        case BRIG_TYPE_S64: set(static_cast<s64_t>(val), idx); return;

        case BRIG_TYPE_U8:  set(static_cast<u8_t> (val), idx); return;
        case BRIG_TYPE_U16: set(static_cast<u16_t>(val), idx); return;
        case BRIG_TYPE_U32: set(static_cast<u32_t>(val), idx); return;
        case BRIG_TYPE_U64: set(static_cast<u64_t>(val), idx); return;

        case BRIG_TYPE_F16: set(static_cast<u16_t>(val), idx); return;
        case BRIG_TYPE_F32: set(static_cast<u32_t>(val), idx); return;
        case BRIG_TYPE_F64: set(static_cast<u64_t>(val), idx); return;

        default:
            assert(false);
            return;
        }
    }

public:
    bool operator==(const b128_t& rhs) const { return (get<u64_t>(0) == rhs.get<u64_t>(0)) && (get<u64_t>(1) == rhs.get<u64_t>(1)); }

    string hexDump() const
    {
        ostringstream s;
        s << "_b128(";
        s << setbase(16) << setfill('0');
        s << "0x" << setw(8) << get<u32_t>(3) << ",";
        s << "0x" << setw(8) << get<u32_t>(2) << ",";
        s << "0x" << setw(8) << get<u32_t>(1) << ",";
        s << "0x" << setw(8) << get<u32_t>(0) << ")";
        return s.str();
    }
};

template <typename OS> inline OS& operator <<(OS& os, const b128_t& v) { os << v.hexDump(); return os; }

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
inline T fillBits(E x, unsigned mask = 0xFFFFFFFF)
{
    b128_t res;
    unsigned dim = sizeof(T) / sizeof(E);

    for (unsigned i = 0; i < dim; ++i, mask >>= 1) res.set(static_cast<E>(x * (i + 1) * (mask & 0x1)), i);

    return res.get<T>();
}

template<typename T>
inline T fillBits_f16(f16_t x, unsigned mask = 0xFFFFFFFF)
{
    b128_t res;
    unsigned dim = sizeof(T) / sizeof(f16_t);

    for (unsigned i = 0; i < dim; ++i, mask >>= 1) res.set(f16_t(x.f64() * (i + 1) * (mask & 0x1)), i);

    return res.get<T>();
}

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

#define fill_f16x2 fillBits_f16<f16x2_t>
#define fill_f16x4 fillBits_f16<f16x4_t>
#define fill_f16x8 fillBits_f16<f16x8_t>
#define fill_f32x2 fillBits<f32x2_t, f32_t>
#define fill_f32x4 fillBits<f32x4_t, f32_t>
#define fill_f64x2 fillBits<f64x2_t, f64_t>

//=============================================================================
//=============================================================================
//=============================================================================
//

#define HEX2F32(x) (*reinterpret_cast<const f32_t*>(&x))
#define HEX2F64(x) (*reinterpret_cast<const f64_t*>(&x))

#define F32_2U(x) (*reinterpret_cast<const u32_t*>(&x))
#define F64_2U(x) (*reinterpret_cast<const u64_t*>(&x))

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

f16_t getTypeBoundary_f16(unsigned type, bool isLo);
f32_t getTypeBoundary_f32(unsigned type, bool isLo);
f64_t getTypeBoundary_f64(unsigned type, bool isLo);

template<typename T>
static T getTypeBoundary(unsigned type, bool isLo)
{
    assert(HSAIL_ASM::isIntType(type));

    if (sizeof(T) == 2) return (T)getTypeBoundary_f16(type, isLo);
    if (sizeof(T) == 4) return (T)getTypeBoundary_f32(type, isLo);
    if (sizeof(T) == 8) return (T)getTypeBoundary_f64(type, isLo);

    assert(false);
    return (T)0.0f;
}

u64_t getIntBoundary(unsigned type, bool low);

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
