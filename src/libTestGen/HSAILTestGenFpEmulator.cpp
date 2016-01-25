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

#include "HSAILTestGenFpEmulator.h"
#include "HSAILTestGenUtilities.h"
#include "HSAILTestGenVal.h"

#include <limits>
#include <cmath>

using std::string;
using std::ostringstream;

using namespace HSAIL_ASM;

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================

static double PI  = 3.1415926535897932384626433832795;
static double LN2 = 0.693147180559945309417;             // ln(2)

//==============================================================================
//==============================================================================
//==============================================================================
// Native Sin and Cos

/// HSAIL spec set no requirements for nsin/ncos WRT range of arguments and precision.
/// Actual traits of nsin/ncos depend on HSA JIT implementation.
/// The following values describe existing implementation and set boundaries for testing:
static const unsigned NSIN_NCOS_RESULT_PRECISION_ULPS = 8192 + 1;

/// Precision is guaranteed only for inputs within:
static const double NSIN_NCOS_ARG_MAX =  PI;
static const double NSIN_NCOS_ARG_MIN = -PI;

/// Precision is unspecified when argument or result is very close to the zero.
/// \note HSA JIT implementation details: These values ensure that denorms would
/// not appear at V_SIN/COS_F32 inputs and outputs.

#define IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(x) \
  ((std::numeric_limits<float>::min()*-2.0f*(float)PI) < (x) && (x) < (std::numeric_limits<float>::min()*2.0f*(float)PI) && (x) != 0.0F)

#define IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(x) (f32_t(x).isSubnormal())

/// [atamazov 24 Mar 2014] C/C++ cos(~(N+0.5)*PI) is too rough. For example,
/// HSAIL ncos(0.5*PI) returns 0, while C/C++ cos() returns something like 8e-08.
/// The deviation is hundreds thousands ULPs which is obviously too much.
/// The same problem occus with nsin(~N*PI). cos_precise_near_zero() and
/// sin_precise_near_zero() are attempts to fix the problem, which seem
/// acceptable for now.
static float cos_precise_near_zero(const float x)
{
    const float pi      = (float)PI;
    const float half_pi = (float)(0.5*PI);

    // find integer N for which N*PI < x <= (N+1)*PI

    const float x_offset = (x >=0) ? 0 : -pi;
    const int n = (int)((x + x_offset) / pi);

    // find error of regular cos() in the middle, at (N+0.5)*PI

    const float middle = n*pi + half_pi;
    const float errN = 0 - cos(middle);

    // linear error correction in the [N*PI, (N+1)*PI] range.
    // compensation should be 0.0 at the ends and errN in the middle.

    const float distance_from_the_middle = (x >= middle) ? (x - middle) : (middle - x);
    const float compensation = errN * (1 - distance_from_the_middle / (half_pi));

    return std::cos(x) + compensation;
}

static f32_t ncos_impl(const f32_t val, bool& isValidArg)
{
    isValidArg = true;
    if (Val(val).isNan()) { return val; }

    const float f = val.floatValue();
    if (f < NSIN_NCOS_ARG_MIN || NSIN_NCOS_ARG_MAX < f || IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(f))
    {
        isValidArg = false;
        return val;
    }

    const float cosine = cos_precise_near_zero(f);
    isValidArg = !IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(cosine);
    return f32_t(cosine);
}

static float sin_precise_near_zero(const float x)
{
    const float pi      = (float)PI;
    const float half_pi = (float)(0.5*PI);

    // find integer N for which (N-0.5)*PI < x <= (N+0.5)*PI

    const float x_offset = (x >=0) ? half_pi : -half_pi;
    const int n = (int)((x + x_offset) / pi); // rounded to nearest, e.g. (int)1.2*PI/PI == 1

    // find error of regular sin() in the middle, at N*PI

    const float middle = n*pi;
    const float errN = 0 - sin(middle);

    // linear error correction in the [(N-0.5)*PI, (N+0.5)*PI] range.
    // compensation should be 0.0 at the ends and errN in the middle.

    const float distance_from_the_middle = (x >= middle) ? (x - middle) : (middle - x);
    const float compensation = errN * (1 - distance_from_the_middle / (half_pi));

    return std::sin(x) + compensation;
}

static f32_t nsin_impl(const f32_t val, bool& isValidArg)
{
    isValidArg = true;
    if (Val(val).isNan()) return val;

    const float f = val.floatValue();
    if (f < NSIN_NCOS_ARG_MIN || NSIN_NCOS_ARG_MAX < f || IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(f))
    {
        isValidArg = false;
        return val;
    }

    const float sine = (float)sin_precise_near_zero(f);
    isValidArg = !IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(sine);
    return f32_t(sine);
}

//=============================================================================
//=============================================================================
//=============================================================================
// Fract, Ceil, Floor, Trunc, Rint

template<typename T> static T fract_impl(T val)
{
    if (Val(val).isNan()) return val; // preserve NaN payload
    
    if (Val(val).isPositiveInf() || Val(val).isPositiveZero()) return Val(val).getPositiveZero();
    if (Val(val).isNegativeInf() || Val(val).isNegativeZero()) return Val(val).getNegativeZero();
    
    T unused;
    const T res = val.modf(&unused);
    const T ONE(1.0);
    const T ZERO(0.0);
    
    if (val > ZERO) {
        return res;
    } else if (res == ZERO) {
        // map -0.0 at modf output to +0.0.
        // note that -0.0 _inputs_ are handled above.
        return ZERO;
    } else { // res < -0.0    
        T x = ONE + res;
        if (x < ONE) return x;
        // Fractional part is so small that (1 + res) have to be rounded to 1.
        // Return the largest representable number which is less than 1.0
        if (sizeof(T) == 4) return Val(BRIG_TYPE_F32, 0x3F7FFFFFULL);
        if (sizeof(T) == 8) return Val(BRIG_TYPE_F64, 0x3FEFFFFFFFFFFFFFULL);
        assert(false);
        return ZERO;
    }
}

template<> f16_t fract_impl<f16_t>(f16_t val)
{
    f32_t f = fract_impl(f32_t(val)); // leverage float version
    const f16_t max_less_1 = f16_t::fromRawBits(0x3bff); // largest representable f16 number which is less than 1.0
    if (f > f32_t(max_less_1)) return f16_t::fromRawBits(0x3bff); 
    return f16_t(f);
}

template<typename T> static T ceil_impl(T val)
{
    if (Val(val).isNan() || Val(val).isInf()) return val;

    T res;
    T fract = val.modf(&res);

    return (fract != T(0.0) && val >= T(0.0))? res + T(1.0) : res;
}

template<> f16_t ceil_impl<f16_t>(f16_t val) { return f16_t(ceil_impl(f32_t(val))); }

template<typename T> static T floor_impl(T val)
{
    if (Val(val).isNan() || Val(val).isInf()) return val;

    T res;
    T fract = val.modf(&res);

    return (fract != T(0.0) && val < T(0.0))? res - T(1.0) : res;
}

template<> f16_t floor_impl<f16_t>(f16_t val) { return f16_t(floor_impl(f32_t(val))); }

template<typename T> static T trunc_impl(T val)
{
    if (Val(val).isNan() || Val(val).isInf()) return val;

    T res;
    val.modf(&res);
    return res;
}

template<> f16_t trunc_impl<f16_t>(f16_t val) { return f16_t(trunc_impl(f32_t(val))); }

template<typename T> static T rint_impl(T val)
{
    if (Val(val).isNan() || Val(val).isInf() || Val(val).isZero()) return val;

    T res;
    T fract = (val.modf(&res)).abs();
    bool isEven = (static_cast<uint64_t>(res.floatValue()) & 1) == 0;

    if (fract < T(0.5) || (fract == T(0.5) && isEven)) fract = T(0.0);
    else fract = T((val < T(0.0)) ? -1.0 : 1.0);

    return (res + fract).copySign(val);
}

template<> f16_t rint_impl<f16_t>(f16_t val) { return f16_t(rint_impl(f32_t(val))); }

//==============================================================================
//==============================================================================
//==============================================================================
// Float to Integer Conversions

// Compute delta d for rounding of val so that (val+d)
// will be rounded to proper value when converted to integer
/// \todo Brain damage
static int f2i_round(Val val, unsigned rounding)
{
    assert(val.isFloat());
    assert(!val.isNan());
    static const u64_t FRACTIONAL_OF_0_5 = (u64_t(1) << (sizeof(u64_t)*8 - 1));

    int round = 0;

    switch (rounding)
    {
    case BRIG_ROUND_INTEGER_NEAR_EVEN:
    case BRIG_ROUND_INTEGER_NEAR_EVEN_SAT:
    case BRIG_ROUND_INTEGER_SIGNALING_NEAR_EVEN:
    case BRIG_ROUND_INTEGER_SIGNALING_NEAR_EVEN_SAT:
        if (val.isInf()) { // nop, keep inf as is
        }
        else if (val.getFractionalOfNormalized() > FRACTIONAL_OF_0_5)
        {   // Rounds to the nearest representable value
            round = val.isNegative() ? -1 : 1;
        }
        else if (val.getFractionalOfNormalized()  == FRACTIONAL_OF_0_5 &&
                 val.getFractionalOfNormalized(-1) > FRACTIONAL_OF_0_5)
        {   // If there is a tie, round to an even least significant digit
            round = val.isNegative() ? -1 : 1;
        }
        break;
    case BRIG_ROUND_INTEGER_ZERO:
    case BRIG_ROUND_INTEGER_ZERO_SAT:
    case BRIG_ROUND_INTEGER_SIGNALING_ZERO:
    case BRIG_ROUND_INTEGER_SIGNALING_ZERO_SAT:
        break;
    case BRIG_ROUND_INTEGER_PLUS_INFINITY:
    case BRIG_ROUND_INTEGER_PLUS_INFINITY_SAT:
    case BRIG_ROUND_INTEGER_SIGNALING_PLUS_INFINITY:
    case BRIG_ROUND_INTEGER_SIGNALING_PLUS_INFINITY_SAT:
        if (val.isRegularPositive() && !val.isIntegral()) round = 1;
        break;
    case BRIG_ROUND_INTEGER_MINUS_INFINITY:
    case BRIG_ROUND_INTEGER_MINUS_INFINITY_SAT:
    case BRIG_ROUND_INTEGER_SIGNALING_MINUS_INFINITY:
    case BRIG_ROUND_INTEGER_SIGNALING_MINUS_INFINITY_SAT:
        if (val.isRegularNegative() && !val.isIntegral()) round = -1;
        break;

    default:
        assert(false);
        return 0;
    }

    return round;
}

// Return true if integer part of val (i.e. val w/o fractional part)
// is within boundaries of the specified type.
// For example, -0,999 is within bounds of u8 [0..255]
template<typename T>
static bool checkTypeBoundaries(unsigned type, T val)
{
    assert(isIntType(type));

    return ((getTypeBoundary<T>(type, true)        <= val)  || // case a: boundary is too large for mantissa
            (getTypeBoundary<T>(type, true) - T(1.0) <  val)) && // case b: boundary is less than max mantissa, so take care of fractional part of val
           ((val <= getTypeBoundary<T>(type, false)       ) ||
            (val <  getTypeBoundary<T>(type, false) + T(1.0)));
}

template<typename T>
static u64_t f2i_impl(T val, unsigned dstType, unsigned intRounding, bool& isValid)
{
    assert(isIntType(dstType));

    isValid = true;
    if (Val(val).isNan()) 
    {
        isValid = isSatRounding(intRounding);
        return 0;
    }
    
    int round = f2i_round(val, intRounding);
    T res = round ? val + T((float)round) : val; // avoid adding 0.0 to Inf (uberparanoid ;)

    if (!checkTypeBoundaries(dstType, res))
    {
        isValid = isSatRounding(intRounding);
        return getIntBoundary(dstType, res <= T(0.0));
    }
    
    if (isSignedType(dstType)) return static_cast<s64_t>(res.floatValue());
    else                       return static_cast<u64_t>(res.floatValue());
}

