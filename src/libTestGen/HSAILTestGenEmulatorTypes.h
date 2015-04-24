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

template<typename T, int mantissa_width> class IEEE754;

/// \brief Floating-point rounding modes
/// \note "enum struct Round {}" is more type-safe, but requires a lot of reworking.
static const unsigned RND_NEAR = BRIG_ROUND_FLOAT_NEAR_EVEN;
static const unsigned RND_ZERO = BRIG_ROUND_FLOAT_ZERO;
static const unsigned RND_PINF = BRIG_ROUND_FLOAT_PLUS_INFINITY;
static const unsigned RND_MINF = BRIG_ROUND_FLOAT_MINUS_INFINITY;

/// \brief Represents numeric value splitted to sign/exponent/mantissa.
///
/// Able to hold any numeric value of any supported type (integer or floating-point)
/// Mantissa is stored with hidden bit, if it is set. Bit 0 is LSB of mantissa.
/// Exponent is stored in decoded (unbiased) format.
///
/// Manupulations with mantissa, exponent and sign are obviouous,
/// so these members are exposed to outsize world.
struct DecodedFpValue {
    uint64_t mant; // with hidden bit
    int64_t exp; // powers of 2
    bool sign; // == is negative // FIXME
    int mant_width; // not counting hidden bit

    template<typename T, int mw>
    DecodedFpValue(const IEEE754<T, mw>& props)
    : mant(0), exp(0), sign(props.isNegative()), mant_width(mw)
    {
        if (props.isInf() || props.isNan()) {
          assert(!"Input number must represent a numeric value here");
          return;
        }
        mant = props.getMantissa();
        if (!props.isSubnormal() && !props.isZero()) mant |= IEEE754<T,mw>::MANT_HIDDEN_MSB_MASK;
        exp = props.decodeExponent();
    }
private:
    template<typename T> DecodedFpValue(T); // = delete;
public:
    /// \todo use enable_if<is_signed<T>>::value* p = NULL and is_integral<> to extend to other intergal types
    /// \todo implement also ctors from floating types
    explicit DecodedFpValue(uint64_t val)
    : mant(val)
    , exp((int64_t)sizeof(val)*8-1) // 
    , sign(false) // unsigned
    , mant_width((int)sizeof(val)*8-1) // hidden bit is MSB of input integer
    {
    }
};

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

/// Order of fields (msb-to-lsb) is fixed to: sign, exponent, mantissa.
/// Fields always occupy all bits of the underlying Type (which carries binary representation).
/// Sign is always at MSB in Type, exponent occupies all bits in the middle.
template<typename T, int mantissa_width>
class IEEE754
{
public:
    typedef T Type;

private:        

    static const int  EXP_WIDTH  = (int)sizeof(Type)*8 - 1 - mantissa_width;
    static const int  EXP_BIAS   = ~((-1) << EXP_WIDTH) / 2;
    static const int  DECODED_EXP_NORM_MAX  = EXP_BIAS;   // if greater, then INF or NAN

    static const Type MANT_MASK  = (Type)~((Type)(-1) << mantissa_width);
    static const Type EXP_MASK   = (Type)(~(Type(-1) << EXP_WIDTH) << mantissa_width);
    static const Type SIGN_MASK  = (Type)((Type)1 << ((int)sizeof(Type)*8 - 1));
public: // FIXME try to get rid of those
    static const int  MANT_WIDTH = mantissa_width;
    static const Type MANT_HIDDEN_MSB_MASK = MANT_MASK + 1;  // Highest '1' bit of mantissa assumed for normalized numbers
    static const int  DECODED_EXP_NORM_MIN  = 1-EXP_BIAS; // if less, then subnormal
    static const int  DECODED_EXP_SUBNORMAL_OR_ZERO = 0-EXP_BIAS; // by definition, sum with EXP_BIAS should yield 0

private:        
    static_assert(0 < mantissa_width && mantissa_width < sizeof(Type)*8 - 1 - 2, // leave sign + 2 bits for exp at least
        "Wrong mantissa width");
    static_assert(EXP_WIDTH <= (int)sizeof(int)*8 - 1,
        "Exponent field is too wide");
    static_assert( (1 << (sizeof(int)*8 - EXP_WIDTH)) > mantissa_width,
        "'int' type is too small for exponent calculations");
    static_assert(SIGN_MASK > 0, // simple "Type(-1) > 0" does not work. MSVC issue?
        "Underlying Type (for bits) must be unsigned.");

    static const Type MANT_MSB_MASK        = MANT_HIDDEN_MSB_MASK >> 1; /// \todo It is strange that we need it
    static const Type NAN_TYPE_MASK        = MANT_MSB_MASK;

    Type bits;

    //template<typename TT> IEEE754(TT); // = delete;
public:
    explicit IEEE754(Type val) : bits(val) {}
    IEEE754(bool isPositive, uint64_t mantissa, int64_t decodedExponent)
    {
        Type exponent = (Type)(decodedExponent + EXP_BIAS) << MANT_WIDTH;

        assert((exponent & ~EXP_MASK) == 0);
        assert((mantissa & ~MANT_MASK) == 0);

        bits = (isPositive? 0 : SIGN_MASK) | 
               (exponent & EXP_MASK)  | 
               (mantissa & MANT_MASK);
    }

    explicit IEEE754(const DecodedFpValue &decoded)
    {
        assert((decoded.mant & ~(MANT_HIDDEN_MSB_MASK | MANT_MASK)) == 0);
        assert(decoded.mant_width == MANT_WIDTH);
        if (decoded.exp > DECODED_EXP_NORM_MAX) {
            // INF or NAN. By design, DecodedFpValue is unable to represent NANs.
            bits = decoded.sign ? getNegativeInf() : getPositiveInf();
            return;
        }
        assert(decoded.exp < DECODED_EXP_NORM_MIN ? decoded.exp == DECODED_EXP_SUBNORMAL_OR_ZERO : true);
        assert(!(decoded.mant & MANT_HIDDEN_MSB_MASK) ? decoded.exp == DECODED_EXP_SUBNORMAL_OR_ZERO : true);
        Type exp_bits = (Type)(decoded.exp + EXP_BIAS) << MANT_WIDTH;
        assert((exp_bits & ~EXP_MASK) == 0);

        bits = (!decoded.sign ? 0 : SIGN_MASK)
             | (exp_bits & EXP_MASK)
             | (decoded.mant & MANT_MASK); // throw away hidden bit
    }

public:
    Type getBits() const { return bits; }

public:
    Type getSign()      const { return bits & SIGN_MASK; }
    Type getMantissa()  const { return bits & MANT_MASK; }
    Type getExponent()  const { return bits & EXP_MASK; }
    Type getNanType()   const { return bits & NAN_TYPE_MASK; }

public:
    bool isPositive()          const { return getSign() == 0; }
    bool isNegative()          const { return getSign() != 0; }

    bool isZero()              const { return getExponent() == 0 && getMantissa() == 0; }
    bool isPositiveZero()      const { return isZero() && isPositive();  }
    bool isNegativeZero()      const { return isZero() && !isPositive(); }

    bool isInf()               const { return getExponent() == EXP_MASK && getMantissa() == 0; }
    bool isPositiveInf()       const { return isInf() && isPositive();  }
    bool isNegativeInf()       const { return isInf() && !isPositive(); }

    bool isNan()               const { return getExponent() == EXP_MASK && getMantissa() != 0; }
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
    bool isNatural()           const { return isZero() || (getNormalizedFract() == 0 && (getExponent() >> MANT_WIDTH) >= EXP_BIAS); } //F1.0 optimize old code using decodeExponent() etc

public:
    static Type getQuietNan()     { return EXP_MASK | NAN_TYPE_MASK; }
    static Type getNegativeZero() { return SIGN_MASK; }
    static Type getPositiveZero() { return 0; }
    static Type getNegativeInf()  { return SIGN_MASK | EXP_MASK; }
    static Type getPositiveInf()  { return             EXP_MASK; }

#if 1 // FIXME
    static bool isValidExponent(int64_t decodedExp) {
        return decodedExp >= DECODED_EXP_NORM_MIN
            && decodedExp <= DECODED_EXP_NORM_MAX;
    }
#endif

public:
    template<typename BT, int MW>
    static Type mapSpecialValues(const IEEE754<BT,MW>& val)
    {
        assert(!val.isRegular());

        if      (val.isPositiveZero()) { return getPositiveZero();  }
        else if (val.isNegativeZero()) { return getNegativeZero();  }
        else if (val.isPositiveInf())  { return getPositiveInf();   }
        else if (val.isNegativeInf())  { return getNegativeInf();   }
        else if (val.isQuietNan())     { return getQuietNan();      }
        else if (val.isSignalingNan()) { assert(false); }

        assert(false);
        return 0;
    }

#if 1 // FIXME
    /// \todo Oversimplified
    /// * to smaller - truncates
    /// * to bigger - LSBs are zero-filled
    template<typename TargetType> uint64_t mapNormalizedMantissaFIXME() const // map to target type
    {
        assert(!isSubnormal() && isRegular());
        int tmw = TargetType::MANT_WIDTH; // must assign to var && no 'const' here, otherwise warning and undefined behavior in MSVC
        assert(tmw != MANT_WIDTH);
        const uint64_t mantissa = getMantissa();
        if (tmw < MANT_WIDTH) {
          return mantissa >> (MANT_WIDTH - tmw);
        }
        return mantissa << (tmw - MANT_WIDTH);
    }
#endif

    template<typename TargetType> uint64_t tryNormalizeMantissaUpdateExponent(int64_t& exponent) const
    {
        assert(sizeof(Type) != sizeof(typename TargetType::Type));

        if (sizeof(Type) < sizeof(typename TargetType::Type)) // Map subnormal to a larger type
        {
            assert(isSubnormal());

            // unused: unsigned targetTypeSize = (unsigned)sizeof(typename TargetType::Type) * 8;

            uint64_t mantissa = getMantissa();
            mantissa <<= sizeof(mantissa)*8 - MANT_WIDTH;

            assert(mantissa != 0);

            bool normalized = false;
            for (; !normalized && TargetType::isValidExponent(exponent - 1); )
            {
                normalized = (mantissa & 0x8000000000000000ULL) != 0;
                mantissa <<= 1; // it is ok to throw away found '1' bit (it is implied in normalized numbers)
                exponent--;
            }
            if (!normalized) exponent = TargetType::DECODED_EXP_SUBNORMAL_OR_ZERO; // subnormal -> subnormal

            return mantissa >> (sizeof(mantissa)*8 - TargetType::MANT_WIDTH);
        }
        else // Map regular value with large negative mantissa to a smaller type (resulting in subnormal or 0)
        {
            assert(exponent < 0);
            assert(!TargetType::isValidExponent(exponent));

            uint64_t mantissa = getMantissa();

            // Add hidden bit of mantissa
            mantissa = MANT_MSB_MASK | (mantissa >> 1);
            exponent++;

            for (; !TargetType::isValidExponent(exponent); )
            {
                mantissa >>= 1;
                exponent++;
            }

            exponent = TargetType::DECODED_EXP_SUBNORMAL_OR_ZERO;
            int nExtraBits = (MANT_WIDTH - TargetType::MANT_WIDTH);
            assert(nExtraBits >= 0);
            return mantissa >> nExtraBits;
        }
    }

    // Return exponent as a signed number
    int64_t decodeExponent() const
    {
        int64_t e = getExponent() >> MANT_WIDTH;
        return e - EXP_BIAS;
    }

    // Return fractional part of fp number
    // normalized so that x-th digit is at (63-x)-th bit of u64_t
    uint64_t getNormalizedFract(int x = 0) const
    {
        if (isInf() || isNan()) {
          assert(!"Input number must represent a numeric value here");
          return 0;
        }

        uint64_t mantissa = getMantissa();
        if (!isSubnormal() && !isZero()) mantissa |= MANT_HIDDEN_MSB_MASK;
        int exponent = static_cast<int>(getExponent() >> MANT_WIDTH);

        // Compute shift required to place 1st fract bit at 63d bit of u64_t
        int width = static_cast<int>(sizeof(mantissa) * 8);
        int shift = (exponent - EXP_BIAS) + (width - MANT_WIDTH) + x;
        if (shift <= -width || width <= shift) return 0;
        return (shift >= 0)? mantissa << shift : mantissa >> (-shift);
    }

    /// FIXME Transforms mantissa of DecodedFpValue to this IEEE754<> format and normalizes it. May also adjust exponent.
    /// Transformation of mantissa includes shrinking/widening, IEEE754-compliant rounding and normalization.
    /// Adjusting of exponent may be required due to rounding and normalization.
    /// Mantissa as always normalized at the end. Exponent range is not guaranteed to be valid, though.
    class TransformMantissaAdjustExponent {
        DecodedFpValue& v;
        const unsigned rounding;
        int srcMantIsWiderBy;
        enum TieKind {
          TIE_UNKNOWN = 0,
          TIE_LIST_BEGIN_,
          TIE_IS_ZERO = TIE_LIST_BEGIN_, // exact match, no rounding required, just truncate
          TIE_LT_HALF, // tie bits ~= 0bb..bb, where at least one b == 1
          TIE_IS_HALF, // tie bits are exactly 100..00
          TIE_GT_HALF,  // tie bits ~= 1bb..bb, where at least one b == 1
          TIE_LIST_END_
        } tieKind;
        // For manipulations with DecodedFpValue we use MANT_ bitfields/masks from IEEE754<>.
        static_assert((MANT_MASK & 0x1), "LSB of mantissa in IEEE754<> must be is at bit 0");

    public:
        TransformMantissaAdjustExponent(DecodedFpValue& v_, unsigned r_)
        : v(v_), rounding(r_), srcMantIsWiderBy(v.mant_width - MANT_WIDTH), tieKind(TIE_UNKNOWN)
        {
            normalizeInputMantAdjustExp();
            calculateTieKind();
            roundMantAdjustExp();
            normalizeMantAdjustExp();
        }

    private:
        void incrementMantAdjustExp() const // does not care about exponent limits.
        {
            assert(v.mant <= MANT_HIDDEN_MSB_MASK + MANT_MASK);
            ++v.mant;
            if (v.mant > MANT_HIDDEN_MSB_MASK + MANT_MASK) { // overflow
                v.mant >>= 1;
                ++v.exp;
            }
        }
        
        void normalizeInputMantAdjustExp()
        {
            if(v.mant == 0) {
                return; // no need (and impossible) to normalize zero.
            }
            const uint64_t INPUT_MANT_HIDDEN_MSB_MASK = uint64_t(1) << v.mant_width;
            while (! (v.mant & INPUT_MANT_HIDDEN_MSB_MASK) ) {
                v.mant <<= 1;
                --v.exp;
            }
        }

        void calculateTieKind() // FIXME move to .cpp
        {
            if (srcMantIsWiderBy <= 0) {
              tieKind = TIE_IS_ZERO; // widths are equal or source is narrower
              return;
            }
            const uint64_t SRC_MANT_TIE_MASK = ((uint64_t)1 << srcMantIsWiderBy) - 1;
            const uint64_t TIE_VALUE_HALF = (uint64_t)1 << (srcMantIsWiderBy - 1); // 100..00B
            const uint64_t tie = v.mant & SRC_MANT_TIE_MASK;
            if (tie == 0             ) { tieKind = TIE_IS_ZERO; return; }
            if (tie == TIE_VALUE_HALF) { tieKind = TIE_IS_HALF; return; }
            if (tie <  TIE_VALUE_HALF) { tieKind = TIE_LT_HALF; return; }
            tieKind = TIE_GT_HALF;
        }

        void roundMantAdjustExp() // FIXME move to .cpp  // does not care about exponent limits.
        {
            assert(TIE_LIST_BEGIN_ <= tieKind && tieKind < TIE_LIST_END_);
            if (srcMantIsWiderBy > 0) {
                // truncate mant, then adjust depending on (pre-calculated) tail kind:
                v.mant >>= srcMantIsWiderBy;
                switch (rounding) {
                default: assert(0); // fall
                case RND_NEAR:
                    switch(tieKind) {
                    case TIE_IS_ZERO: case TIE_LT_HALF: default:
                        break;
                    case TIE_IS_HALF:
                        if (v.mant & 1) incrementMantAdjustExp(); // if mant is odd, increment to even
                        break;
                    case TIE_GT_HALF:
                        incrementMantAdjustExp();
                        break;
                    }
                    break;
                case RND_ZERO:
                    break;
                case RND_PINF:
                    switch(tieKind) {
                    case TIE_IS_ZERO: default:
                        break;
                    case TIE_LT_HALF: case TIE_IS_HALF: case TIE_GT_HALF:
                        if (!v.sign) incrementMantAdjustExp();
                        break;
                    }
                    break;
                case RND_MINF:
                    switch(tieKind) {
                    case TIE_IS_ZERO: default:
                        break;
                    case TIE_LT_HALF: case TIE_IS_HALF: case TIE_GT_HALF:
                        if (v.sign) incrementMantAdjustExp();
                        break;
                    }
                    break;
                }
            } else { // destination mantisa is wider or has the same width.
                // this conversion is always exact
                v.mant <<= (0 - srcMantIsWiderBy); // fill by 0's from the left
            }
            v.mant_width = MANT_WIDTH;
            srcMantIsWiderBy = 0;
        }
        
        void normalizeMantAdjustExp() const  // does not care about upper exp limit.
        {
            assert (v.mant_width == MANT_WIDTH && srcMantIsWiderBy == 0 && "Mantissa must be in destination format here.");
            assert((v.mant & ~(MANT_HIDDEN_MSB_MASK | MANT_MASK)) == 0 && "Wrong mantissa, unused upper bits must be 0.");
            if(v.mant == 0) {
                v.exp = DECODED_EXP_SUBNORMAL_OR_ZERO;
                return; // no need (and impossible) to normalize zero.
            }
            if (v.exp >= DECODED_EXP_NORM_MIN) {
                if (! (v.mant & MANT_HIDDEN_MSB_MASK)) { // try to normalize
                    while (v.exp > DECODED_EXP_NORM_MIN && !(v.mant & MANT_HIDDEN_MSB_MASK)) {
                        v.mant <<= 1;
                        --v.exp;
                    }
                    // end up with (1) either exp is minimal or (2) hidden bit is 1 or (3) both
                    if (v.exp > DECODED_EXP_NORM_MIN) {
                        assert(v.mant & MANT_HIDDEN_MSB_MASK);
                        // exp is above min limit, mant is normalized
                        // nothing to do
                    } else {
                        assert(v.exp == DECODED_EXP_NORM_MIN);
                        if (! (v.mant & MANT_HIDDEN_MSB_MASK)) {
                            // we dropped exp down to minimal, but mant is still subnormal
                            v.exp = DECODED_EXP_SUBNORMAL_OR_ZERO;
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
                assert(v.exp < DECODED_EXP_NORM_MIN);
                // try to lift up exponent to reach the minimum... at the expense of losing LSBs of mantissa
                /// \todo ROUNDING!
                // we will always end up with zero or subnormal
                while (v.exp < DECODED_EXP_NORM_MIN && v.mant != 0) {
                    v.mant >>= 1;
                    ++v.exp;
                }
                if (v.mant != 0) {
                    assert(v.exp == DECODED_EXP_NORM_MIN);
                    v.exp = DECODED_EXP_SUBNORMAL_OR_ZERO;
                } else {
                    // zero, nothing to do
                    v.exp = DECODED_EXP_SUBNORMAL_OR_ZERO;
                }
            }
        }
    };

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
          << dumpAsBin(getExponent() >> MANT_WIDTH, sizeof(T) * 8 - MANT_WIDTH - 1) << " "
          << dumpAsBin(getMantissa(), MANT_WIDTH) << ") ["
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

typedef IEEE754<uint16_t, 10> FloatProp16;
typedef IEEE754<uint32_t, 23> FloatProp32;
typedef IEEE754<uint64_t, 52> FloatProp64;

//==============================================================================
//==============================================================================
//==============================================================================

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

// Finding property types for floating types
template<typename T> struct FloatProp; // allowed pairs of types:
template<> struct FloatProp<float>  { typedef FloatProp32 PropType; };
template<> struct FloatProp<double> { typedef FloatProp64 PropType; };

//==============================================================================
//==============================================================================
//==============================================================================
// F16 type

class f16_t
{
public:
    typedef FloatProp16::Type bits_t;
private:

private:
    bits_t bits;

public:
    f16_t() {}
    explicit f16_t(double x, unsigned rounding = RND_NEAR) { convertFrom(x,rounding); }
    explicit f16_t(float x, unsigned rounding = RND_NEAR) { convertFrom(x,rounding); }
    //explicit f16_t(f64_t x, unsigned rounding = RND_NEAR) : f16_t(double(x), rounding) {};
    //explicit f16_t(f32_t x, unsigned rounding = RND_NEAR) : f16_t(float(x), rounding) {};
    explicit f16_t(int32_t x) { bits = f16_t((f64_t)x).getBits(); }
private:
    template<typename T> void convertFrom(T x, unsigned rounding) {
        typedef typename FloatProp<T>::PropType InProp;
        InProp input(asBits(x));
        if (!input.isRegular()) {
            bits = FloatProp16::mapSpecialValues(input);
            return;
        }
        DecodedFpValue val(input);
        FloatProp16::TransformMantissaAdjustExponent(val,rounding);
        bits = FloatProp16(val).getBits();
    }

public:
    f32_t f32() const { return convertTo<float>(); };
    f64_t f64() const { return convertTo<double>(); };
    operator float() const { return f32(); }
    operator double() const { return f64(); }
private:
    template<typename T> T convertTo() const {
        FloatProp16 f16(bits);
        typedef typename FloatProp<T>::PropType OutFloatProp;
        typename OutFloatProp::Type outbits;
        if (!f16.isRegular()) {
            outbits = OutFloatProp::mapSpecialValues(f16);
        } else if (f16.isSubnormal()) {
            assert(f16.decodeExponent() == FloatProp16::DECODED_EXP_SUBNORMAL_OR_ZERO);
            int64_t exponent = FloatProp16::DECODED_EXP_NORM_MIN;
            uint64_t mantissa = f16.tryNormalizeMantissaUpdateExponent<OutFloatProp>(exponent);
            OutFloatProp outprop(f16.isPositive(), mantissa, exponent);
            outbits = outprop.getBits();
        } else {
            int64_t exponent = f16.decodeExponent();
            uint64_t mantissa = f16.mapNormalizedMantissaFIXME<OutFloatProp>();
            OutFloatProp outprop(f16.isPositive(), mantissa, exponent);
            outbits = outprop.getBits();
        }
        return asFloating(outbits);
    }

public:
    bool operator>  (const f16_t& x) const { return f64() >  x.f64(); } /// \todo reimplement all via f32 at least
    bool operator<  (const f16_t& x) const { return f64() <  x.f64(); }
    bool operator>= (const f16_t& x) const { return f64() >= x.f64(); }
    bool operator<= (const f16_t& x) const { return f64() <= x.f64(); }
    bool operator== (const f16_t& x) const { return f64() == x.f64(); }

public:
    f16_t& operator+= (f16_t x) { bits = f16_t(f64() + x.f64()).bits; return *this; }

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
