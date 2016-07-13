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

#include "HSAILTestGenEmulatorTypes.h"
#include "HSAILTestGenFpEmulator.h"
#include "HSAILTestGenVal.h"

using namespace HSAIL_ASM;

#include <limits>
#include <string>
#include <sstream>

#define FMA_F64_SUPPORT 3 // generic implementation, independent from CPU and/or compiler.

#ifndef FMA_F64_SUPPORT
#define FMA_F64_SUPPORT 0
#if __cplusplus > 199711L // C++11
  // #pragma STDC FENV_ACCESS ON
  // #pragma STDC FP_CONTRACT OFF
  #undef FMA_F64_SUPPORT
  #define FMA_F64_SUPPORT 1 //use c++11 for fma
#else
  #ifdef _MSC_VER
    #undef FMA_F64_SUPPORT
    #define FMA_F64_SUPPORT 2 //use sse fma instructions
  #endif
#endif
#if 0 && defined(__GNUC__)
  // Workaround for gcc's C++11 fma() double-rounding and sign-of zero issues.
  // NOTE 1: Does NOT work on AMD HW because gcc maps amd's fma4 instructions to intel's fma3.
  // NOTE 2: -msse2 -mfma4 options required.
  #undef FMA_F64_SUPPORT
  #define FMA_F64_SUPPORT 2 //use sse fma instructions
#endif
#if (FMA_F64_SUPPORT == 0)
  #error "Unable to emulate HSAIL fma f64. Either C++11 or MSVC SSE/SSE2/FMA intrinsics required."
#endif

#if (FMA_F64_SUPPORT == 1)
  #include <cfenv>
  #include <cmath>
#endif
#if (FMA_F64_SUPPORT == 2)
  #ifdef _MSC_VER
    #include <intrin.h> // __m128d etc
  #else
    #include <x86intrin.h>
  #endif
  #include <ammintrin.h> // fma (amd's FMA4)
  #include <xmmintrin.h> // _MM_SET_ROUNDING_MODE

  struct
  #ifdef _MSC_VER
    __declspec(align(16))
  #else
    __attribute__ ((aligned (16)))
  #endif
  m128aligner {
    double val[2];
    explicit m128aligner(double x) { val[0] = x; val[1] = 0.0; }
    double* operator() (){ return &val[0]; }
  };
#endif
#endif