//==============================================================================
//==============================================================================
//==============================================================================
// Traps for Unsupported Operations

f16_t f16_unsupported()
{ 
    assert(false); 
    return f16_t(); 
}
f32_t f32_unsupported()
{ 
    assert(false); 
    return f32_t(); 
}
f64_t f64_unsupported()
{ 
    assert(false); 
    return f64_t(); 
}

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Precision of Emulation

// Returns expected accuracy for an HSAIL instruction.
// If the value == 0, the precision is infinite (no deviation is allowed).
// The values in the (0,1) range specify relative precision.
// Values >= 1 denote precision in ULPS calculated as (value - 0.5), i.e. 1.0 means 0.5 ULPS.
//
// Accuracy of native ops depends on target HW!
double getNativeOpPrecision(unsigned opcode, unsigned type)
{
    switch(opcode)
    {
    case BRIG_OPCODE_NRCP:
    case BRIG_OPCODE_NSQRT:
    case BRIG_OPCODE_NRSQRT:
    case BRIG_OPCODE_NEXP2:
    case BRIG_OPCODE_NLOG2:
    case BRIG_OPCODE_NFMA:
        if (type == BRIG_TYPE_F16) return 0.04;
        if (type == BRIG_TYPE_F32) return 0.000005;  // Relative
        if (type == BRIG_TYPE_F64) return 0.00000002; // Relative
        break;

    case BRIG_OPCODE_NSIN:
    case BRIG_OPCODE_NCOS:
        return NSIN_NCOS_RESULT_PRECISION_ULPS;

    default:
        assert(false);
        break;
    }
    return 0;
}

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Supported Rounding Modes

bool isSupportedFpRounding(unsigned rounding) 
{ 
    switch(rounding)
    {
    case BRIG_ROUND_NONE:
    case BRIG_ROUND_FLOAT_NEAR_EVEN:
        return true;

    case BRIG_ROUND_FLOAT_ZERO:
    case BRIG_ROUND_FLOAT_PLUS_INFINITY:
    case BRIG_ROUND_FLOAT_MINUS_INFINITY:
        return false; // TODO

    default:
        return false;
    }
}

void validateFpRounding(unsigned rounding)
{
    assert(isSupportedFpRounding(rounding) && rounding != BRIG_ROUND_NONE);
}

void validateRoundingNone(unsigned rounding)
{
    assert(rounding == BRIG_ROUND_NONE);
}

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Conversions

// NB: signalign forms must be handled the same as non-signaling ones.
//     Exceptions handling is implemented elsewhere
u64_t emulate_f2i(f16_t val, unsigned dstType, unsigned intRounding, bool& isValid) { return f2i_impl(val, dstType, intRounding, isValid); }
u64_t emulate_f2i(f32_t val, unsigned dstType, unsigned intRounding, bool& isValid) { return f2i_impl(val, dstType, intRounding, isValid); }
u64_t emulate_f2i(f64_t val, unsigned dstType, unsigned intRounding, bool& isValid) { return f2i_impl(val, dstType, intRounding, isValid); }

f16_t emulate_f2f16(f16_t,     unsigned)          {                               return f16_unsupported(); }
f16_t emulate_f2f16(f32_t val, unsigned rounding) { validateFpRounding(rounding); return f16_t(val, rounding); }
f16_t emulate_f2f16(f64_t val, unsigned rounding) { validateFpRounding(rounding); return f16_t(val, rounding); }

f32_t emulate_f2f32(f16_t val, unsigned rounding) { validateRoundingNone(rounding); return f32_t(val); }
f32_t emulate_f2f32(f32_t,     unsigned)          {                                 return f32_unsupported(); }
f32_t emulate_f2f32(f64_t val, unsigned rounding) { validateFpRounding(rounding);   return f32_t(val, rounding); }

f64_t emulate_f2f64(f16_t val, unsigned rounding) { validateRoundingNone(rounding); return f64_t(val); }
f64_t emulate_f2f64(f32_t val, unsigned rounding) { validateRoundingNone(rounding); return f64_t(val); }
f64_t emulate_f2f64(f64_t,     unsigned)          {                                 return f64_unsupported(); }

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Comparisons

int emulate_cmp(f16_t val1, f16_t val2) { return val1 < val2 ? -1 : val1 > val2 ? 1 : 0; }
int emulate_cmp(f32_t val1, f32_t val2) { return val1 < val2 ? -1 : val1 > val2 ? 1 : 0; }
int emulate_cmp(f64_t val1, f64_t val2) { return val1 < val2 ? -1 : val1 > val2 ? 1 : 0; }

//==============================================================================
//==============================================================================
//==============================================================================
// Truncations

f16_t emulate_fract(f16_t val, unsigned rounding) { validateFpRounding(rounding); return fract_impl(val); } //TODO: add rounding support
f32_t emulate_fract(f32_t val, unsigned rounding) { validateFpRounding(rounding); return fract_impl(val); } //TODO: add rounding support
f64_t emulate_fract(f64_t val, unsigned rounding) { validateFpRounding(rounding); return fract_impl(val); } //TODO: add rounding support

f16_t emulate_ceil(f16_t val) { return ceil_impl(val); }
f32_t emulate_ceil(f32_t val) { return ceil_impl(val); }
f64_t emulate_ceil(f64_t val) { return ceil_impl(val); }

f16_t emulate_floor(f16_t val) { return floor_impl(val); }
f32_t emulate_floor(f32_t val) { return floor_impl(val); }
f64_t emulate_floor(f64_t val) { return floor_impl(val); }

f16_t emulate_trunc(f16_t val) { return trunc_impl(val); }
f32_t emulate_trunc(f32_t val) { return trunc_impl(val); }
f64_t emulate_trunc(f64_t val) { return trunc_impl(val); }

f16_t emulate_rint(f16_t val) { return rint_impl(val); }
f32_t emulate_rint(f32_t val) { return rint_impl(val); }
f64_t emulate_rint(f64_t val) { return rint_impl(val); }

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Bit Operations

f16_t emulate_cpsgn(f16_t val1, f16_t val2) { return Val(val1).copySign(Val(val2)); }
f32_t emulate_cpsgn(f32_t val1, f32_t val2) { return Val(val1).copySign(Val(val2)); }
f64_t emulate_cpsgn(f64_t val1, f64_t val2) { return Val(val1).copySign(Val(val2)); }

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Standard Arithmetic

f16_t emulate_abs(f16_t val) { return val.abs(); }
f32_t emulate_abs(f32_t val) { return val.abs(); }
f64_t emulate_abs(f64_t val) { return val.abs(); }

f16_t emulate_neg(f16_t val) { return val.neg(); }
f32_t emulate_neg(f32_t val) { return val.neg(); }
f64_t emulate_neg(f64_t val) { return val.neg(); }

f16_t emulate_add(f16_t val1, f16_t val2, unsigned rounding) { validateFpRounding(rounding); return add(val1, val2, rounding); }
f32_t emulate_add(f32_t val1, f32_t val2, unsigned rounding) { validateFpRounding(rounding); return add(val1, val2, rounding); }
f64_t emulate_add(f64_t val1, f64_t val2, unsigned rounding) { validateFpRounding(rounding); return add(val1, val2, rounding); }

f16_t emulate_sub(f16_t val1, f16_t val2, unsigned rounding) { validateFpRounding(rounding); return add(val1, val2.neg(), rounding); }
f32_t emulate_sub(f32_t val1, f32_t val2, unsigned rounding) { validateFpRounding(rounding); return add(val1, val2.neg(), rounding); }
f64_t emulate_sub(f64_t val1, f64_t val2, unsigned rounding) { validateFpRounding(rounding); return add(val1, val2.neg(), rounding); }

f16_t emulate_mul(f16_t val1, f16_t val2, unsigned rounding) { validateFpRounding(rounding); return mul(val1, val2, rounding); }
f32_t emulate_mul(f32_t val1, f32_t val2, unsigned rounding) { validateFpRounding(rounding); return mul(val1, val2, rounding); }
f64_t emulate_mul(f64_t val1, f64_t val2, unsigned rounding) { validateFpRounding(rounding); return mul(val1, val2, rounding); }

f16_t emulate_div(f16_t val1, f16_t val2, unsigned rounding) { validateFpRounding(rounding); return f16_t(f64_t(val1).floatValue() / f64_t(val2).floatValue(), rounding); }
f32_t emulate_div(f32_t val1, f32_t val2, unsigned rounding) { validateFpRounding(rounding); return val1.floatValue() / val2.floatValue(); }
f64_t emulate_div(f64_t val1, f64_t val2, unsigned rounding) { validateFpRounding(rounding); return val1.floatValue() / val2.floatValue(); }

template<typename T>
static inline
T emulate_max_(T val1, T val2)
{ return
    Val(val1).isNan()? val2
    : Val(val2).isNan()? val1
      : (val1.isZero() && val2.isZero() && (Val(val1).isPositive() || Val(val2).isPositive())) ? T()
        : std::max(val1, val2);
}
f16_t emulate_max(f16_t val1, f16_t val2) { return emulate_max_(val1, val2); }
f32_t emulate_max(f32_t val1, f32_t val2) { return emulate_max_(val1, val2); }
f64_t emulate_max(f64_t val1, f64_t val2) { return emulate_max_(val1, val2); }

template<typename T>
static inline
T emulate_min_(T val1, T val2)
{ return
    Val(val1).isNan()? val2
    : Val(val2).isNan()? val1
      : (val1.isZero() && val2.isZero() && (Val(val1).isNegative() || Val(val2).isNegative())) ? T().neg()
        : std::min(val1, val2);
}
f16_t emulate_min(f16_t val1, f16_t val2) { return emulate_min_(val1, val2); }
f32_t emulate_min(f32_t val1, f32_t val2) { return emulate_min_(val1, val2); }
f64_t emulate_min(f64_t val1, f64_t val2) { return emulate_min_(val1, val2); }


f16_t emulate_fma(f16_t val1, f16_t val2, f16_t val3, unsigned rounding) { validateFpRounding(rounding); return fma(val1, val2, val3, rounding); }
f32_t emulate_fma(f32_t val1, f32_t val2, f32_t val3, unsigned rounding) { validateFpRounding(rounding); return fma(val1, val2, val3, rounding); }
f64_t emulate_fma(f64_t val1, f64_t val2, f64_t val3, unsigned rounding) { validateFpRounding(rounding); return fma(val1, val2, val3, rounding); }

/// [HSA-PRM-1.02 5.12 Floating-Point Optimization Instruction]: <blockquote>
/// The computation must be performed using the semantic equivalent of one of
/// the following methods:
/// -  Single Round Method: fma_ftz_round_fLength dest, src0, src1, scr2;
/// -  Double Round Method: mul_ftz_round_fLength temp, src0, src1;
///                         add_ftz_round_fLength dest, temp, src2;
/// Where each instruction uses the same modifiers and type as the mad
/// instruction. No alternative method is allowed. The same method must
/// be used for all floating-point mad instructions on a specific kernel agent.
/// An HSA runtime query is available to determine
/// the method used on a kernel agent. </blockquote>
/// 
/// The quote above implies that some HSA_AGENT_INFO_ (to be specified in
/// HSA-Runtime) attribute shall report if SINGLE or DOUBLE round method is
/// used. For now, let's assume that the same method shall be used for ftz- and
/// non-ftz versions of mad_fxx.
/// 
/// \todo Rsa-Runtime spec, HSA_AGENT_INFO_ reporting mad_fxx rounding method:
/// Track runtime spec and and update implementation accordingly.
/// For now, let's assume that SINGLE ROUND is used.
f16_t emulate_mad(f16_t val1, f16_t val2, f16_t val3, unsigned rounding) { validateFpRounding(rounding); return fma(val1, val2, val3, rounding); }
f32_t emulate_mad(f32_t val1, f32_t val2, f32_t val3, unsigned rounding) { validateFpRounding(rounding); return fma(val1, val2, val3, rounding); }
f64_t emulate_mad(f64_t val1, f64_t val2, f64_t val3, unsigned rounding) { validateFpRounding(rounding); return fma(val1, val2, val3, rounding); }

f16_t emulate_sqrt(f16_t val, unsigned rounding)  { validateFpRounding(rounding); return f16_t(sqrt(f64_t(val).floatValue()), rounding); }
f32_t emulate_sqrt(f32_t val, unsigned rounding)  { validateFpRounding(rounding); return f32_t(sqrt(val.floatValue())); }
f64_t emulate_sqrt(f64_t val, unsigned rounding)  { validateFpRounding(rounding); return sqrt(val.floatValue()); }

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Native Arithmetic

f16_t emulate_nfma(f16_t val1, f16_t val2, f16_t val3) { return fma(val1, val2, val3, BRIG_ROUND_FLOAT_NEAR_EVEN); }
f32_t emulate_nfma(f32_t val1, f32_t val2, f32_t val3) { return fma(val1, val2, val3, BRIG_ROUND_FLOAT_NEAR_EVEN); }
f64_t emulate_nfma(f64_t val1, f64_t val2, f64_t val3) { return fma(val1, val2, val3, BRIG_ROUND_FLOAT_NEAR_EVEN); }

f16_t emulate_nsqrt(f16_t val)  { return f16_t(sqrt(f64_t(val).floatValue())); }
f32_t emulate_nsqrt(f32_t val)  { return f32_t(sqrt(val.floatValue())); }
f64_t emulate_nsqrt(f64_t val)  { return sqrt(val.floatValue()); }

f16_t emulate_nrsqrt(f16_t val) { return f16_t(1.0  / sqrt(f64_t(val).floatValue())); }
f32_t emulate_nrsqrt(f32_t val) { return f32_t(1.0 / sqrt(static_cast<double>(val.floatValue()))); }
f64_t emulate_nrsqrt(f64_t val) { return 1.0 / sqrt(val.floatValue()); }

f16_t emulate_nrcp(f16_t val)   { return f16_t(1.0 / f64_t(val).floatValue()); }
f32_t emulate_nrcp(f32_t val)   { return 1.0f / val.floatValue(); }
f64_t emulate_nrcp(f64_t val)   { return 1.0 / val.floatValue(); }

f16_t emulate_ncos(f16_t val, bool& isValidArg)  { return f16_unsupported(); }
f32_t emulate_ncos(f32_t val, bool& isValidArg)  { return ncos_impl(val, isValidArg); };
f64_t emulate_ncos(f64_t val, bool& isValidArg)  { return f64_unsupported(); }

f16_t emulate_nsin(f16_t val, bool& isValidArg)  { return f16_unsupported(); }
f32_t emulate_nsin(f32_t val, bool& isValidArg)  { return nsin_impl(val, isValidArg); };
f64_t emulate_nsin(f64_t val, bool& isValidArg)  { return f64_unsupported(); }

f16_t emulate_nexp2(f16_t val)  { return f16_unsupported(); }
f32_t emulate_nexp2(f32_t val)  { return f32_t(exp((val * static_cast<f32_t>(LN2)).floatValue())); }
f64_t emulate_nexp2(f64_t val)  { return f64_unsupported(); }

f16_t emulate_nlog2(f16_t val)  { return f16_unsupported(); }
f32_t emulate_nlog2(f32_t val)  { return f32_t(log(val.floatValue()) / static_cast<float>(LN2)); } 
f64_t emulate_nlog2(f64_t val)  { return f64_unsupported(); }

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN
