//===-- HSAILTestGenFpEmulator.h - HSAIL Test Generator  ===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef INCLUDED_HSAIL_TESTGEN_FP_EMULATOR_H
#define INCLUDED_HSAIL_TESTGEN_FP_EMULATOR_H

#include "HSAILTestGenEmulatorTypes.h"

namespace TESTGEN {

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

template<typename T, T sign_mask, T exponent_mask, T mantissa_mask, T mantissa_msb_mask, unsigned exponent_bias, unsigned mantissa_width>
class IEEE754
{
public:
    typedef T Type;

    static const T SIGN_MASK         = sign_mask;
    static const T EXPONENT_MASK     = exponent_mask;
    static const T MANTISSA_MASK     = mantissa_mask;
    static const T MANTISSA_MSB_MASK = mantissa_msb_mask;
    static const T NAN_TYPE_MASK     = mantissa_msb_mask;

    static const unsigned EXPONENT_BIAS  = exponent_bias;
    static const unsigned MANTISSA_WIDTH = mantissa_width;

private:
    T bits;

public:
    IEEE754(T val) : bits(val) {}
    IEEE754(bool isPositive, u64_t mantissa, s64_t decodedExponent)
    {
        T exponent = (T)(decodedExponent + EXPONENT_BIAS) << MANTISSA_WIDTH;

        assert((exponent & ~EXPONENT_MASK) == 0);
        assert((mantissa & ~MANTISSA_MASK) == 0);

        bits = (isPositive? 0 : SIGN_MASK) | 
               (exponent & EXPONENT_MASK)  | 
               (mantissa & MANTISSA_MASK);
    }

public:
    T getBits() const { return bits; }

public:
    T getSign()      const { return bits & SIGN_MASK; }
    T getMantissa()  const { return bits & MANTISSA_MASK; }
    T getExponent()  const { return bits & EXPONENT_MASK; }
    T getNanType()   const { return bits & NAN_TYPE_MASK; }

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
    static T getQuietNan()     { return EXPONENT_MASK | NAN_TYPE_MASK; }
    static T getNegativeZero() { return SIGN_MASK; }
    static T getPositiveZero() { return 0; }
    static T getNegativeInf()  { return SIGN_MASK | EXPONENT_MASK; }
    static T getPositiveInf()  { return             EXPONENT_MASK; }

    static bool isValidExponent(s64_t exponenValue) // check _decoded_ exponent value
    {
        s64_t minExp = - (s64_t)EXPONENT_BIAS;      // Reserved for subnormals
        s64_t maxExp =   (s64_t)EXPONENT_BIAS + 1;  // Reserved for Infs and NaNs
        return minExp < exponenValue && exponenValue < maxExp;
    }

    static s64_t decodedSubnormalExponent() { return - (s64_t)EXPONENT_BIAS; }   // Decoded exponent value which indicates a subnormal
    static s64_t actualSubnormalExponent()  { return 1 - (s64_t)EXPONENT_BIAS; } // Actual decoded exponent value for subnormals

public:
    template<typename TargetType> u64_t mapSpecialValues()
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

    template<typename TargetType> u64_t mapNormalizedMantissa() // map to target type
    {
        u64_t mantissa = getMantissa();
        u64_t targetMantissaWidth = TargetType::MANTISSA_WIDTH;

        assert(targetMantissaWidth != MANTISSA_WIDTH);

        if (targetMantissaWidth < MANTISSA_WIDTH) return mantissa >> (MANTISSA_WIDTH - targetMantissaWidth);
        else                                      return mantissa << (targetMantissaWidth - MANTISSA_WIDTH);
    }