namespace TESTGEN {

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
        shrMant += (T::DECODED_EXP_NORM_MIN - static_cast<int>(v.exp));
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
    if (shrMant >= static_cast<int>(sizeof(v.mant)*8 - 1)) {
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
        if (shrMant >= static_cast<int>(sizeof(v.mant)*8)) { // we need this since C++0x
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
        assert((0 - shrMant) < static_cast<int>(sizeof(v.mant)*8));
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
    const typename T::bits_t* res = reinterpret_cast<typename T::bits_t*>(&x);
    const typename T::props_t input(*res);
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

//=============================================================================
//=============================================================================
//=============================================================================

namespace FP_MATH_M128 {

class uint128_t {
    uint64_t lo;
    uint64_t hi;
public:
    uint128_t() : lo(0), hi(0) {}
    explicit uint128_t(uint64_t lo_) : lo(lo_), hi(0) {}
    explicit uint128_t(uint64_t lo_, uint64_t hi_) : lo(lo_), hi(hi_) {}
    uint64_t getLo() const { return lo; }
    uint128_t& operator|= (uint64_t x) { lo |= x; return *this; }
    uint128_t& operator++ () { if(++lo == 0) { ++hi; } return *this; }
    uint128_t& operator-- () { if(lo == 0) { --hi; } --lo; return *this; }
    uint128_t& operator>>= (int64_t x);
    uint128_t& operator>>= (int x) { return (*this) >>= int64_t(x); }
    uint128_t& operator<<= (int x);
    uint128_t  operator<<  (int x) const { return uint128_t(*this) <<= x; }
    uint128_t  operator-   (const uint128_t &y) const;
    uint128_t  operator+   (const uint128_t &y) const;
    uint128_t  operator&   (uint64_t y) const { return uint128_t(lo & y); }
    uint128_t  operator&   (const uint128_t &y) const { return uint128_t(lo & y.lo, hi & y.hi); }
    bool operator<= (uint64_t x) const { return hi == 0 && lo <= x; }
    bool operator<  (uint64_t x) const { return hi == 0 && lo < x; }
    bool operator>  (uint64_t x) const { return hi != 0 || lo > x; }
    bool operator== (uint64_t x) const { return hi == 0 && lo == x; }
    bool operator!= (uint64_t x) const { return !(*this == x); }
    bool operator== (const uint128_t &x) const { return hi == x.hi && lo == x.lo; }
    bool operator<  (const uint128_t &x) const { return hi < x.hi ? true : (hi == x.hi ? lo < x.lo : false); }
    bool operator>  (const uint128_t &x) const { return hi > x.hi ? true : (hi == x.hi ? lo > x.lo : false); }
};

template<typename T> static T explicit_static_cast(const uint128_t &); // compilation error
template<> inline uint64_t explicit_static_cast<uint64_t>(const uint128_t &x) { return x.getLo(); }
template<> inline uint32_t explicit_static_cast<uint32_t>(const uint128_t &x) { return static_cast<uint32_t>(x.getLo()); }
template<> inline uint16_t explicit_static_cast<uint16_t>(const uint128_t &x) { return static_cast<uint16_t>(x.getLo()); }

uint128_t& uint128_t::operator>>= (int64_t x)
{
    assert(x >= 0);
    if (x >= int(sizeof(lo)*8 + sizeof(hi)*8)) {
        lo =0; hi = 0;
        return *this;
    }
    for (; x > 0; --x) { // straightforward impl, can be made faster
        lo >>= 1;
        if (hi & 1) { lo |= (uint64_t(1) << 63); }
        hi >>= 1;
    }
    return *this;
}

uint128_t& uint128_t::operator<<= (int x)
{
    assert(x >= 0);
    if (x >= int(sizeof(lo)*8 + sizeof(hi)*8)) {
        lo =0; hi = 0;
        return *this;
    }
    for (; x > 0; --x) { // straightforward impl, can be made faster
        hi <<= 1;
        if (lo & (uint64_t(1) << 63)) { hi |= 1; }
        lo <<= 1;
    }
    return *this;
}

uint128_t uint128_t::operator- (const uint128_t &y) const
{
    uint128_t rv(lo - y.lo, 0);
    const uint64_t borrow = ((rv.lo & y.lo & 0x1) + (y.lo >> 1) + (rv.lo >> 1)) >> (64 - 1);
    rv.hi = hi - (y.hi + borrow);
    return rv;
}

uint128_t uint128_t::operator+ (const uint128_t &y) const
{
    const uint64_t carry = (((lo & y.lo) & 0x1) + (lo >> 1) + (y.lo >> 1)) >> (64 - 1);
    return uint128_t(lo + y.lo, hi + y.hi + carry);
}

static
uint128_t mul_u128_u64(const uint64_t x, const uint64_t y)
{
    const uint64_t x_lo = x & 0xffffffff;
    const uint64_t y_lo = y & 0xffffffff;
    const uint64_t mul_lo = x_lo * y_lo;
    const uint64_t mul_lo_lo = mul_lo & 0xffffffff;
    const uint64_t mul_lo_hi = mul_lo >> 32;

    const uint64_t x_hi = x >> 32;
    const uint64_t mul_mid = (x_hi * y_lo) + mul_lo_hi;
    const uint64_t mul_mid_lo = (mul_mid & 0xffffffff);
    const uint64_t mul_mid_hi = (mul_mid >> 32);

    const uint64_t y_hi = y >> 32;
    const uint64_t mul_mid2 = (x_lo * y_hi) + mul_mid_lo;
    const uint64_t mul_mid2_hi = (mul_mid2 >> 32);

    const uint64_t res_hi = (x_hi * y_hi) + mul_mid_hi + mul_mid2_hi;
    const uint64_t res_lo = (mul_mid2 << 32) + mul_lo_lo;
    return uint128_t(res_lo, res_hi);
}

typedef DecodedFpValueImpl<uint128_t> decoded_t;
typedef decoded_t::mant_t mant_t;

template<typename T> // shortcut to simplify code
struct FpProcessorM128 : public FpProcessorImpl<T, uint128_t> {
    explicit FpProcessorM128(FP_MATH_M128::decoded_t& v_) : FpProcessorImpl<T, uint128_t>(v_) {}
};

template <typename T> static
bool mulSpecial(T &out, const T &p0, const T &p1)
{
    const bool isResultNegative = p0.isNegative() ^ p1.isNegative();
    if (p0.isNan()) {
        out = p0.isSignalingNan() ? p0.quietedSignalingNan() : p0;
        return true;
    }
    if (p1.isNan()) {
        out = p1.isSignalingNan() ? p1.quietedSignalingNan() : p1;
        return true;
    }
    if (p0.isInf()) {
        if (p1.isZero()) {
            out = T::makeQuietNan(isResultNegative, 0);
        } else {
            out = isResultNegative ? T::getNegativeInf() : T::getPositiveInf();
        }
        return true;
    }
    if (p1.isInf()) {
        if (p0.isZero()) {
            out = T::makeQuietNan(isResultNegative, 0);
        } else {
            out = isResultNegative ? T::getNegativeInf() : T::getPositiveInf();
        }
        return true;
    }
    if (p0.isZero() || p1.isZero()) {
        out = isResultNegative ? T::getNegativeZero() : T::getPositiveZero();
        return true;
    }
    return false;
}

template <typename T> static
void mulNumeric(decoded_t &out, const T &p0, const T &p1)
{
    const bool isResultNegative = p0.isNegative() ^ p1.isNegative();
    DecodedFpValue dv0(p0);
    DecodedFpValue dv1(p1);
    {
        FpProcessor<T> fp0(dv0);
        FpProcessor<T> fp1(dv1);
        fp0.NormalizeInputMantAdjustExp();
        fp1.NormalizeInputMantAdjustExp();
        out.exp = dv0.exp + dv1.exp - fp0.getShrMant() - fp1.getShrMant();
    }
    out.mant = mul_u128_u64(dv0.mant, dv1.mant);
    out.mant_width = (T::MANT_WIDTH + 1)*2 - 1; // not counting hidden bit
    out.is_negative = isResultNegative;
    // The weight of MSB of product's mantissa is 2, but should be 1. Correction is required:
    {
        const uint128_t product_msb_mask = uint128_t(1) << out.mant_width;
        if ((out.mant & product_msb_mask) == 0) {
            out.mant <<= 1;
        } else {
            ++out.exp;
        }
    }
}

}; // namespace FP_MATH_M128

template <typename T>
T mul(const T &src0, const T & src1, unsigned rounding)
{
    typedef typename T::props_t props_t; // shortcut
    using namespace FP_MATH_M128;
    const props_t p0(src0.props());
    const props_t p1(src1.props());
    props_t product_special(typename T::bits_t(0));
    if (mulSpecial(product_special, p0, p1)) {
        return T(product_special);
    }
    decoded_t product_numeric;
    mulNumeric(product_numeric, p0, p1);
    FpProcessorM128<props_t>(product_numeric).NormalizeRound(rounding);
    return T(product_numeric.toProps<props_t>());
}

// explicitly instantiate for the types we need:
template f64_t mul(const f64_t &src0, const f64_t & src1, unsigned rounding);
template f32_t mul(const f32_t &src0, const f32_t & src1, unsigned rounding);
template f16_t mul(const f16_t &src0, const f16_t & src1, unsigned rounding);

namespace FP_MATH_M128 {

template <typename T> static
bool addSpecial(T &out, const T &p0, const T &p1)
{
    if (p0.isNan()) {
        out = p0.isSignalingNan() ? p0.quietedSignalingNan() : p0;
        return true;
    }
    if (p1.isNan()) {
        out = p1.isSignalingNan() ? p1.quietedSignalingNan() : p1;
        return true;
    }
    if (p0.isInf() && p1.isInf() && (p0.isNegative() ^ p1.isNegative())) {
        out = T::makeQuietNan(false, 0);
        return true;
    }
    if (p0.isInf()) {
        out = p0;
        return true;
    }
    if (p1.isInf()) {
        out = p1;
        return true;
    }
    if (p0.isZero() && p1.isZero()) {
        out = (p0.isNegative() && p1.isNegative()) ? T::getNegativeZero() : T::getPositiveZero();
        return true;
    }
    if (p0.isZero()) {
        out = p1;
        return true;
    }
    if (p1.isZero()) {
        out = p0;
        return true;
    }
    return false;
}

template <typename T> static
bool addSpecial(T &out, bool &outIsSource0, const decoded_t &p0, const T &p1)
{
    outIsSource0 = false;
    if (p1.isNan()) {
        out = p1.isSignalingNan() ? p1.quietedSignalingNan() : p1;
        return true;
    }
    if (p1.isInf()) {
        out = p1;
        return true;
    }
    if (p1.isZero()) {
        outIsSource0 = true; // out = p0
        return true;
    }
    return false;
}

static inline
void widenMant(decoded_t &v, int64_t new_width_)
{
    const int new_width = static_cast<int>(new_width_);
    assert((new_width - v.mant_width) >= 0);
    v.mant <<= (new_width - v.mant_width);
    v.mant_width = new_width;
}

static inline
void normalizeMantDenorms(decoded_t &v)
{
    const mant_t hidden_msb_mask = mant_t(1) << v.mant_width;
    while ((v.mant & hidden_msb_mask) == 0 ) {
        v.mant <<= 1;
        --v.exp;
    }
}

static inline
void shrMantIncExpRememberOmicron(decoded_t &v, int64_t count_)
{
    const int count = static_cast<int>(count_);
    const mant_t tail_mask = (mant_t(1) << count) - mant_t(1);
    v.add_omicron = ((v.mant & tail_mask) == 0) ? decoded_t::ADD_OMICRON_ZERO : decoded_t::ADD_OMICRON_POSITIVE; // sign of omicron is not imporant here
    v.mant >>= count;
    v.exp += count;
}

static inline
void subtract(decoded_t &out, decoded_t &x, decoded_t &y)
{
    out.mant = x.mant - y.mant;
    out.exp = x.exp + 1; /*(1.1)*/
    out.is_negative = x.is_negative;
    assert(x.add_omicron == decoded_t::ADD_OMICRON_ZERO || y.add_omicron == decoded_t::ADD_OMICRON_ZERO);
    if (x.add_omicron != decoded_t::ADD_OMICRON_ZERO) {
        out.add_omicron = decoded_t::ADD_OMICRON_POSITIVE;
    } else if (y.add_omicron != decoded_t::ADD_OMICRON_ZERO) {
        out.add_omicron = decoded_t::ADD_OMICRON_NEGATIVE;
    }
}

void addNumeric(decoded_t &out, decoded_t &dv0, decoded_t &dv1, int destination_fp_mant_width)
{
    assert(dv0.mant != 0 && dv1.mant != 0);
    // Find mant_width required for output.
    // (1) To avoid precision loss, it must be not less than any of inputs, plus 1.
    //     Examples for 3-bit inputs:
    //        1.000B + 1.000B = 10.000
    //        1.111B + 1.111B = 11.110 // required width is 4
    //     (1.1) Note that MSB weight of output mant is 2 (exp correction will be required).
    // (2) We also need 2 extra bits (wrt mant width of destination fp number).
    //     That is for correct rounding (we need to correctly select one of 5 possible tail kinds).
    //     (2.1) One more bit is required because MSB can be cleared during subtraction.
    out.mant_width = std::max(dv0.mant_width, dv1.mant_width) + 1; // (1)
    out.mant_width = std::max(out.mant_width, destination_fp_mant_width + 2 + 1); // (2), (2.1)
    // Prior actual addition, we need identical mant_width of inputs, shorter by 1 bit than output:
    widenMant(dv0, out.mant_width - 1);
    widenMant(dv1, out.mant_width - 1);
    // Normalize denormal inputs to canonical form (hidden bit must be 1)
    normalizeMantDenorms(dv0);
    normalizeMantDenorms(dv1);
    // shr value with smaller exp (and ++exp) until both exp become the same.
    // Remember the lost tail (add_omicron), if there is something lost or just zeroes.
    {
        const int64_t diff_exp = dv0.exp - dv1.exp;
        if (diff_exp > 0) shrMantIncExpRememberOmicron(dv1, diff_exp);
        if (diff_exp < 0) shrMantIncExpRememberOmicron(dv0, -diff_exp);
    }
    // Now we can add/sub mantissas and find out sign of the result.
    // Exp can be taken from any source operand (except zero result).
    // Also pass omicron with proper sign.
    if (dv0.is_negative ^ dv1.is_negative) {
        // signs are different. Avoid subtraction of greater value from smaller one.
        if (dv0.mant > dv1.mant) {
            subtract(out, dv0, dv1);
        } else if (dv0.mant < dv1.mant) {
            subtract(out, dv1, dv0);
        } else { // zero
            out.mant = mant_t(0);
            out.exp = 0;
            out.is_negative = false; // always +0.0
            assert(dv0.add_omicron == decoded_t::ADD_OMICRON_ZERO && dv1.add_omicron == decoded_t::ADD_OMICRON_ZERO);
            out.add_omicron = decoded_t::ADD_OMICRON_ZERO;
        }
    } else {
        // signs are the same
        out.mant = dv0.mant + dv1.mant;
        out.exp = dv0.exp + 1; // (1.1)
        out.is_negative = dv0.is_negative;
        if ((dv0.add_omicron != decoded_t::ADD_OMICRON_ZERO)
         || (dv1.add_omicron != decoded_t::ADD_OMICRON_ZERO)) { out.add_omicron = decoded_t::ADD_OMICRON_POSITIVE; }
    }
}

}; // namespace FP_MATH_M128

template <typename T>
T add(const T &src0, const T & src1, unsigned rounding)
{
    using namespace FP_MATH_M128;
    typedef typename T::props_t props_t; // shortcut 
    const props_t p0(src0.props());
    const props_t p1(src1.props());
    props_t sum_special(typename T::bits_t(0));
    if (addSpecial(sum_special, p0, p1)) {
        return T(sum_special);
    }
    decoded_t sum_numeric;
    {
        decoded_t dv0(p0);
        decoded_t dv1(p1);
        addNumeric(sum_numeric, dv0, dv1, props_t::MANT_WIDTH);
    }
    FpProcessorM128<props_t>(sum_numeric).NormalizeRound(rounding);
    return T(sum_numeric.toProps<props_t>());
}

// explicitly instantiate for the types we need:
template f64_t add(const f64_t &src0, const f64_t & src1, unsigned rounding);
template f32_t add(const f32_t &src0, const f32_t & src1, unsigned rounding);
template f16_t add(const f16_t &src0, const f16_t & src1, unsigned rounding);

template <typename T>
T fma(const T &src0, const T & src1, const T & src2, unsigned rounding)
{
    using namespace FP_MATH_M128;
    typedef typename T::props_t props_t; // shortcut 
    typedef typename T::bits_t bits_t; // shortcut 
    const props_t p2(src2.props());
    decoded_t product;
    {
        const props_t p0(src0.props());
        const props_t p1(src1.props());
        props_t product_special(bits_t(0));
        if (mulSpecial(product_special, p0, p1)) {
            props_t sum_special(bits_t(0));
            if (addSpecial(sum_special, product_special, p2)) {
                return T(sum_special);
            }
            assert(!"If mul result is special (NAN/INF/ZERO), then add must take special path as well");
            return T(props_t::makeSignalingNan(false, 1));
        }
        mulNumeric(product, p0, p1);
    }
    // We need two Ieee754Props objects to process possible special fp values before actual addition.
    // But Ieee754Props object can not be constructed from the result of multiply (product),
    // because product's format does not correspond to standard ieee-754 floating-point
    // (i.e. mantissa is too wide etc). after first glance, it seems to be OK to use rounded/normalized
    // product value (in standard ieee-754 format) for the handling of special values, but this is not true.
    // Example case is when product is too big but 3rd operand of fma is negative and big as well,
    // so the overall result of fma would fit into ieee-754 fp range. If we construct product_props from product,
    // then product_props would be INF and that that would immediately lead to a "special" case, which is wrong.
    //
    // However, we know that product is NOT a special fp number at this point.
    // Let's use addSpecial() kind which accepts DecodedFpVal (as src0) and props (as src1).
    decoded_t sum_numeric;
    props_t sum_special(bits_t(0));
    bool sum_is_product = false;
    if (addSpecial(sum_special, sum_is_product, product, p2)) {
        if (!sum_is_product) {
            return T(sum_special);
        }
        sum_numeric = product;
    } else {
        decoded_t dv2(p2);
        addNumeric(sum_numeric, product, dv2, props_t::MANT_WIDTH);
    }
    FpProcessorM128<props_t>(sum_numeric).NormalizeRound(rounding);
    return T(sum_numeric.toProps<props_t>());
}

// explicitly instantiate for the types we need:
template f64_t fma(const f64_t &src0, const f64_t & src1, const f64_t & src2, unsigned rounding);
template f32_t fma(const f32_t &src0, const f32_t & src1, const f32_t & src2, unsigned rounding);
template f16_t fma(const f16_t &src0, const f16_t & src1, const f16_t & src2, unsigned rounding);

#if (FMA_F64_SUPPORT == 1) || (FMA_F64_SUPPORT == 2)

#if (FMA_F64_SUPPORT == 1)
static int toCpp11round(unsigned rounding)
{
    switch (rounding) {
    default: assert(0); //fall
    case RND_NEAR: return FE_TONEAREST;
    case RND_ZERO: return FE_TOWARDZERO;
    case RND_PINF: return FE_UPWARD;
    case RND_MINF: return FE_DOWNWARD;
    }
}
#endif

#if (FMA_F64_SUPPORT == 2)
static unsigned toSseRound(unsigned rounding)
{
    switch (rounding) {
    default: assert(0); //fall
    case RND_NEAR: return _MM_ROUND_NEAREST;
    case RND_ZERO: return _MM_ROUND_TOWARD_ZERO;
    case RND_PINF: return _MM_ROUND_UP;
    case RND_MINF: return _MM_ROUND_DOWN;
    }
}
#endif

f64_t fma(const f64_t &a, const f64_t & b, const f64_t &c, unsigned rounding)
{
#if (FMA_F64_SUPPORT == 1)
  int save = std::fegetround();
  if (save < 0) { save = FE_TONEAREST; } // if getround fails, defaults to this.
  int ec = std::fesetround(toCpp11round(rounding));
  assert(ec == 0 && "std::fesetround() failed to set rounding mode.");
  //
  const double res = std::fma(a.floatValue(), b.floatValue(), c.floatValue());
  //
  ec = std::fesetround(save);
  assert(ec == 0 && "std::fesetround() failed to restore rounding mode.");
#endif
#if (FMA_F64_SUPPORT == 2)
  unsigned save = _mm_getcsr(); // sse <intrin.h>
  _MM_SET_ROUNDING_MODE(toSseRound(rounding));
  //
  __m128d xmm_a = _mm_load_pd(m128aligner(a.floatValue())()); // sse2 <intrin.h>
  __m128d xmm_b = _mm_load_pd(m128aligner(b.floatValue())());
  __m128d xmm_c = _mm_load_pd(m128aligner(c.floatValue())());
  __m128d xmm_res = _mm_macc_pd(xmm_a, xmm_b, xmm_c); // fma4 amd <ammintrin.h>
  m128aligner res_store(0.0);
  _mm_store_pd(res_store(), xmm_res); // sse2 <intrin.h>
  const double res = *res_store();
  //
  _mm_setcsr(save); // sse <intrin.h>
#endif
  return f64_t(&res);
}
#endif

//=============================================================================
//=============================================================================
//=============================================================================

template<typename T, typename INTEGER> static inline
T getTypeBoundary(bool getMin)
{
    if (getMin) {
        if (-std::numeric_limits<T>::max() >= std::numeric_limits<INTEGER>::min()) {
            return -std::numeric_limits<T>::max();
        } else {
            // RND_ZERO to cut off extra LSBs of mantissa.
            return T(std::numeric_limits<INTEGER>::min(), RND_ZERO);
        }
    } else {
        if (std::numeric_limits<T>::max() <= std::numeric_limits<INTEGER>::max()) {
            return std::numeric_limits<T>::max();
        } else {
            return T(std::numeric_limits<INTEGER>::max(), RND_ZERO);
        }
    }
}

template<typename T>
T getTypeBoundary(unsigned type, bool getMin)
{
    switch (type)
    {
        case BRIG_TYPE_S8:  return getTypeBoundary<T,s8_t >(getMin);
        case BRIG_TYPE_S16: return getTypeBoundary<T,s16_t>(getMin);
        case BRIG_TYPE_S32: return getTypeBoundary<T,s32_t>(getMin);
        case BRIG_TYPE_S64: return getTypeBoundary<T,s64_t>(getMin);
        case BRIG_TYPE_U8:  return getTypeBoundary<T,u8_t >(getMin);
        case BRIG_TYPE_U16: return getTypeBoundary<T,u16_t>(getMin);
        case BRIG_TYPE_U32: return getTypeBoundary<T,u32_t>(getMin);
        case BRIG_TYPE_U64: return getTypeBoundary<T,u64_t>(getMin);
        default: assert(0); return T();
    }
}

// explicitly instantiate for the types we need:
template f16_t getTypeBoundary(unsigned type, bool getMin);
template f32_t getTypeBoundary(unsigned type, bool getMin);
template f64_t getTypeBoundary(unsigned type, bool getMin);

//=============================================================================
//=============================================================================
//=============================================================================

u64_t b128_t::getElement(unsigned type, unsigned idx) const
{
    assert(idx < 128 / static_cast<unsigned>(HSAIL_ASM::getBrigTypeNumBits(type)));

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

void b128_t::setElement(u64_t val, unsigned type, unsigned idx)
{
    assert(idx < 128 / static_cast<unsigned>(HSAIL_ASM::getBrigTypeNumBits(type)));

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


string b128_t::hexDump() const
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

//=============================================================================
//=============================================================================
//=============================================================================
// Min and max values for integer types (used for saturating rounding)

static const s64_t S8IB  = 0x7f;
static const s64_t S16IB = 0x7fff;
static const s64_t S32IB = 0x7fffffff;
static const s64_t S64IB = 0x7fffffffffffffffLL;
static const u64_t U8IB  = 0xff;
static const u64_t U16IB = 0xffff;
static const u64_t U32IB = 0xffffffff;
static const u64_t U64IB = 0xffffffffffffffffLL;

u64_t getIntBoundary(unsigned type, bool low)
{
    switch (type)
    {
    case BRIG_TYPE_S8:  return low? -S8IB  - 1 : S8IB;
    case BRIG_TYPE_S16: return low? -S16IB - 1 : S16IB;
    case BRIG_TYPE_S32: return low? -S32IB - 1 : S32IB;
    case BRIG_TYPE_S64: return low? -S64IB - 1 : S64IB;
    case BRIG_TYPE_U8:  return low? 0          : U8IB;
    case BRIG_TYPE_U16: return low? 0          : U16IB;
    case BRIG_TYPE_U32: return low? 0          : U32IB;
    case BRIG_TYPE_U64: return low? 0          : U64IB;

    default:
        assert(false);
        return 0;
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Helpers for generation of tests for rounding modes

static const unsigned ROUNDING_TESTS_NUM = 12;

unsigned getRoundingTestsNum(unsigned dstType)
{
    return (isSignedType(dstType) || isUnsignedType(dstType))? ROUNDING_TESTS_NUM : 1;
}

template<typename T>
const void makeRoundingTestsData(unsigned dstType, AluMod aluMod, T* dst)
{
    if (getRoundingTestsNum(dstType) == 1)
    {
        dst[0] = (T)0.0; // dummy test data (cannot return empty list)
        return;
    }

    T lo = getTypeBoundary<T>(dstType, true);
    T hi = getTypeBoundary<T>(dstType, false);

    switch(aluMod.getRounding())
    {
    case AluMod::ROUNDING_NEARI:
    case AluMod::ROUNDING_NEARI_SAT:
    case AluMod::ROUNDING_SNEARI:
    case AluMod::ROUNDING_SNEARI_SAT:
                                        lo += (T)0.5;
                                        hi += (T)0.5;
                                        break;

    case AluMod::ROUNDING_ZEROI:
    case AluMod::ROUNDING_ZEROI_SAT:
    case AluMod::ROUNDING_SZEROI:
    case AluMod::ROUNDING_SZEROI_SAT:
                                        if (lo > (T)0.0) lo += (T)1.0;
                                        if (hi > (T)0.0) hi += (T)1.0;
                                        break;

    case AluMod::ROUNDING_DOWNI:
    case AluMod::ROUNDING_DOWNI_SAT:
    case AluMod::ROUNDING_SDOWNI:
    case AluMod::ROUNDING_SDOWNI_SAT:
                                        lo += (T)1.0;
                                        hi += (T)1.0;
                                        break;

    case AluMod::ROUNDING_UPI:
    case AluMod::ROUNDING_UPI_SAT:
    case AluMod::ROUNDING_SUPI:
    case AluMod::ROUNDING_SUPI_SAT:
                                        break;

    default:
        assert(false);
        break;
    }

    dst[0] = lo - (T)1.0;
    dst[1] = Val(lo - (T)1.0).ulp(1);
    dst[2] = Val(lo).ulp(-1);
    dst[3] = lo;
    dst[4] = Val(lo).ulp(1);
    dst[5] = lo + (T)1.0;
    dst[6] = hi - (T)1.0;
    dst[7] = Val(hi).ulp(-1);
    dst[8] = hi;
    dst[9] = Val(hi).ulp(1);
    dst[10] = Val(hi + (T)1.0).ulp(-1);
    dst[11] = hi + (T)1.0;

}

static f16_t f16_rounding_tests[ROUNDING_TESTS_NUM];
static f32_t f32_rounding_tests[ROUNDING_TESTS_NUM];
static f64_t f64_rounding_tests[ROUNDING_TESTS_NUM];

const f16_t*  getF16RoundingTestsData(unsigned dstType, AluMod aluMod)
{
    makeRoundingTestsData(dstType, aluMod, f16_rounding_tests);
    return f16_rounding_tests;
}

const f32_t* getF32RoundingTestsData(unsigned dstType, AluMod aluMod)
{
    makeRoundingTestsData(dstType, aluMod, f32_rounding_tests);
    return f32_rounding_tests;
}

const f64_t* getF64RoundingTestsData(unsigned dstType, AluMod aluMod)
{
    makeRoundingTestsData(dstType, aluMod, f64_rounding_tests);
    return f64_rounding_tests;
}

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN
