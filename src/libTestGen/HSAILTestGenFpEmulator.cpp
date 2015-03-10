
#include "HSAILTestGenFpEmulator.h"
#include "HSAILTestGenUtilities.h"
#include "HSAILTestGenVal.h"

using std::string;
using std::ostringstream;

using namespace HSAIL_ASM;
using Brig::BRIG_TYPE_F16;
using Brig::BRIG_TYPE_F32;
using Brig::BRIG_TYPE_F64;

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================

static f64_t PI  = 3.1415926535897932384626433832795;
static f64_t LN2 = 0.693147180559945309417;             // ln(2)

//=============================================================================
//=============================================================================
//=============================================================================

// Limits of normalized floats.
// Values between (FLT_MAX_NEG_NORM, 0) and (0,FLT_MIN_POS_NORM) are denormals.
static const unsigned FLT_MAX_NEG_NORM_BITS_ = 0x80800000;
static const unsigned FLT_MIN_POS_NORM_BITS_ = 0x00800000;
#define FLT_MAX_NEG_NORM (HEX2F32(FLT_MAX_NEG_NORM_BITS_))
#define FLT_MIN_POS_NORM (HEX2F32(FLT_MIN_POS_NORM_BITS_))
#define IS_FLT_DENORM(x) (FLT_MAX_NEG_NORM < (x) && (x) < FLT_MIN_POS_NORM && (x) != 0.0F)

//==============================================================================
//==============================================================================
//==============================================================================
// Native Sin and Cos

/// HSAIL spec set no requirements for nsin/ncos WRT range of arguments and precision.
/// Actual traits of nsin/ncos depend on HSA JIT implementation.
/// The following values describe existing implementation and set boundaries for testing:
static const unsigned NSIN_NCOS_RESULT_PRECISION_ULPS = 8192 + 1;

/// Precision is guaranteed only for inputs within:
static const f64_t NSIN_NCOS_ARG_MAX =  PI;
static const f64_t NSIN_NCOS_ARG_MIN = -PI;

/// Precision is unspecified when argument or result is very close to the zero.
/// \note HSA JIT implementation details: These values ensure that denorms would
/// not appear at V_SIN/COS_F32 inputs and outputs.

#define IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(x) \
  ((FLT_MAX_NEG_NORM*2*(float)PI) < (x) && (x) < (FLT_MIN_POS_NORM*2*(float)PI) && (x) != 0.0F)

#define IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(x) \
  (IS_FLT_DENORM(x))

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

    return cos(x) + compensation;
}

static float ncos_impl(const float val, bool& isValidArg)
{
    isValidArg = true;

    if (Val(val).isNan()) return Val(val);
    if (val < NSIN_NCOS_ARG_MIN || NSIN_NCOS_ARG_MAX < val || IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(val)) 
    {
        isValidArg = false;
        return val;
    }

    const float cosine = cos_precise_near_zero(val);
    isValidArg = !IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(cosine);

    return Val(cosine);
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

    return sin(x) + compensation;
}

static float nsin_impl(const float val, bool& isValidArg)
{
    isValidArg = true;

    if (Val(val).isNan()) return val;
    if (val < NSIN_NCOS_ARG_MIN || NSIN_NCOS_ARG_MAX < val || IS_NSIN_NCOS_ARG_TOO_CLOSE_TO_ZERO(val))
    {
        isValidArg = false;
        return val;
    }

    const float sine = (float)sin_precise_near_zero(val);
    isValidArg = !IS_NSIN_NCOS_RESULT_TOO_CLOSE_TO_ZERO(sine);

    return sine;
}

//=============================================================================
//=============================================================================
//=============================================================================
// Fract, Ceil, Floor, Trunc, Rint