    template<typename TargetType> u64_t normalizeMantissa(s64_t& exponent)
    {
        assert(sizeof(Type) != sizeof(TargetType::Type));

        if (sizeof(Type) < sizeof(TargetType::Type)) // Map subnormal to a larger type
        {
            assert(isSubnormal());

            unsigned targetTypeSize = (unsigned)sizeof(TargetType::Type) * 8;

            u64_t mantissa = getMantissa();
            mantissa <<= 64 - MANTISSA_WIDTH;

            assert(mantissa != 0);

            bool found = false;
            for (; !found && TargetType::isValidExponent(exponent - 1); )
            {
                found = (mantissa & 0x8000000000000000ULL) != 0;
                mantissa <<= 1;
                exponent--;
            }
            if (!found) exponent = TargetType::decodedSubnormalExponent(); // subnormal -> subnormal

            return mantissa >> (64 - TargetType::MANTISSA_WIDTH);
        }
        else // Map regular value with large negative mantissa to a smaller type (resulting in subnormal or 0)
        {
            assert(exponent < 0);
            assert(!TargetType::isValidExponent(exponent));
            assert(sizeof(Type) > sizeof(TargetType::Type));

            u64_t mantissa = getMantissa();

            // Add hidden bit of mantissa
            mantissa = MANTISSA_MSB_MASK | (mantissa >> 1);
            exponent++;

            for (; !TargetType::isValidExponent(exponent); )
            {
                mantissa >>= 1;
                exponent++;
            }

            exponent = TargetType::decodedSubnormalExponent();
            unsigned delta = (MANTISSA_WIDTH - TargetType::MANTISSA_WIDTH);
            return mantissa >> delta;
        }
    }

    // Return exponent as a signed number
    s64_t decodeExponent()
    {
        s64_t e = getExponent() >> MANTISSA_WIDTH;
        return e - EXPONENT_BIAS;
    }

    // Return fractional part of fp number
    // normalized so that x-th digit is at (63-x)-th bit of u64_t
    u64_t getNormalizedFract(int x = 0) const
    {
        if (isZero() || isInf() || isNan()) return 0;

        u64_t mantissa = getMantissa() | (MANTISSA_MASK + 1); // Highest bit of mantissa is not encoded but assumed
        int exponent = static_cast<int>(getExponent() >> MANTISSA_WIDTH);

        // Compute shift required to place 1st fract bit at 63d bit of u64_t
        int width = static_cast<int>(sizeof(u64_t) * 8);
        int shift = (exponent - EXPONENT_BIAS) + (width - MANTISSA_WIDTH) + x;
        if (shift <= -width || width <= shift) return 0;
        return (shift >= 0)? mantissa << shift : mantissa >> (-shift);
    }

    T negate()      const { return (getSign()? 0 : SIGN_MASK) | getExponent() | getMantissa(); }
    T copySign(T v) const { return (v & SIGN_MASK) | getExponent() | getMantissa(); }

    T normalize(bool discardNanSign) const // Clear NaN payload and sign
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
        u16_t,
        0x8000,  // Sign
        0x7C00,  // Exponent
        0x03ff,  // Mantissa
        0x0200,  // Mantissa MSB
        15,      // Exponent bias
        10       // Mantissa width
    > FloatProp16;

typedef IEEE754
    <
        u32_t,
        0x80000000,  // Sign
        0x7f800000,  // Exponent
        0x007fffff,  // Mantissa
        0x00400000,  // Mantissa MSB
        127,         // Exponent bias
        23           // Mantissa width
    > FloatProp32;

typedef IEEE754
    <
        u64_t,
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

class f16_t;
f16_t f16_todo();

class f16_t
{
private:
    static const unsigned RND_NEAR = Brig::BRIG_ROUND_FLOAT_NEAR_EVEN;
    static const unsigned RND_ZERO = Brig::BRIG_ROUND_FLOAT_ZERO;
    static const unsigned RND_UP   = Brig::BRIG_ROUND_FLOAT_PLUS_INFINITY;
    static const unsigned RND_DOWN = Brig::BRIG_ROUND_FLOAT_MINUS_INFINITY;

private:
    u16_t bits;

public:
    f16_t() {}

    explicit f16_t(f64_t x, unsigned rounding = RND_NEAR);
    explicit f16_t(f32_t x, unsigned rounding = RND_NEAR) { f16_todo(); }
    explicit f16_t(s32_t x) { bits = f16_t((f64_t)x).bits; }

public:
    bool operator>  (const f16_t& x) const { return f64() >  x.f64(); }
    bool operator<  (const f16_t& x) const { return f64() <  x.f64(); }
    bool operator>= (const f16_t& x) const { return f64() >= x.f64(); }
    bool operator<= (const f16_t& x) const { return f64() <= x.f64(); }
    bool operator== (const f16_t& x) const { return f64() == x.f64(); }

public:
    f16_t& operator+= (f16_t x) { bits = f16_t(f64() + x.f64()).bits; return *this; }

public:
    f32_t f32()      const { f16_todo(); return 0; };
    f64_t f64()      const;
    operator f32_t() const { return f32(); }
    operator f64_t() const { return f64(); }

public:
    f16_t neg() const { return make(FloatProp16(bits).negate()); }

public:
    static f16_t make(u16_t bits) { f16_t res; res.bits = bits; return res; }

public:
    static void sanityTests();
    string dump() { return FloatProp16(bits).dump(); }
};

inline f16_t operator+ (f16_t l, f16_t r) { return f16_t(l.f64() + r.f64()); }
inline f16_t operator- (f16_t l, f16_t r) { return f16_t(l.f64() - r.f64()); }

inline f16_t f16_todo()
{
    //assert(false);
    return f16_t(666.0);
}

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Precision of Emulation

double getNativeOpPrecision(unsigned opcode, unsigned type);

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Supported Rounding Modes

// NB: ROUNDING_NONE must be supported
bool isSupportedFpRounding(unsigned rounding);

void validateFpRounding(unsigned rounding);

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Conversions

template<typename T> f16_t emulate_i2f16(T val, unsigned fpRounding) { validateFpRounding(fpRounding); return f16_todo(); } //TODO
template<typename T> f32_t emulate_i2f32(T val, unsigned fpRounding) { validateFpRounding(fpRounding); return 0.0f + val; } //TODO: rounding
template<typename T> f64_t emulate_i2f64(T val, unsigned fpRounding) { validateFpRounding(fpRounding); return 0.0  + val; } //TODO: rounding

// Convert val to dstType using integer rounding.
// If dstType is a signed type, it is converted to s64_t.
// isValid indicates if conversion was successful.
// NB: signalign forms must be handled the same as non-signaling ones.
//     Checks for exceptions are implemented elsewhere
u64_t emulate_f2i(f16_t val, unsigned dstType, unsigned intRounding, bool& isValid);
u64_t emulate_f2i(f32_t val, unsigned dstType, unsigned intRounding, bool& isValid);
u64_t emulate_f2i(f64_t val, unsigned dstType, unsigned intRounding, bool& isValid);

f16_t emulate_f2f16(f16_t,     unsigned           ); // unused
f16_t emulate_f2f16(f32_t val, unsigned fpRounding);
f16_t emulate_f2f16(f64_t val, unsigned fpRounding);

f32_t emulate_f2f32(f16_t val, unsigned           );
f32_t emulate_f2f32(f32_t,     unsigned           ); // unused
f32_t emulate_f2f32(f64_t val, unsigned fpRounding);

f64_t emulate_f2f64(f16_t val, unsigned           );
f64_t emulate_f2f64(f32_t val, unsigned           );
f64_t emulate_f2f64(f64_t,     unsigned           ); // unused

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Comparisons

// Return -1 if val1 < val2; return 1 if val1 > val2; return 0 otherwise
// if either operand is a NaN, then the result must be false
int emulate_cmp(f16_t val1, f16_t val2);
int emulate_cmp(f32_t val1, f32_t val2);
int emulate_cmp(f64_t val1, f64_t val2);

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Truncations

f16_t emulate_fract(f16_t val);
f32_t emulate_fract(f32_t val);
f64_t emulate_fract(f64_t val);

f16_t emulate_ceil(f16_t val);
f32_t emulate_ceil(f32_t val);
f64_t emulate_ceil(f64_t val);

f16_t emulate_floor(f16_t val);
f32_t emulate_floor(f32_t val);
f64_t emulate_floor(f64_t val);

f16_t emulate_trunc(f16_t val);
f32_t emulate_trunc(f32_t val);
f64_t emulate_trunc(f64_t val);

f16_t emulate_rint(f16_t val);
f32_t emulate_rint(f32_t val);
f64_t emulate_rint(f64_t val);

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Bit Operations

f16_t emulate_cpsgn(f16_t val1, f16_t val2);
f32_t emulate_cpsgn(f32_t val1, f32_t val2);
f64_t emulate_cpsgn(f64_t val1, f64_t val2);

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Standard Arithmetic

f16_t emulate_abs(f16_t val);
f32_t emulate_abs(f32_t val);
f64_t emulate_abs(f64_t val);

f16_t emulate_neg(f16_t val);
f32_t emulate_neg(f32_t val);
f64_t emulate_neg(f64_t val);

f16_t emulate_add(f16_t val1, f16_t val2, unsigned fpRounding);
f32_t emulate_add(f32_t val1, f32_t val2, unsigned fpRounding);
f64_t emulate_add(f64_t val1, f64_t val2, unsigned fpRounding);

f16_t emulate_sub(f16_t val1, f16_t val2, unsigned fpRounding);
f32_t emulate_sub(f32_t val1, f32_t val2, unsigned fpRounding);
f64_t emulate_sub(f64_t val1, f64_t val2, unsigned fpRounding);

f16_t emulate_mul(f16_t val1, f16_t val2, unsigned fpRounding);
f32_t emulate_mul(f32_t val1, f32_t val2, unsigned fpRounding);
f64_t emulate_mul(f64_t val1, f64_t val2, unsigned fpRounding);

f16_t emulate_div(f16_t val1, f16_t val2, unsigned fpRounding);
f32_t emulate_div(f32_t val1, f32_t val2, unsigned fpRounding);
f64_t emulate_div(f64_t val1, f64_t val2, unsigned fpRounding);

f16_t emulate_max(f16_t val1, f16_t val2);
f32_t emulate_max(f32_t val1, f32_t val2);
f64_t emulate_max(f64_t val1, f64_t val2);

f16_t emulate_min(f16_t val1, f16_t val2);
f32_t emulate_min(f32_t val1, f32_t val2);
f64_t emulate_min(f64_t val1, f64_t val2);

f16_t emulate_fma(f16_t val1, f16_t val2, f16_t val3, unsigned fpRounding);
f32_t emulate_fma(f32_t val1, f32_t val2, f32_t val3, unsigned fpRounding);
f64_t emulate_fma(f64_t val1, f64_t val2, f64_t val3, unsigned fpRounding);

f16_t emulate_sqrt(f16_t val, unsigned fpRounding);
f32_t emulate_sqrt(f32_t val, unsigned fpRounding);
f64_t emulate_sqrt(f64_t val, unsigned fpRounding);

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Native Arithmetic

f16_t emulate_nfma(f16_t val1, f16_t val2, f16_t val3);
f32_t emulate_nfma(f32_t val1, f32_t val2, f32_t val3);
f64_t emulate_nfma(f64_t val1, f64_t val2, f64_t val3);

f16_t emulate_nsqrt(f16_t val);
f32_t emulate_nsqrt(f32_t val);
f64_t emulate_nsqrt(f64_t val);

f16_t emulate_nrsqrt(f16_t val);
f32_t emulate_nrsqrt(f32_t val);
f64_t emulate_nrsqrt(f64_t val);

f16_t emulate_nrcp(f16_t val);
f32_t emulate_nrcp(f32_t val);
f64_t emulate_nrcp(f64_t val);

f16_t emulate_ncos(f16_t val, bool& isValidArg);
f32_t emulate_ncos(f32_t val, bool& isValidArg);
f64_t emulate_ncos(f64_t val, bool& isValidArg);

f16_t emulate_nsin(f16_t val, bool& isValidArg);
f32_t emulate_nsin(f32_t val, bool& isValidArg);
f64_t emulate_nsin(f64_t val, bool& isValidArg);

f16_t emulate_nexp2(f16_t val);
f32_t emulate_nexp2(f32_t val);
f64_t emulate_nexp2(f64_t val);

f16_t emulate_nlog2(f16_t val);
f32_t emulate_nlog2(f32_t val);
f64_t emulate_nlog2(f64_t val);

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_FP_EMULATOR_H