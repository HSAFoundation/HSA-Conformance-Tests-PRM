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