// Handling of infinity is not specified by HSAIL but required by OpenCL 2.0.
// See bug 10282
//  - must return POS_ZERO when src is POS_INF
//  - must return NEG_ZERO when src is NEG_INF
template<typename T> static T fract_impl(T val)
{
    if (Val(val).isNan()) return val; // preserve NaN payload
    
    if (Val(val).isPositiveInf()) return Val(val).getPositiveZero();
    if (Val(val).isNegativeInf()) return Val(val).getNegativeZero();
    
    T unused;
    T res = std::modf(val, &unused);
    T one = 1; // NB: avoid casting to double
    
    if (val > 0)       return Val(res);
    if (res == (T)0)   return Val((T)0); // NB: res may be -0!
    
    T x = one + res;
    if (x < one) return Val(x);
    
    // Fractional part is so small that (1 + res) have to be rounded to 1.
    // Return the largest representable number which is less than 1.0
    if (sizeof(T) == 4) return Val(BRIG_TYPE_F32, 0x3F7FFFFFULL);
    if (sizeof(T) == 8) return Val(BRIG_TYPE_F64, 0x3FEFFFFFFFFFFFFFULL);
    
    assert(false);
    return 0;
}

template<typename T> static T ceil_impl(T val)
{
    if (Val(val).isNan() || Val(val).isInf()) return val;

    T res;
    T fract = std::modf(val, &res);

    return (fract != 0 && val >= 0)? res + 1 : res;
}

template<typename T> static T floor_impl(T val)
{
    if (Val(val).isNan() || Val(val).isInf()) return val;

    T res;
    T fract = std::modf(val, &res);

    return (fract != 0 && val < 0)? res - 1 : res;
}

template<typename T> static T trunc_impl(T val)
{
    if (Val(val).isNan() || Val(val).isInf()) return val;

    T res;
    std::modf(val, &res);
    return res;
}

template<typename T> static T rint_impl(T val)
{
    if (Val(val).isNan() || Val(val).isInf()) return val;

    T res;
    T fract = std::abs(std::modf(val, &res));
    bool isEven = (static_cast<uint64_t>(res) & 1) == 0;

    if (fract < 0.5 || (fract == 0.5 && isEven)) fract = 0;
    else fract = static_cast<T>((val < 0)? -1 : 1);

    return res + fract;
}

//==============================================================================
//==============================================================================
//==============================================================================
// Float to Integer Conversions

// Compute delta d for rounding of val so that (val+d)
// will be rounded to proper value when converted to integer
static int f2i_round(Val val, unsigned rounding)
{
    assert(val.isFloat());
    assert(!val.isNan());

    int round = 0;

    switch (rounding)
    {
    case Brig::BRIG_ROUND_INTEGER_NEAR_EVEN:
    case Brig::BRIG_ROUND_INTEGER_NEAR_EVEN_SAT:
    case Brig::BRIG_ROUND_INTEGER_SIGNALING_NEAR_EVEN:
    case Brig::BRIG_ROUND_INTEGER_SIGNALING_NEAR_EVEN_SAT:
        if (val.getNormalizedFract() > Val(0.5f).getNormalizedFract())          // Rounds to the nearest representable value
        {
            round = val.isNegative() ? -1 : 1;
        }
        else if (val.getNormalizedFract()  == Val(0.5f).getNormalizedFract() &&  // If there is a tie, round to an even least significant digit
                 val.getNormalizedFract(-1) > Val(0.5f).getNormalizedFract())
        {
            round = val.isNegative() ? -1 : 1;
        }
        break;
    case Brig::BRIG_ROUND_INTEGER_ZERO:
    case Brig::BRIG_ROUND_INTEGER_ZERO_SAT:
    case Brig::BRIG_ROUND_INTEGER_SIGNALING_ZERO:
    case Brig::BRIG_ROUND_INTEGER_SIGNALING_ZERO_SAT:
        break;
    case Brig::BRIG_ROUND_INTEGER_PLUS_INFINITY:
    case Brig::BRIG_ROUND_INTEGER_PLUS_INFINITY_SAT:
    case Brig::BRIG_ROUND_INTEGER_SIGNALING_PLUS_INFINITY:
    case Brig::BRIG_ROUND_INTEGER_SIGNALING_PLUS_INFINITY_SAT:
        if (val.isRegularPositive() && !val.isNatural()) round = 1;
        break;
    case Brig::BRIG_ROUND_INTEGER_MINUS_INFINITY:
    case Brig::BRIG_ROUND_INTEGER_MINUS_INFINITY_SAT:
    case Brig::BRIG_ROUND_INTEGER_SIGNALING_MINUS_INFINITY:
    case Brig::BRIG_ROUND_INTEGER_SIGNALING_MINUS_INFINITY_SAT:
        if (val.isRegularNegative() && !val.isNatural()) round = -1;
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
            (getTypeBoundary<T>(type, true) - (T)1 <  val)) && // case b: boundary is less than max mantissa, so take care of fractional part of val
           ((val <= getTypeBoundary<T>(type, false)       ) ||
            (val <  getTypeBoundary<T>(type, false) + (T)1));
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
    T res = val + (T)round;

    if (!checkTypeBoundaries(dstType, res))
    {
        isValid = isSatRounding(intRounding);
        return getIntBoundary(dstType, res <= (T)0);
    }
    
    if (isSignedType(dstType)) return static_cast<s64_t>((f64_t)res);
    else                       return static_cast<u64_t>((f64_t)res);
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

// Return precision of result computation for this instruction.
// If the value is 0, the precision is infinite.
// If the value is between 0 and 1, the precision is relative.
// If the value is greater than or equals to 1, the precision is specified in ULPS.
// This is a property of target HW, not emulator!
double getNativeOpPrecision(unsigned opcode, unsigned type)
{
    using namespace Brig;

    switch(opcode)
    {
    case BRIG_OPCODE_NRCP:
    case BRIG_OPCODE_NSQRT:
    case BRIG_OPCODE_NRSQRT:
    case BRIG_OPCODE_NEXP2:
    case BRIG_OPCODE_NLOG2:
        if (type == BRIG_TYPE_F16) return 0.0000005;  // TODO
        if (type == BRIG_TYPE_F32) return 0.0000005;  // Relative
        if (type == BRIG_TYPE_F64) return 0.00000002; // Relative
        break;

    case BRIG_OPCODE_NSIN:
    case BRIG_OPCODE_NCOS:
        return NSIN_NCOS_RESULT_PRECISION_ULPS;

    case BRIG_OPCODE_NFMA:
        return 1; // TODO

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
    using namespace Brig;

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
    assert(isSupportedFpRounding(rounding) && rounding != Brig::BRIG_ROUND_NONE);
}

void validateRoundingNone(unsigned rounding)
{
    assert(rounding == Brig::BRIG_ROUND_NONE);
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
f16_t emulate_f2f16(f32_t val, unsigned rounding) { validateFpRounding(rounding); return f16_todo(); }
f16_t emulate_f2f16(f64_t val, unsigned rounding) { validateFpRounding(rounding); return f16_t(val, rounding); }

f32_t emulate_f2f32(f16_t val, unsigned rounding) { validateRoundingNone(rounding); f16_todo(); return 0.0f; }
f32_t emulate_f2f32(f32_t,     unsigned)          {                                 return f32_unsupported(); }
f32_t emulate_f2f32(f64_t val, unsigned rounding) { validateFpRounding(rounding);   return static_cast<f32_t>(val); }

f64_t emulate_f2f64(f16_t val, unsigned rounding) { validateRoundingNone(rounding); return val.f64(); }
f64_t emulate_f2f64(f32_t val, unsigned rounding) { validateRoundingNone(rounding); return static_cast<f64_t>(val); }
f64_t emulate_f2f64(f64_t,     unsigned)          {                                 return f64_unsupported(); }

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Comparisons

int emulate_cmp(f16_t val1, f16_t val2) { return val1.f64() < val2.f64() ? -1 : val1.f64() > val2.f64() ? 1 : 0; }
int emulate_cmp(f32_t val1, f32_t val2) { return val1 < val2 ? -1 : val1 > val2 ? 1 : 0; }
int emulate_cmp(f64_t val1, f64_t val2) { return val1 < val2 ? -1 : val1 > val2 ? 1 : 0; }

//==============================================================================
//==============================================================================
//==============================================================================
// Truncations

f16_t emulate_fract(f16_t val) { return f16_todo(); }
f32_t emulate_fract(f32_t val) { return fract_impl(val); }
f64_t emulate_fract(f64_t val) { return fract_impl(val); }

f16_t emulate_ceil(f16_t val) { return f16_todo(); }
f32_t emulate_ceil(f32_t val) { return ceil_impl(val); }
f64_t emulate_ceil(f64_t val) { return ceil_impl(val); }

f16_t emulate_floor(f16_t val) { return f16_todo(); }
f32_t emulate_floor(f32_t val) { return floor_impl(val); }
f64_t emulate_floor(f64_t val) { return floor_impl(val); }

f16_t emulate_trunc(f16_t val) { return f16_todo(); }
f32_t emulate_trunc(f32_t val) { return trunc_impl(val); }
f64_t emulate_trunc(f64_t val) { return trunc_impl(val); }

f16_t emulate_rint(f16_t val) { return f16_todo(); }
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

f16_t emulate_abs(f16_t val) { return f16_t(std::abs(val.f64())); }
f32_t emulate_abs(f32_t val) { return std::abs(val); }
f64_t emulate_abs(f64_t val) { return std::abs(val); }

f16_t emulate_neg(f16_t val) { return f16_t(val.neg()); }
f32_t emulate_neg(f32_t val) { return -val; }
f64_t emulate_neg(f64_t val) { return -val; }

f16_t emulate_add(f16_t val1, f16_t val2, unsigned rounding) { validateFpRounding(rounding); return f16_t(val1.f64() + val2.f64(), rounding); }
f32_t emulate_add(f32_t val1, f32_t val2, unsigned rounding) { validateFpRounding(rounding); return val1 + val2; }
f64_t emulate_add(f64_t val1, f64_t val2, unsigned rounding) { validateFpRounding(rounding); return val1 + val2; }

f16_t emulate_sub(f16_t val1, f16_t val2, unsigned rounding) { validateFpRounding(rounding); return f16_t(val1.f64() - val2.f64(), rounding); }
f32_t emulate_sub(f32_t val1, f32_t val2, unsigned rounding) { validateFpRounding(rounding); return val1 - val2; }
f64_t emulate_sub(f64_t val1, f64_t val2, unsigned rounding) { validateFpRounding(rounding); return val1 - val2; }

f16_t emulate_mul(f16_t val1, f16_t val2, unsigned rounding) { validateFpRounding(rounding); return f16_t(val1.f64() * val2.f64(), rounding); }
f32_t emulate_mul(f32_t val1, f32_t val2, unsigned rounding) { validateFpRounding(rounding); return val1 * val2; }
f64_t emulate_mul(f64_t val1, f64_t val2, unsigned rounding) { validateFpRounding(rounding); return val1 * val2; }

f16_t emulate_div(f16_t val1, f16_t val2, unsigned rounding) { validateFpRounding(rounding); return f16_t(val1.f64() / val2.f64(), rounding); }
f32_t emulate_div(f32_t val1, f32_t val2, unsigned rounding) { validateFpRounding(rounding); return val1 / val2; }
f64_t emulate_div(f64_t val1, f64_t val2, unsigned rounding) { validateFpRounding(rounding); return val1 / val2; }

f16_t emulate_max(f16_t val1, f16_t val2) { return Val(val1).isNan()? val2 : Val(val2).isNan()? val1 : f16_t(std::max(val1.f64(), val2.f64())); }
f32_t emulate_max(f32_t val1, f32_t val2) { return Val(val1).isNan()? val2 : Val(val2).isNan()? val1 : std::max(val1, val2); }
f64_t emulate_max(f64_t val1, f64_t val2) { return Val(val1).isNan()? val2 : Val(val2).isNan()? val1 : std::max(val1, val2); }

f16_t emulate_min(f16_t val1, f16_t val2) { return Val(val1).isNan()? val2 : Val(val2).isNan()? val1 : f16_t(std::min(val1.f64(), val2.f64())); }
f32_t emulate_min(f32_t val1, f32_t val2) { return Val(val1).isNan()? val2 : Val(val2).isNan()? val1 : std::min(val1, val2); }
f64_t emulate_min(f64_t val1, f64_t val2) { return Val(val1).isNan()? val2 : Val(val2).isNan()? val1 : std::min(val1, val2); }

f16_t emulate_fma(f16_t val1, f16_t val2, f16_t val3, unsigned rounding) { validateFpRounding(rounding); return f16_t(val1.f64() * val2.f64() + val3.f64(), rounding); }
f32_t emulate_fma(f32_t val1, f32_t val2, f32_t val3, unsigned rounding) { validateFpRounding(rounding); return val1 * val2 + val3; } //TODO: not enough precision
f64_t emulate_fma(f64_t val1, f64_t val2, f64_t val3, unsigned rounding) { validateFpRounding(rounding); return val1 * val2 + val3; } //TODO: not enough precision

f16_t emulate_sqrt(f16_t val, unsigned rounding)  { validateFpRounding(rounding); return f16_t(sqrt(val.f64()), rounding); }
f32_t emulate_sqrt(f32_t val, unsigned rounding)  { validateFpRounding(rounding); return sqrt(val); }
f64_t emulate_sqrt(f64_t val, unsigned rounding)  { validateFpRounding(rounding); return sqrt(val); }

//==============================================================================
//==============================================================================
//==============================================================================
// HSAIL Floating-Point Library: Native Arithmetic

f16_t emulate_nfma(f16_t val1, f16_t val2, f16_t val3) { return f16_t(val1.f64() * val2.f64() + val3.f64()); }
f32_t emulate_nfma(f32_t val1, f32_t val2, f32_t val3) { return val1 * val2 + val3; }
f64_t emulate_nfma(f64_t val1, f64_t val2, f64_t val3) { return val1 * val2 + val3; }

f16_t emulate_nsqrt(f16_t val)  { return f16_t(sqrt(val.f64())); }
f32_t emulate_nsqrt(f32_t val)  { return sqrt(val); }
f64_t emulate_nsqrt(f64_t val)  { return sqrt(val); }

f16_t emulate_nrsqrt(f16_t val) { return f16_t(1.0  / sqrt(val.f64())); }
f32_t emulate_nrsqrt(f32_t val) { return 1.0f / sqrt(val); }
f64_t emulate_nrsqrt(f64_t val) { return 1.0  / sqrt(val); }

f16_t emulate_nrcp(f16_t val)   { return f16_t(static_cast<f64_t>(1.0) / val.f64()); }
f32_t emulate_nrcp(f32_t val)   { return static_cast<f32_t>(1.0) / val; }
f64_t emulate_nrcp(f64_t val)   { return static_cast<f64_t>(1.0) / val; }

f16_t emulate_ncos(f16_t val, bool& isValidArg)  { return f16_unsupported(); }
f32_t emulate_ncos(f32_t val, bool& isValidArg)  { return ncos_impl(val, isValidArg); };
f64_t emulate_ncos(f64_t val, bool& isValidArg)  { return f64_unsupported(); }

f16_t emulate_nsin(f16_t val, bool& isValidArg)  { return f16_unsupported(); }
f32_t emulate_nsin(f32_t val, bool& isValidArg)  { return nsin_impl(val, isValidArg); };
f64_t emulate_nsin(f64_t val, bool& isValidArg)  { return f64_unsupported(); }

f16_t emulate_nexp2(f16_t val)  { return f16_unsupported(); }
f32_t emulate_nexp2(f32_t val)  { return exp(val * static_cast<f32_t>(LN2)); }
f64_t emulate_nexp2(f64_t val)  { return f64_unsupported(); }

f16_t emulate_nlog2(f16_t val)  { return f16_unsupported(); }
f32_t emulate_nlog2(f32_t val)  { return log(val) / static_cast<f32_t>(LN2); } 
f64_t emulate_nlog2(f64_t val)  { return f64_unsupported(); }

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN
