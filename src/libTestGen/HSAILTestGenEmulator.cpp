//===-- HSAILTestGenEmulator.cpp - HSAIL Instructions Emulator ------------===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#include "HSAILTestGenEmulator.h"
#include "HSAILTestGenUtilities.h"

#include <cmath>
#include <limits>

using std::numeric_limits;

using Brig::BRIG_TYPE_B1;
using Brig::BRIG_TYPE_B8;
using Brig::BRIG_TYPE_B32;
using Brig::BRIG_TYPE_B64;
using Brig::BRIG_TYPE_B128;
using Brig::BRIG_TYPE_S8;
using Brig::BRIG_TYPE_S16;
using Brig::BRIG_TYPE_S32;
using Brig::BRIG_TYPE_S64;
using Brig::BRIG_TYPE_U8;
using Brig::BRIG_TYPE_U16;
using Brig::BRIG_TYPE_U32;
using Brig::BRIG_TYPE_U64;
using Brig::BRIG_TYPE_F16;
using Brig::BRIG_TYPE_F32;
using Brig::BRIG_TYPE_F64;
using Brig::BRIG_TYPE_U8X4;
using Brig::BRIG_TYPE_U16X2;

using HSAIL_ASM::InstBasic;
using HSAIL_ASM::InstSourceType;
using HSAIL_ASM::InstAtomic;
using HSAIL_ASM::InstCmp;
using HSAIL_ASM::InstCvt;
using HSAIL_ASM::InstImage;
using HSAIL_ASM::InstMem;
using HSAIL_ASM::InstMod;
using HSAIL_ASM::InstBr;

using HSAIL_ASM::OperandOperandList;

using HSAIL_ASM::isBitType;
using HSAIL_ASM::isFloatType;
using HSAIL_ASM::isPackedType;
using HSAIL_ASM::isSignedType;
using HSAIL_ASM::isUnsignedType;
using HSAIL_ASM::isFloatPackedType;
using HSAIL_ASM::isIntType;
using HSAIL_ASM::getBrigTypeNumBits;
using HSAIL_ASM::getPacking;
using HSAIL_ASM::isSatPacking;
using HSAIL_ASM::getPackedDstDim;
using HSAIL_ASM::getPackedTypeDim;
using HSAIL_ASM::packedType2elementType;
using HSAIL_ASM::packedType2baseType;
using HSAIL_ASM::isSignalingRounding;
using HSAIL_ASM::isSatRounding;

//=================================================================================================
//=================================================================================================
//=======================      HOW TO ADD NEW INSTRUCTIONS TO EMULATOR      =======================
//=================================================================================================
//=================================================================================================
//
// Take a look at emulator interface defined in HSAILTestGenEmulator.h
// Each of these interface functions should be extended accordingly.
// The following notes describe how to implement emulation of a new instruction X.
//
// 1. Define an emulator function template with semantics appropriate for X. ALL EMULATOR FUNCTIONS 
//    MUST TAKE VALUES OF THE SAME TYPE. This rule is enforced to minimize the number of function 
//    selectors (they are described below). If the instruction being emulated has different types 
//    of its arguments, some additional code is required (see discussion of special cases below). 
//    The type of returned value MUST match the type of result expected by instruction.
//
//    An example:
//
//          struct op_add {
//              template<typename T> T ix(T val1, T val2) { return val1 + val2; }
//          };
//
// 2. Choose an appropriate function selector. Function selectors convert abstract source values
//    to specific type and invoke corresponding instantiation of function template. There are many kinds
//    of selectors which differ in number of arguments and types.
//    For example:
//
//          emulateUnrOpB       - for b 1/32/64 unary instructions
//          emulateBinOpB       - for b 1/32/64 binary instructions
//          emulateTrnOpB       - for b 1/32/64 ternary instructions
//
//    Supported selector types must include all types accepted by instruction being emulated,
//    BUT MAY BE WIDER. Paths for types not supported by instruction will never be taken.
//    BEFORE WRITING A NEW SELECTOR, TRY USING EXISTING ONES WITH WIDER LIST OF SUPPORTED TYPES.
//
//    For example, emulateBinOpBSUF is a suitable selector for instruction "add", though this
//    instruction does not work with 'b' types.
//
// 3. Invoke function selector passing it the type of source values together with source values
//    and the function. For example:
//
//          static Val emulate_add(unsigned opcode, unsigned type, Val arg1, Val arg2)
//          {
//              return emulateBinOpBSUF(type, arg1, arg2, op_add());
//          }
//
//=================================================================================================
//
// SPECIAL CASES
//
// 1. Emulator function templates must take care of special values such as NaN if these values are 
//    not handled by the runtime. For example:
//
//          struct op_max {
//              template<typename T>
//              T fx(T val1, T val2) {
//                  return Val(val1).isNan()? val1 : Val(val2).isNan()? val2 : std::max(val1, val2);
//              }
//          };
//
// 2. Some instructions have unspecified behaviour for special values.
//    For example, ncos(x) has unspecified precision for values out of the range
//    [NSIN_NCOS_ARG_MIN, NSIN_NCOS_ARG_MAX]. In this case emulation function 
//    should have return type Val and return undefValue. An example:
//
//          struct op_cos {
//              template<typename T>
//              Val fx(T val) {
//                  return Val(val).isNan()?
//                              Val(val) :
//                              (val < NSIN_NCOS_ARG_MIN || NSIN_NCOS_ARG_MAX < val)? undefValue() : Val(cos(val));
//              }
//          };
//
// 3. Some instructions have several source arguments with different types.
//    To emulate these instructions, there are several possibilities:
//
//      a) Convert all arguments to some generic type before passing values to selector.
//
//      b) Implement emulation of this instruction w/o emulation function templates and selectors.
//         Appropriate for very irregular instructions.
//
//      c) Define a non-standard function template selector. Appropriate for very irregular instructions.
//
//=================================================================================================

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================

#define FX_RND_UNR(impl) template<typename T> Val fx(T val,                  unsigned rounding) { return Val(emulate_##impl(val,              rounding)); }
#define FX_RND_BIN(impl) template<typename T> Val fx(T val1, T val2,         unsigned rounding) { return Val(emulate_##impl(val1, val2,       rounding)); }
#define FX_RND_TRN(impl) template<typename T> Val fx(T val1, T val2, T val3, unsigned rounding) { return Val(emulate_##impl(val1, val2, val3, rounding)); }

#define FX_UNR(impl)     template<typename T> Val fx(T val,                  unsigned rounding) { return Val(emulate_##impl(val             )); }
#define FX_BIN(impl)     template<typename T> Val fx(T val1, T val2,         unsigned rounding) { return Val(emulate_##impl(val1, val2      )); }
#define FX_TRN(impl)     template<typename T> Val fx(T val1, T val2, T val3, unsigned rounding) { return Val(emulate_##impl(val1, val2, val3)); }

#define FX_CHK_UNR(impl) template<typename T> Val fx(T val,                  unsigned rounding) { bool ok; T res = emulate_##impl(val, ok); return ok? Val(res) : undefValue(); }

#define IX_UNR           template<typename T> T   ix(T val)
#define IX_BIN           template<typename T> T   ix(T val1, T val2)
#define IX_TNR           template<typename T> T   ix(T val1, T val2, T val3)

#define IX_BIN_VAL       template<typename T> Val ix(T val1, T val2)
#define IX_BIN_INT       template<typename T> int ix(T val1, T val2)

#define IU_BIN           template<typename T> T   ix(T val1, unsigned val2)

//=============================================================================
//=============================================================================
//=============================================================================

static Val emulationFailed()                    // This should not happen
{
    assert(false);
    return Val();
}

static Val undefValue()    { return Val(); }    // Value returned for unspecified behaviour
static Val unimplemented() { return Val(); }    // Value returned for unimplemented features
static Val emptyDstValue() { return Val(); }    // Value returned when instruction has no destination register(s)
static Val emptyMemValue() { return Val(); }    // Value returned when instruction does not affect memory

//=============================================================================
//=============================================================================
//=============================================================================
// Template Selectors for Unary Ops

template<class T>
static Val emulateUnrOpUS_Rnd(unsigned type, unsigned rounding, Val arg, T op)
{
    assert(arg.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_U8:  return op.fx(arg.u8(),  rounding);
    case BRIG_TYPE_S8:  return op.fx(arg.s8(),  rounding);
    case BRIG_TYPE_U16: return op.fx(arg.u16(), rounding);
    case BRIG_TYPE_S16: return op.fx(arg.s16(), rounding);
    case BRIG_TYPE_U32: return op.fx(arg.u32(), rounding);
    case BRIG_TYPE_S32: return op.fx(arg.s32(), rounding);
    case BRIG_TYPE_U64: return op.fx(arg.u64(), rounding);
    case BRIG_TYPE_S64: return op.fx(arg.s64(), rounding);

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateUnrOpSF(unsigned type, unsigned rounding, Val arg, T op)
{
    assert(arg.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_S32: return op.ix(arg.s32());
    case BRIG_TYPE_S64: return op.ix(arg.s64());

    case BRIG_TYPE_F16: return op.fx(arg.f16(), rounding);
    case BRIG_TYPE_F32: return op.fx(arg.f32(), rounding);
    case BRIG_TYPE_F64: return op.fx(arg.f64(), rounding);

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateUnrOpF(unsigned type, unsigned rounding, Val arg, T op)
{
    assert(arg.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_F16: return op.fx(arg.f16(), rounding);
    case BRIG_TYPE_F32: return op.fx(arg.f32(), rounding);
    case BRIG_TYPE_F64: return op.fx(arg.f64(), rounding);

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateUnrOpB(unsigned type, Val arg, T op)
{
    assert(arg.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_B1:  return op.ix(arg.b1());
    case BRIG_TYPE_B32: return op.ix(arg.b32());
    case BRIG_TYPE_B64: return op.ix(arg.b64());

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Template Selectors for Binary Ops

template<class T>
static Val emulateBinOpBSUF(unsigned type, unsigned rounding, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_B1:  return op.ix(arg1.b1(),  arg2.b1());
    case BRIG_TYPE_B32: return op.ix(arg1.b32(), arg2.b32());
    case BRIG_TYPE_B64: return op.ix(arg1.b64(), arg2.b64());

    case BRIG_TYPE_S32: return op.ix(arg1.s32(), arg2.s32());
    case BRIG_TYPE_S64: return op.ix(arg1.s64(), arg2.s64());

    case BRIG_TYPE_U32: return op.ix(arg1.u32(), arg2.u32());
    case BRIG_TYPE_U64: return op.ix(arg1.u64(), arg2.u64());

    case BRIG_TYPE_F16: return op.fx(arg1.f16(), arg2.f16(), rounding);
    case BRIG_TYPE_F32: return op.fx(arg1.f32(), arg2.f32(), rounding);
    case BRIG_TYPE_F64: return op.fx(arg1.f64(), arg2.f64(), rounding);

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpF(unsigned type, unsigned rounding, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_F16: return op.fx(arg1.f16(), arg2.f16(), rounding);
    case BRIG_TYPE_F32: return op.fx(arg1.f32(), arg2.f32(), rounding);
    case BRIG_TYPE_F64: return op.fx(arg1.f64(), arg2.f64(), rounding);

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpB(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_B1:  return op.ix(arg1.b1(),  arg2.b1());
    case BRIG_TYPE_B32: return op.ix(arg1.b32(), arg2.b32());
    case BRIG_TYPE_B64: return op.ix(arg1.b64(), arg2.b64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpSU(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_S32: return op.ix(arg1.s32(), arg2.s32());
    case BRIG_TYPE_U32: return op.ix(arg1.u32(), arg2.u32());

    case BRIG_TYPE_S64: return op.ix(arg1.s64(), arg2.s64());
    case BRIG_TYPE_U64: return op.ix(arg1.u64(), arg2.u64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpSU_U32(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == BRIG_TYPE_U32);

    switch (type)
    {
    case BRIG_TYPE_S32: return op.ix(arg1.s32(), arg2.u32());
    case BRIG_TYPE_U32: return op.ix(arg1.u32(), arg2.u32());

    case BRIG_TYPE_S64: return op.ix(arg1.s64(), arg2.u32());
    case BRIG_TYPE_U64: return op.ix(arg1.u64(), arg2.u32());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateBinOpSat(unsigned elementType, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == elementType);
    assert(arg2.getType() == elementType);

    switch (elementType)
    {
    case BRIG_TYPE_S8:  return op.ix(elementType, arg1.s8(), arg2.s8());
    case BRIG_TYPE_U8:  return op.ix(elementType, arg1.u8(), arg2.u8());

    case BRIG_TYPE_S16: return op.ix(elementType, arg1.s16(), arg2.s16());
    case BRIG_TYPE_U16: return op.ix(elementType, arg1.u16(), arg2.u16());

    case BRIG_TYPE_S32: return op.ix(elementType, arg1.s32(), arg2.s32());
    case BRIG_TYPE_U32: return op.ix(elementType, arg1.u32(), arg2.u32());

    case BRIG_TYPE_S64: return op.ix(elementType, arg1.s64(), arg2.s64());
    case BRIG_TYPE_U64: return op.ix(elementType, arg1.u64(), arg2.u64());

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Template Selectors for Ternary Ops

template<class T>
static Val emulateTrnOpF(unsigned type, unsigned rounding, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_F16: return op.fx(arg1.f16(), arg2.f16(), arg3.f16(), rounding);
    case BRIG_TYPE_F32: return op.fx(arg1.f32(), arg2.f32(), arg3.f32(), rounding);
    case BRIG_TYPE_F64: return op.fx(arg1.f64(), arg2.f64(), arg3.f64(), rounding);

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateTrnOpSU(unsigned type, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_S32: return op.ix(arg1.s32(), arg2.s32(), arg3.s32());
    case BRIG_TYPE_S64: return op.ix(arg1.s64(), arg2.s64(), arg3.s64());

    case BRIG_TYPE_U32: return op.ix(arg1.u32(), arg2.u32(), arg3.u32());
    case BRIG_TYPE_U64: return op.ix(arg1.u64(), arg2.u64(), arg3.u64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateTrnOpSUF(unsigned type, unsigned rounding, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_S32: return op.ix(arg1.s32(), arg2.s32(), arg3.s32());
    case BRIG_TYPE_S64: return op.ix(arg1.s64(), arg2.s64(), arg3.s64());

    case BRIG_TYPE_U32: return op.ix(arg1.u32(), arg2.u32(), arg3.u32());
    case BRIG_TYPE_U64: return op.ix(arg1.u64(), arg2.u64(), arg3.u64());

    case BRIG_TYPE_F16: return op.fx(arg1.f16(), arg2.f16(), arg3.f16(), rounding);
    case BRIG_TYPE_F32: return op.fx(arg1.f32(), arg2.f32(), arg3.f32(), rounding);
    case BRIG_TYPE_F64: return op.fx(arg1.f64(), arg2.f64(), arg3.f64(), rounding);

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateTrnOpB(unsigned type, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    switch (type)
    {
    case BRIG_TYPE_B1:  return op.ix(arg1.b1(),  arg2.b1(),  arg3.b1());
    case BRIG_TYPE_B32: return op.ix(arg1.b32(), arg2.b32(), arg3.b32());
    case BRIG_TYPE_B64: return op.ix(arg1.b64(), arg2.b64(), arg3.b64());

    default: return emulationFailed();
    }
}

template<class T>
static Val emulateTrnOpSU_U32_U32(unsigned type, Val arg1, Val arg2, Val arg3, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == BRIG_TYPE_U32);
    assert(arg3.getType() == BRIG_TYPE_U32);

    switch (type)
    {
    case BRIG_TYPE_S32: return op.ix(arg1.s32(), arg2.u32(), arg3.u32());
    case BRIG_TYPE_S64: return op.ix(arg1.s64(), arg2.u32(), arg3.u32());

    case BRIG_TYPE_U32: return op.ix(arg1.u32(), arg2.u32(), arg3.u32());
    case BRIG_TYPE_U64: return op.ix(arg1.u64(), arg2.u32(), arg3.u32());

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Template Selectors for Quaternary Ops

template<class T>
static Val emulateQrnOpSU_U32_U32(unsigned type, Val arg1, Val arg2, Val arg3, Val arg4, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == BRIG_TYPE_U32);
    assert(arg4.getType() == BRIG_TYPE_U32);

    switch (type)
    {
    case BRIG_TYPE_S32: return op.ix(arg1.s32(), arg2.s32(), arg3.u32(), arg4.u32());
    case BRIG_TYPE_S64: return op.ix(arg1.s64(), arg2.s64(), arg3.u32(), arg4.u32());

    case BRIG_TYPE_U32: return op.ix(arg1.u32(), arg2.u32(), arg3.u32(), arg4.u32());
    case BRIG_TYPE_U64: return op.ix(arg1.u64(), arg2.u64(), arg3.u32(), arg4.u32());

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Simple HSAIL Instructions

template<typename T> bool undef_div_rem(T val1, T val2) // Identify special cases for integer div/rem
{
    assert(Val(val1).isInt());
    if (val2 == (T)0) return true;
    return Val(val1).isSignedInt() && val1 == numeric_limits<T>::min() && val2 == (T)-1;
}

struct op_abs    { FX_UNR(abs)          IX_UNR      { return std::abs(val); }};
struct op_neg    { FX_UNR(neg)          IX_UNR      { return -val; }};

struct op_not    {                      IX_UNR      { return static_cast<T>(val ^ 0xffffffffffffffffULL); }};

struct op_add    { FX_RND_BIN(add)      IX_BIN      { return val1 + val2; }};
struct op_sub    { FX_RND_BIN(sub)      IX_BIN      { return val1 - val2; }};
struct op_mul    { FX_RND_BIN(mul)      IX_BIN      { return val1 * val2; }};
struct op_div    { FX_RND_BIN(div)      IX_BIN_VAL  { return undef_div_rem(val1, val2)? undefValue() : Val(val1 / val2); }};
struct op_rem    {                      IX_BIN_VAL  { return undef_div_rem(val1, val2)? ((val2 == (T)0)? undefValue() : Val((T)0)) : Val(val1 % val2); }};
struct op_max    { FX_BIN(max)          IX_BIN      { return std::max(val1, val2); }};
struct op_min    { FX_BIN(min)          IX_BIN      { return std::min(val1, val2); }};
struct op_arg1   {                      IX_BIN      { return val1; }};
struct op_arg2   {                      IX_BIN      { return val2; }};

struct op_and    {                      IX_BIN      { return val1 & val2; }};
struct op_or     {                      IX_BIN      { return val1 | val2; }};
struct op_xor    {                      IX_BIN      { return val1 ^ val2; }};

struct op_inc    {                      IX_BIN      { return (val1 >= val2)? 0 : val1 + 1; }};
struct op_dec    {                      IX_BIN      { return (val1 == 0 || val1 > val2)? val2 : val1 - 1; }};

struct op_cas    {                      IX_TNR      { return (val1 == val2)? val3 : val1; }};

struct op_cmov   {                      IX_TNR      { return val1? val2 : val3; }};

struct op_cmp    { FX_BIN(cmp)          IX_BIN_INT  { return val1 < val2 ? -1 : val1 > val2 ? 1 : 0; }};

struct op_carry  {                      IX_BIN      { assert(!isSigned(val1)); T res = val1 + val2; return res < val1; }};
struct op_borrow {                      IX_BIN      { assert(!isSigned(val1)); return val1 < val2; }};

struct op_shl    {                      IU_BIN      { return val1 << (val2 & NumProps<T>::shiftMask()); }};
struct op_shr    {                      IU_BIN      { return val1 >> (val2 & NumProps<T>::shiftMask()); }};
                                                     
struct op_mad    { FX_RND_TRN(mad)      IX_TNR      { return val1 * val2 + val3; }};

struct op_cpsgn  { FX_BIN(cpsgn)      };

struct op_fract  { FX_RND_UNR(fract)  }; 
struct op_ceil   { FX_UNR(ceil)       }; 
struct op_floor  { FX_UNR(floor)      }; 
struct op_trunc  { FX_UNR(trunc)      }; 
struct op_rint   { FX_UNR(rint)       }; 

struct op_sqrt   { FX_RND_UNR(sqrt)   }; 
struct op_nsqrt  { FX_UNR(nsqrt)      }; 
struct op_nlog2  { FX_UNR(nlog2)      }; 
struct op_nexp2  { FX_UNR(nexp2)      }; 
struct op_nrsqrt { FX_UNR(nrsqrt)     }; 
struct op_nrcp   { FX_UNR(nrcp)       }; 
struct op_nsin   { FX_CHK_UNR(nsin)   }; 
struct op_ncos   { FX_CHK_UNR(ncos)   }; 

struct op_fma    { FX_RND_TRN(fma)    };
struct op_nfma   { FX_TRN(nfma)       };

struct op_i2f16  { FX_RND_UNR(i2f16)  };
struct op_i2f32  { FX_RND_UNR(i2f32)  };
struct op_i2f64  { FX_RND_UNR(i2f64)  };

struct op_f2f16  { FX_RND_UNR(f2f16)  };
struct op_f2f32  { FX_RND_UNR(f2f32)  };
struct op_f2f64  { FX_RND_UNR(f2f64)  };

//=============================================================================
//=============================================================================
//=============================================================================
// Saturating versions of add, sub and mul (used for packed operands)

struct op_add_sat
{
    template<typename T>
    T ix(unsigned type, T val1, T val2)
    {
        assert(getBrigTypeNumBits(type) == sizeof(T) * 8);

        T res  = (T)(val1 + val2);

        int sat = 0;

        if      (!isSigned(val1) && res < val1)                         sat =  1;
        else if (isSigned(val1)  && val1 >= 0 && val2 >= 0 && res <  0) sat =  1;
        else if (isSigned(val1)  && val1 <  0 && val2 <  0 && res >= 0) sat = -1;

        if (sat != 0) res = (T)getIntBoundary(type, sat == -1);

        return res;
    }
};

struct op_sub_sat
{
    template<typename T>
    T ix(unsigned type, T val1, T val2)
    {
        assert(getBrigTypeNumBits(type) == sizeof(T) * 8);

        T res  = (T)(val1 - val2);

        int sat = 0;

        if      (!isSigned(val1) && res > val1)                         sat = -1;
        else if (isSigned(val1)  && val1 >= 0 && val2 <  0 && res <  0) sat =  1;
        else if (isSigned(val1)  && val1 <  0 && val2 >= 0 && res >= 0) sat = -1;

        if (sat != 0) res = (T)getIntBoundary(type, sat == -1);

        return res;
    }
};

struct op_mul_sat
{
    template<typename T>
    T ix(unsigned type, T val1, T val2)
    {
        assert(getBrigTypeNumBits(type) == sizeof(T) * 8);

        T res = (T)(val1 * val2);

        int sat = 0;

        if (isSigned(val1))
        {
            T min = (T)getSignMask(getBrigTypeNumBits(type));

            if ((val1 < 0 && val2 == min) || // min negative value is a special case
                (val1 != 0 && ((T)(res / val1) != val2)))
            {
                sat = ((val1 < 0) != (val2 < 0))? -1 : 1;
            }
        }
        else // unsigned
        {
            if (val1 != 0 && (res / val1) != val2) sat = 1;
        }

        if (sat != 0) res = (T)getIntBoundary(type, sat == -1);

        return res;
    }
};

//=============================================================================
//=============================================================================
//=============================================================================

template<typename DT>
struct op_bitmask
{
    template<typename T>
    Val ix(T val1, T val2)
    {
        u64_t offset = val1 & NumProps<DT>::shiftMask();
        u64_t width  = val2 & NumProps<DT>::shiftMask();
        u64_t mask   = (1ULL << width) - 1;

        if (offset + width > NumProps<DT>::width()) return undefValue();

        unsigned dstType = Val(DT(0)).getType();
        return Val(dstType, mask << offset);
    }
};

struct op_bitsel { template<typename T> T ix(T val1, T val2, T val3) { return (val2 & val1) | (val3 & (~val1)); } };

struct op_bitextract
{
    template<typename T>
    Val ix(T val1, unsigned val2, unsigned val3)
    {
        u64_t offset    = val2 & NumProps<T>::shiftMask();
        u64_t width     = val3 & NumProps<T>::shiftMask();

        if (width == 0) return Val(static_cast<T>(0));
        if (width + offset > NumProps<T>::width()) return undefValue();

        u64_t shift = NumProps<T>::width() - width;
        return Val(static_cast<T>((val1 << (shift - offset)) >> shift));
    }
};

struct op_bitinsert
{
    template<typename T>
    Val ix(T val1, T val2, unsigned val3, unsigned val4)
    {
        u64_t offset    = val3 & NumProps<T>::shiftMask();
        u64_t width     = val4 & NumProps<T>::shiftMask();
        u64_t mask      = (1ULL << width) - 1;

        if (width + offset > NumProps<T>::width()) return undefValue();

        u64_t res = (val1 & ~(mask << offset)) | ((val2 & mask) << offset);
        return Val(static_cast<T>(res));
    }
};

struct op_bitrev
{
    template<typename T>
    T ix(T val)
    {
        T res = 0;
        for (unsigned i = 0; i < sizeof(T) * 8; i++, val = val >> 1) res = (res << 1) | (val & 0x1);
        return res;
    }
};

struct op_bitalign
{
    unsigned shift_mask;
    unsigned element_width;

    op_bitalign(unsigned mask, unsigned width = 1) : shift_mask(mask), element_width(width) {}

    template<typename T>
    T ix(T val0, T val1, T val2)
    {
        assert(sizeof(T) == 4);

        uint32_t shift = (val2 & shift_mask) * element_width;
        uint64_t value = (static_cast<uint64_t>(val1) << 32) | val0;

        return static_cast<uint32_t>((value >> shift) & 0xffffffff);
    }
};

//=============================================================================
//=============================================================================
//=============================================================================

template<typename T> static bool isSU24(T val)
{
    if (isSigned(val)) {
        return -0x400000 <= (signed) val && (signed) val <= 0x3FFFFF;
    } else {
        return val <= 0x7FFFFF;
    }
}

struct op_mad24
{
    unsigned res_shift;

    op_mad24(unsigned shift) : res_shift(shift) {}

    template<typename T>
    Val ix(T val1, T val2, T val3)
    {
        assert(sizeof(T) == 4);

        if (isSU24(val1) && isSU24(val2) && isSU24(val3))
        {
            if (isSigned(val1)) {
                return static_cast<T>(((static_cast<s64_t>(val1) * val2) >> res_shift) + val3);
            } else {
                return static_cast<T>(((static_cast<u64_t>(val1) * val2) >> res_shift) + val3);
            }
        }
        else
        {
            return undefValue();
        }
    }
};

/// Multiplies x, by a uint64_t integer and places the result into dest.
static void mul64hi(uint64_t *res, uint64_t x, uint64_t y)
{
    if (x == 0 || y == 0)
    {
        res[0] = 0;
        res[1] = 0;
        return;
    }

    // Split y into high 32-bit part (hy)  and low 32-bit part (ly)
    uint64_t ly = y & 0xffffffffULL, hy = y >> 32;
    uint64_t carry = 0;

    // For each digit of x.
    // Split x into high and low words
    uint64_t lx = x & 0xffffffffULL;
    uint64_t hx = x >> 32;
    // hasCarry - A flag to indicate if there is a carry to the next digit.
    // hasCarry == 0, no carry
    // hasCarry == 1, has carry
    // hasCarry == 2, no carry and the calculation result == 0.
    uint8_t hasCarry = 0;
    res[0] = carry + lx * ly;
    // Determine if the add above introduces carry.
    hasCarry = (res[0] < carry) ? 1 : 0;
    carry = hx * ly + (res[0] >> 32) + (hasCarry ? (1ULL << 32) : 0);
    // The upper limit of carry can be (2^32 - 1)(2^32 - 1) +
    // (2^32 - 1) + 2^32 = 2^64.
    hasCarry = (!carry && hasCarry) ? 1 : (!carry ? 2 : 0);

    carry += (lx * hy) & 0xffffffffULL;
    res[0] = (carry << 32) | (res[0] & 0xffffffffULL);
    carry = (((!carry && hasCarry != 2) || hasCarry == 1) ? (1ULL << 32) : 0) +
            (carry >> 32) + ((lx * hy) >> 32) + hx * hy;

    res[1] = carry;
}

static void neg64(uint64_t *val)
{
    bool borrow = false;
    for (unsigned i = 0; i < 2; ++i)
    {
        uint64_t x_tmp = borrow ? -1 : 0;
        borrow = (val[i] > x_tmp) || borrow;
        val[i] = x_tmp - val[i];
    }
}

struct op_mulhi
{
    template<typename T>
    T ix(T val1, T val2)
    {
        uint64_t res;
        bool sgn = isSigned(val1);
        unsigned bits = sizeof(T) * 8;

        if (bits <= 32)
        {
            uint64_t x = sgn ? (uint64_t)(int64_t)(val1) : (uint64_t)val1;
            uint64_t y = sgn ? (uint64_t)(int64_t)(val2) : (uint64_t)val2;
            res = ((x * y) >> bits);
        }
        else
        {
            uint64_t x = sgn ? (val1 < 0? (uint64_t)(0 - val1) : (uint64_t)val1) : (uint64_t)val1;
            uint64_t y = sgn ? (val2 < 0? (uint64_t)(0 - val2) : (uint64_t)val2) : (uint64_t)val2;
            bool isNegativeRes = sgn && (val1 < 0) != (val2 < 0) && val1 != 0 && val2 != 0;

            uint64_t dst[2];
            mul64hi(dst, x, y);
            if (isNegativeRes) neg64(dst);

            res = dst[1];
        }

        return *(const T*)&res;
    }
};

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of 'class' Instruction

static Val emulate_class(unsigned stype, Val arg1, Val arg2)
{
    assert(arg1.isFloat());
    assert(arg1.getType() == stype);
    assert(arg2.getType() == BRIG_TYPE_U32);

    u32_t res = 0;
    u32_t flags = arg2.u32();

    if (arg1.isSpecialFloat())
    {
        if ((flags & 0x001) && arg1.isSignalingNan())      res = 1;
        if ((flags & 0x002) && arg1.isQuietNan())          res = 1;
        if ((flags & 0x004) && arg1.isNegativeInf())       res = 1;
        if ((flags & 0x200) && arg1.isPositiveInf())       res = 1;
    }
    else if (arg1.isSubnormal())
    {
        if ((flags & 0x010) && arg1.isNegativeSubnormal()) res = 1;
        if ((flags & 0x080) && arg1.isPositiveSubnormal()) res = 1;
    }
    else if (arg1.isZero())
    {
        if ((flags & 0x020) &&  arg1.isNegativeZero())     res = 1;
        if ((flags & 0x040) &&  arg1.isPositiveZero())     res = 1;
    }
    else
    {
        if ((flags & 0x100) &&  arg1.isPositive())         res = 1;
        if ((flags & 0x008) && !arg1.isPositive())         res = 1;
    }

    return Val(BRIG_TYPE_B1, res);
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Bit String operations

static Val emulate_popcount(unsigned stype, Val arg)
{
    assert(arg.getType() == stype);
    assert(isBitType(stype));

    u32_t count = 0;
    for (u64_t val = arg.getAsB64(); val > 0; val >>= 1)
    {
        if ((val & 0x1) != 0) count++;
    }
    return Val(count);
}

static Val emulate_firstbit(unsigned stype, Val arg)
{
    assert(arg.getType() == stype);

    u64_t firstBit = 0x1ULL << (arg.getSize() - 1);
    s64_t val = arg.getAsS64(); // zero/sign-extend as necessary

    if (arg.isSignedInt() && val < 0) val = ~val;
    if (val == 0) return Val(BRIG_TYPE_U32, -1);

    u32_t res = 0;
    for (; (val & firstBit) == 0; ++res, val <<= 1);
    return Val(res);
}

static Val emulate_lastbit(unsigned stype, Val arg)
{
    assert(arg.getType() == stype);

    u64_t val = arg.getAsB64(); // Disable sign-extension

    if (val == 0) return Val(BRIG_TYPE_U32, -1);

    u32_t res = 0;
    for (; (val & 0x1) == 0; ++res, val >>= 1);
    return Val(res);
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of combine/expand Instruction

static Val emulate_combine(unsigned type, unsigned stype, Val arg)
{
    assert(arg.isVector());
    assert(arg.getVecType() == stype);

    if (type == BRIG_TYPE_B64)
    {
        assert(arg.getDim() == 2);
        assert(stype == BRIG_TYPE_B32);

        return Val(type, (arg[1].getAsB64() << 32) | arg[0].b32());
    }

    assert(type == BRIG_TYPE_B128);

    if (stype == BRIG_TYPE_B32)
    {
        assert(arg.getDim() == 4);

        return Val(type,
                   b128((arg[1].getAsB64() << 32) | arg[0].b32(),
                        (arg[3].getAsB64() << 32) | arg[2].b32()));
    }
    else
    {
        assert(arg.getDim() == 2);
        assert(stype == BRIG_TYPE_B64);

        return Val(type, b128(arg[0].b64(), arg[1].b64()));
    }
}

static Val emulate_expand(unsigned type, unsigned stype, Val arg)
{
    assert(!arg.isVector());
    assert(arg.getType() == stype);

    if (stype == BRIG_TYPE_B64)
    {
        assert(type == BRIG_TYPE_B32);

        return Val(2,
                   Val(type, arg.getAsB32(0)),
                   Val(type, arg.getAsB32(1)),
                   Val(),
                   Val());
    }
    else
    {
        assert(stype == BRIG_TYPE_B128);

        if (type == BRIG_TYPE_B32)
        {
            return Val(4,
                       Val(type, arg.getAsB32(0)),
                       Val(type, arg.getAsB32(1)),
                       Val(type, arg.getAsB32(2)),
                       Val(type, arg.getAsB32(3)));
        }
        else
        {
            assert(type == BRIG_TYPE_B64);
            return Val(2,
                       Val(type, arg.getAsB64(0)),
                       Val(type, arg.getAsB64(1)),
                       Val(),
                       Val());
        }
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of cmp Instruction

static Val emulateCmp(unsigned type, unsigned stype, unsigned op, Val arg1, Val arg2)
{
    using namespace Brig;

    assert(arg1.getType() == stype);
    assert(arg2.getType() == stype);

    bool isNan = arg1.isNan() || arg2.isNan();
    int cmp = emulateBinOpBSUF(stype, AluMod::ROUNDING_NONE, arg1, arg2, op_cmp());

    bool res;
    bool signaling = false;

    switch (op)
    {
    case BRIG_COMPARE_EQ:   res = (cmp == 0) && !isNan;                      break;
    case BRIG_COMPARE_SEQ:  res = (cmp == 0) && !isNan;   signaling = true;  break;

    case BRIG_COMPARE_EQU:  res = (cmp == 0) || isNan;                       break;
    case BRIG_COMPARE_SEQU: res = (cmp == 0) || isNan;    signaling = true;  break;

    case BRIG_COMPARE_NE:   res = (cmp != 0) && !isNan;                      break;
    case BRIG_COMPARE_SNE:  res = (cmp != 0) && !isNan;   signaling = true;  break;
                                                                             
    case BRIG_COMPARE_NEU:  res = (cmp != 0) || isNan;                       break;
    case BRIG_COMPARE_SNEU: res = (cmp != 0) || isNan;    signaling = true;  break;
                                                                             
    case BRIG_COMPARE_LT:   res = (cmp == -1) && !isNan;                     break;
    case BRIG_COMPARE_SLT:  res = (cmp == -1) && !isNan;  signaling = true;  break;
                                                                             
    case BRIG_COMPARE_LTU:  res = (cmp == -1) || isNan;                      break;
    case BRIG_COMPARE_SLTU: res = (cmp == -1) || isNan;   signaling = true;  break;
                                                                             
    case BRIG_COMPARE_LE:   res = (cmp != 1) && !isNan;                      break;
    case BRIG_COMPARE_SLE:  res = (cmp != 1) && !isNan;   signaling = true;  break;
                                                                             
    case BRIG_COMPARE_LEU:  res = (cmp != 1) || isNan;                       break;
    case BRIG_COMPARE_SLEU: res = (cmp != 1) || isNan;    signaling = true;  break;
                                                                             
    case BRIG_COMPARE_GT:   res = (cmp == 1) && !isNan;                      break;
    case BRIG_COMPARE_SGT:  res = (cmp == 1) && !isNan;   signaling = true;  break;

    case BRIG_COMPARE_GTU:  res = (cmp == 1) || isNan;                       break;
    case BRIG_COMPARE_SGTU: res = (cmp == 1) || isNan;    signaling = true;  break;
                                                                             
    case BRIG_COMPARE_GE:   res = (cmp != -1) && !isNan;                     break;
    case BRIG_COMPARE_SGE:  res = (cmp != -1) && !isNan;  signaling = true;  break;
                                                                             
    case BRIG_COMPARE_GEU:  res = (cmp != -1) || isNan;                      break;
    case BRIG_COMPARE_SGEU: res = (cmp != -1) || isNan;   signaling = true;  break;
                                                                             
    case BRIG_COMPARE_NUM:  res = !isNan;                                    break;
    case BRIG_COMPARE_SNUM: res = !isNan;                 signaling = true;  break;

    case BRIG_COMPARE_NAN:  res = isNan;                                     break;
    case BRIG_COMPARE_SNAN: res = isNan;                  signaling = true;  break;

    default: 
        assert(false);
        return emulationFailed();
    }

    if (signaling && isNan) return unimplemented();

    switch (type)
    {
    case BRIG_TYPE_B1:  return Val(type, res? 1 : 0);

    case BRIG_TYPE_S32:
    case BRIG_TYPE_S64:
    case BRIG_TYPE_U32:
    case BRIG_TYPE_U64: return Val(type, res? -1 : 0);

    case BRIG_TYPE_F16: return Val(res? f16_t(1.0) : f16_t(0.0));
    case BRIG_TYPE_F32: return Val(res? 1.0f : 0.0f);
    case BRIG_TYPE_F64: return Val(res? 1.0  : 0.0 );

    default:
        return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of cvt Instruction

static bool isIntegral(Val val)
{
    //F Should check if exception policy is enabled for the inexact exception

    Val fract = emulateUnrOpF(val.getType(), AluMod::ROUNDING_NEAR, val, op_fract()); //F1.0 use isNatural instead of op_fract
    return fract.isZero();
}

static Val cvt_f2i(unsigned type, unsigned rounding, Val val)
{
    assert(isIntType(type));

    bool isValid;
    uint64_t res;

    switch(val.getType())
    {
    case BRIG_TYPE_F16: res = emulate_f2i(val.f16(), type, rounding, isValid); break;
    case BRIG_TYPE_F32: res = emulate_f2i(val.f32(), type, rounding, isValid); break;
    case BRIG_TYPE_F64: res = emulate_f2i(val.f64(), type, rounding, isValid); break;
        
    default:
        assert(false);
        res = 0;
        break;
    }
    
    if (!isValid) return undefValue();

    if (isSignalingRounding(rounding) && !isIntegral(val)) return unimplemented(); // generate an inexact exception

    return isSignedType(type)? Val(type, static_cast<s64_t>(res)) : Val(type, static_cast<u64_t>(res));
}

static Val cvt_f2f(unsigned type, unsigned stype, unsigned rounding, Val arg)
{
    assert(isFloatType(stype));
    assert(isFloatType(type));
    assert(type != stype);

    if (!isSupportedFpRounding(rounding)) return unimplemented();

    switch(type)
    {
    case BRIG_TYPE_F16: return emulateUnrOpF(stype, rounding, arg, op_f2f16());
    case BRIG_TYPE_F32: return emulateUnrOpF(stype, rounding, arg, op_f2f32());
    case BRIG_TYPE_F64: return emulateUnrOpF(stype, rounding, arg, op_f2f64());
        
    default:
        assert(false);
        return emulationFailed();
    }
}

static Val cvt_f2x(unsigned type, unsigned stype, unsigned rounding, Val arg)
{
    assert(isFloatType(stype));

    if (isFloatType(type))
    {
        return cvt_f2f(type, stype, rounding, arg);
    }
    else
    {
        assert(isIntType(type));
        return cvt_f2i(type, rounding, arg);
    }
}

static Val cvt_i2f(unsigned type, Val val, unsigned rounding)
{
    if (!isSupportedFpRounding(rounding)) return unimplemented();

    switch(type)
    {
    case BRIG_TYPE_F16: return emulateUnrOpUS_Rnd(val.getType(), rounding, val, op_i2f16());
    case BRIG_TYPE_F32: return emulateUnrOpUS_Rnd(val.getType(), rounding, val, op_i2f32());
    case BRIG_TYPE_F64: return emulateUnrOpUS_Rnd(val.getType(), rounding, val, op_i2f64());

    default:
        assert(false);
        return emulationFailed();
    }
}

static Val cvt_i2x(unsigned type, unsigned stype, unsigned rounding, Val arg)
{
    assert(isIntType(stype));

    return isIntType(type)?
            Val(type, arg.getAsS64()) : // zero/sign-extend as necessary
            cvt_i2f(type, arg, rounding);
}

static Val cvt_x2b1(unsigned type, unsigned stype, Val arg)
{
    return isIntType(stype)?
            Val(type, arg.getAsB64() != 0) :
            Val(type, !arg.isZero());
}

static Val emulateCvt(unsigned type, unsigned stype, AluMod aluMod, Val arg)
{
    assert(arg.getType() == stype);
    assert(type != stype);

    unsigned rounding = aluMod.getRounding();

    // to avoid handling b1 in other places, pretend that it is an u32 value
    if (stype == BRIG_TYPE_B1) 
    {
        assert(rounding == Brig::BRIG_ROUND_NONE);
        if (isFloatType(type)) rounding = Brig::BRIG_ROUND_FLOAT_NEAR_EVEN; // Any fp mode will do
        arg = Val(BRIG_TYPE_U32, arg.getAsB64());
        stype = BRIG_TYPE_U32;
    }

    if (type == BRIG_TYPE_B1)  return cvt_x2b1(type, stype, arg);
    else                       return isFloatType(stype)?
                                      cvt_f2x(type, stype, rounding, arg) :
                                      cvt_i2x(type, stype, rounding, arg);
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of atomic Instruction

static Val emulateAtomicMem(unsigned type, unsigned atomicOperation, Val arg1, Val arg2, Val arg3)
{
    using namespace Brig;

    assert(arg1.getType() == type);
    assert(BRIG_ATOMIC_ST || arg2.getType() == type);

    switch (atomicOperation)
    {
    case BRIG_ATOMIC_AND:     return emulateBinOpB(type, arg1, arg2, op_and());
    case BRIG_ATOMIC_OR:      return emulateBinOpB(type, arg1, arg2, op_or());
    case BRIG_ATOMIC_XOR:     return emulateBinOpB(type, arg1, arg2, op_xor());

    case BRIG_ATOMIC_ADD:     return emulateBinOpSU(type, arg1, arg2, op_add());
    case BRIG_ATOMIC_SUB:     return emulateBinOpSU(type, arg1, arg2, op_sub());
    case BRIG_ATOMIC_MAX:     return emulateBinOpSU(type, arg1, arg2, op_max());
    case BRIG_ATOMIC_MIN:     return emulateBinOpSU(type, arg1, arg2, op_min());

    case BRIG_ATOMIC_WRAPINC: return emulateBinOpSU(type, arg1, arg2, op_inc());
    case BRIG_ATOMIC_WRAPDEC: return emulateBinOpSU(type, arg1, arg2, op_dec());
    case BRIG_ATOMIC_EXCH:    return emulateBinOpB(type, arg1, arg2, op_arg2());
    case BRIG_ATOMIC_CAS:     return emulateTrnOpB(type, arg1, arg2, arg3, op_cas());

    case BRIG_ATOMIC_LD:      assert(arg1.getType() == type); return arg1;
    case BRIG_ATOMIC_ST:      assert(arg2.getType() == type); return arg2;

    default: return emulationFailed();
    }
}

static Val emulateAtomicDst(unsigned opcode, Val arg1)
{
    return (opcode == Brig::BRIG_OPCODE_ATOMIC)? arg1 : emptyDstValue();
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of carry/borrow

template<class T>
static Val emulateAluFlag(unsigned type, Val arg1, Val arg2, T op)
{
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    unsigned utype = type;

    if (isSignedType(type)) // Convert args to unsigned (to simplify template implementation)
    {
        utype = (getBrigTypeNumBits(type) == 32)? BRIG_TYPE_U32 : BRIG_TYPE_U64;

        arg1 = Val(utype, arg1.getAsB64()); // copy bits w/o sign-extension
        arg2 = Val(utype, arg2.getAsB64()); // copy bits w/o sign-extension
    }

    Val res = emulateBinOpSU(utype, arg1, arg2, op);

    // Convert back to original type
    // NB: result is either 0 or 1, sign-extension is not necessary
    return Val(type, res.getAsB64());
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Irregular Instructions operating with packed data

static Val emulate_shuffle(unsigned type, Val arg1, Val arg2, Val arg3)
{
    assert(arg1.isPacked());
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(isBitType(arg3.getType()) && arg3.getSize() == 32);

    Val dst(type, 0);

    u32_t ctl   = arg3.getAsB32();          // value that controls shuffling
    u32_t dim   = getPackedTypeDim(type);   // number of elements in packed data
    u32_t width = range2width(dim);         // number of control bits per element
    u64_t mask  = getWidthMask(width);      // mask for extracting control bits

    for (unsigned i = 0; i < dim; ++i)
    {
        unsigned idx = (ctl & mask);
        u64_t x = (i < dim / 2)? arg1.getElement(idx) : arg2.getElement(idx);
        dst.setElement(i, x);
        ctl >>= width;
    }

    return dst;
}

static Val emulate_unpackHalf(unsigned type, bool lowHalf, Val arg1, Val arg2)
{
    assert(arg1.isPacked());
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);

    Val dst(type, 0);

    unsigned dim = getPackedTypeDim(type);
    unsigned srcPos = lowHalf? 0 : (dim / 2);

    for (unsigned dstPos = 0; dstPos < dim; ++srcPos)
    {
        dst.setElement(dstPos++, arg1.getElement(srcPos));
        dst.setElement(dstPos++, arg2.getElement(srcPos));
    }

    return dst;
}

static Val emulate_pack(unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3)
{
    assert(isPackedType(type));
    assert(!isPackedType(stype));
    assert(arg1.getType() == type);
    assert(arg2.getType() == stype);
    assert(arg3.getType() == BRIG_TYPE_U32);

    u32_t dim   = getPackedTypeDim(type);   // number of elements in packed data
    u32_t width = range2width(dim);         // number of control bits per element
    u64_t mask  = getWidthMask(width);      // mask for extracting control bits

    Val dst = arg1;
    dst.setElement(arg3.u32() & mask, arg2.getAsB64());
    return dst;
}

static Val emulate_unpack(unsigned type, unsigned stype, Val arg1, Val arg2)
{
    assert(!isPackedType(type));
    assert(isPackedType(stype));
    assert(arg1.getType() == stype);
    assert(arg2.getType() == BRIG_TYPE_U32);

    u32_t dim   = getPackedTypeDim(stype);  // number of elements in packed data
    u32_t width = range2width(dim);         // number of control bits per element
    u64_t mask  = getWidthMask(width);      // mask for extracting control bits

    // Extract specified element in native type
    Val res(arg1.getElementType(), arg1.getElement(arg2.u32() & mask));

    // The required type may be wider than extracted (for s/u).
    // In this case sign-extend or zero-extend the value as required.
    if (res.getType() != type)
    {
        assert(!res.isFloat());
        assert(!isFloatType(type));

        res = res.isSignedInt()? Val(type, res.getAsS64()) : Val(type, res.getAsB64());
    }

    return res;
}

static Val emulate_lerp(unsigned type, Val arg1, Val arg2, Val arg3)
{
    assert(type == BRIG_TYPE_U8X4);
    assert(arg1.getType() == type);
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);

    Val res(type, 0);

    for (unsigned i = 0; i < 4; ++i)
    {
        res.setElement(i, (arg1.getElement(i) + arg2.getElement(i) + (arg3.getElement(i) & 0x1)) / 2);
    }

    return res;
}

static Val emulate_packcvt(unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3, Val arg4)
{
    assert(type == BRIG_TYPE_U8X4);
    assert(stype == BRIG_TYPE_F32);
    assert(arg1.getType() == stype);
    assert(arg2.getType() == stype);
    assert(arg3.getType() == stype);
    assert(arg4.getType() == stype);

    Val x1 = emulateCvt(BRIG_TYPE_U8, stype, AluMod(AluMod::ROUNDING_NEARI_SAT), arg1);
    Val x2 = emulateCvt(BRIG_TYPE_U8, stype, AluMod(AluMod::ROUNDING_NEARI_SAT), arg2);
    Val x3 = emulateCvt(BRIG_TYPE_U8, stype, AluMod(AluMod::ROUNDING_NEARI_SAT), arg3);
    Val x4 = emulateCvt(BRIG_TYPE_U8, stype, AluMod(AluMod::ROUNDING_NEARI_SAT), arg4);

    if (x1.empty() || x2.empty() || x3.empty() || x4.empty()) return undefValue();

    Val res(type, 0);
    res.setElement(0, x1.u8());
    res.setElement(1, x2.u8());
    res.setElement(2, x3.u8());
    res.setElement(3, x4.u8());

    return res;
}

static Val emulate_unpackcvt(unsigned type, unsigned stype, Val arg1, Val arg2)
{
    assert(type == BRIG_TYPE_F32);
    assert(stype == BRIG_TYPE_U8X4);
    assert(arg1.getType() == stype);
    assert(arg2.getType() == BRIG_TYPE_U32);

    Val val(BRIG_TYPE_U8, arg1.getElement(arg2.u32() & 0x3));
    return emulateCvt(type, BRIG_TYPE_U8, AluMod(AluMod::ROUNDING_NEAR), val);
}

static Val emulate_cmov(unsigned type, Val arg1, Val arg2, Val arg3)
{
    assert(arg1.isPacked());
    assert(isUnsignedType(arg1.getElementType()));
    assert(arg2.getType() == type);
    assert(arg3.getType() == type);
    assert(arg1.getSize() == arg2.getSize());
    assert(arg1.getElementSize() == arg2.getElementSize());

    Val dst = arg2;

    u32_t dim = getPackedTypeDim(type);   // number of elements in packed data

    for (unsigned i = 0; i < dim; ++i)
    {
        dst.setElement(i, arg1.getElement(i)? arg2.getElement(i) : arg3.getElement(i));
    }

    return dst;
}

static u64_t sad(u64_t a, u64_t b) { return a < b ? b - a : a - b; }

static Val emulate_sad(unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3)
{
    assert(type == BRIG_TYPE_U32);
    assert(stype == BRIG_TYPE_U32 || stype == BRIG_TYPE_U16X2 || stype == BRIG_TYPE_U8X4);
    assert(arg1.getType() == stype);
    assert(arg2.getType() == stype);
    assert(arg3.getType() == BRIG_TYPE_U32);

    uint64_t res = arg3.u32();

    if (stype == BRIG_TYPE_U32)
    {
        res += sad(arg1.u32(), arg2.u32());
    }
    else
    {
        assert(isPackedType(stype));
        u32_t dim = getPackedTypeDim(stype);
        for (unsigned i = 0; i < dim; ++i)
        {
            res += sad(arg1.getElement(i), arg2.getElement(i));
        }
    }

    return Val(type, res);
}

static Val emulate_sadhi(unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3)
{
    assert(type == BRIG_TYPE_U16X2);
    assert(stype == BRIG_TYPE_U8X4);
    assert(arg1.getType() == stype);
    assert(arg2.getType() == stype);
    assert(arg3.getType() == BRIG_TYPE_U16X2);

    uint64_t res = arg3.getElement(1);
    u32_t dim = getPackedTypeDim(stype);
    for (unsigned i = 0; i < dim; ++i)
    {
        res += sad(arg1.getElement(i), arg2.getElement(i));
    }

    Val dst = arg3;
    dst.setElement(1, res);
    return dst;
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Instructions in Basic/Mod Formats

static Val emulateMod(unsigned opcode, unsigned type, AluMod aluMod, Val arg1, Val arg2, Val arg3 = Val(), Val arg4 = Val())
{
    using namespace Brig;

    if (!isSupportedFpRounding(aluMod.getRounding())) return unimplemented();

    unsigned rounding = aluMod.getRounding();

    switch (opcode)
    {
    case BRIG_OPCODE_ABS:       return emulateUnrOpSF(type, rounding, arg1, op_abs());
    case BRIG_OPCODE_NEG:       return emulateUnrOpSF(type, rounding, arg1, op_neg());

    case BRIG_OPCODE_NOT:       return emulateUnrOpB(type, arg1, op_not());

    case BRIG_OPCODE_ADD:       return emulateBinOpBSUF(type, rounding, arg1, arg2, op_add());
    case BRIG_OPCODE_SUB:       return emulateBinOpBSUF(type, rounding, arg1, arg2, op_sub());
    case BRIG_OPCODE_MUL:       return emulateBinOpBSUF(type, rounding, arg1, arg2, op_mul());
    case BRIG_OPCODE_DIV:       return emulateBinOpBSUF(type, rounding, arg1, arg2, op_div());
    case BRIG_OPCODE_MAX:       return emulateBinOpBSUF(type, rounding, arg1, arg2, op_max());
    case BRIG_OPCODE_MIN:       return emulateBinOpBSUF(type, rounding, arg1, arg2, op_min());

    case BRIG_OPCODE_MULHI:     return emulateBinOpSU(type, arg1, arg2, op_mulhi());
    case BRIG_OPCODE_REM:       return emulateBinOpSU(type, arg1, arg2, op_rem());

    case BRIG_OPCODE_MUL24:     return emulateTrnOpSU(type, arg1, arg2, Val(type, 0), op_mad24(0));
    case BRIG_OPCODE_MUL24HI:   return emulateTrnOpSU(type, arg1, arg2, Val(type, 0), op_mad24(32));

    case BRIG_OPCODE_MAD24:     return emulateTrnOpSU(type, arg1, arg2, arg3, op_mad24(0));
    case BRIG_OPCODE_MAD24HI:   return emulateTrnOpSU(type, arg1, arg2, arg3, op_mad24(32));

    case BRIG_OPCODE_AND:       return emulateBinOpB(type, arg1, arg2, op_and());
    case BRIG_OPCODE_OR:        return emulateBinOpB(type, arg1, arg2, op_or());
    case BRIG_OPCODE_XOR:       return emulateBinOpB(type, arg1, arg2, op_xor());

    case BRIG_OPCODE_CARRY:     return emulateAluFlag(type, arg1, arg2, op_carry());
    case BRIG_OPCODE_BORROW:    return emulateAluFlag(type, arg1, arg2, op_borrow());

    case BRIG_OPCODE_SHL:       return emulateBinOpSU_U32(type, arg1, arg2, op_shl());
    case BRIG_OPCODE_SHR:       return emulateBinOpSU_U32(type, arg1, arg2, op_shr());

    case BRIG_OPCODE_COPYSIGN:  return emulateBinOpF(type, rounding, arg1, arg2, op_cpsgn());

    case BRIG_OPCODE_FRACT:     return emulateUnrOpF(type, rounding, arg1, op_fract());
    case BRIG_OPCODE_CEIL:      return emulateUnrOpF(type, rounding, arg1, op_ceil());
    case BRIG_OPCODE_FLOOR:     return emulateUnrOpF(type, rounding, arg1, op_floor());
    case BRIG_OPCODE_RINT:      return emulateUnrOpF(type, rounding, arg1, op_rint());
    case BRIG_OPCODE_TRUNC:     return emulateUnrOpF(type, rounding, arg1, op_trunc());
    
    case BRIG_OPCODE_SQRT:      return emulateUnrOpF(type, rounding, arg1, op_sqrt());
    case BRIG_OPCODE_NCOS:      return emulateUnrOpF(type, rounding, arg1, op_ncos());
    case BRIG_OPCODE_NSIN:      return emulateUnrOpF(type, rounding, arg1, op_nsin());
    case BRIG_OPCODE_NEXP2:     return emulateUnrOpF(type, rounding, arg1, op_nexp2());
    case BRIG_OPCODE_NLOG2:     return emulateUnrOpF(type, rounding, arg1, op_nlog2());
    case BRIG_OPCODE_NSQRT:     return emulateUnrOpF(type, rounding, arg1, op_nsqrt());
    case BRIG_OPCODE_NRSQRT:    return emulateUnrOpF(type, rounding, arg1, op_nrsqrt());
    case BRIG_OPCODE_NRCP:      return emulateUnrOpF(type, rounding, arg1, op_nrcp());
    case BRIG_OPCODE_NFMA:      return emulateTrnOpF(type, rounding, arg1, arg2, arg3, op_nfma());

    case BRIG_OPCODE_FMA:       return emulateTrnOpF(type, rounding, arg1, arg2, arg3, op_fma());

    case BRIG_OPCODE_MAD:       return emulateTrnOpSUF(type, rounding, arg1, arg2, arg3, op_mad());

    case BRIG_OPCODE_MOV:       assert(arg1.getType() == type);         return arg1;
    case BRIG_OPCODE_CMOV:      assert(arg1.getType() == BRIG_TYPE_B1); return emulateTrnOpB(type, Val(type, arg1.getAsB32()), arg2, arg3, op_cmov());

    case BRIG_OPCODE_BITMASK:   return (type == BRIG_TYPE_B32)?
                                       emulateBinOpB(arg1.getType(), arg1, arg2, op_bitmask<b32_t>()) :
                                       emulateBinOpB(arg1.getType(), arg1, arg2, op_bitmask<b64_t>());
    case BRIG_OPCODE_BITSELECT: return emulateTrnOpB(type, arg1, arg2, arg3, op_bitsel());

    case BRIG_OPCODE_BITREV:    return emulateUnrOpB(type, arg1, op_bitrev());
    case BRIG_OPCODE_BITEXTRACT:return emulateTrnOpSU_U32_U32(type, arg1, arg2, arg3, op_bitextract());
    case BRIG_OPCODE_BITINSERT: return emulateQrnOpSU_U32_U32(type, arg1, arg2, arg3, arg4, op_bitinsert());

    case BRIG_OPCODE_BITALIGN:  return emulateTrnOpB(type, arg1, arg2, arg3, op_bitalign(31, 1));
    case BRIG_OPCODE_BYTEALIGN: return emulateTrnOpB(type, arg1, arg2, arg3, op_bitalign( 3, 8));

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Instructions in SourceType Format

static Val emulateSourceType(unsigned opcode, unsigned type, unsigned stype, Val arg1, Val arg2, Val arg3)
{
    using namespace Brig;

    switch (opcode)
    {
    case BRIG_OPCODE_CLASS:     return emulate_class(stype, arg1, arg2);
    case BRIG_OPCODE_POPCOUNT:  return emulate_popcount(stype, arg1);
    case BRIG_OPCODE_FIRSTBIT:  return emulate_firstbit(stype, arg1);
    case BRIG_OPCODE_LASTBIT:   return emulate_lastbit(stype, arg1);

    case BRIG_OPCODE_COMBINE:   return emulate_combine(type, stype, arg1);
    case BRIG_OPCODE_EXPAND:    return emulate_expand(type, stype, arg1);

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of Instructions in Mem Format (ld/st)

static Val emulateMemDst(unsigned segment, unsigned opcode, Val arg)
{
    using namespace Brig;

    switch (opcode)
    {
    case BRIG_OPCODE_LD:  return arg;
    case BRIG_OPCODE_ST:  return emptyDstValue();

    default: return emulationFailed();
    }
}

static Val emulateMemMem(unsigned segment, unsigned opcode, Val arg0, Val arg1)
{
    using namespace Brig;

    switch (opcode)
    {
    case BRIG_OPCODE_LD:  return arg1;
    case BRIG_OPCODE_ST:  return arg0;

    default: return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Helpers

// Arrays declared at TOPLEVEL must belong to global, group or private segments.
// NB: Readonly cannot be initialized and so unsuitable for tesing.
static bool isSupportedSegment(unsigned segment)
{
    using namespace Brig;

    switch (segment)
    {
    case BRIG_SEGMENT_GLOBAL:
    case BRIG_SEGMENT_GROUP:
    case BRIG_SEGMENT_PRIVATE:
        return true;
    default:
        return false;
    }
}

static bool emulateFtz(Inst inst, Val& arg0, Val& arg1, Val& arg2, Val& arg3, Val& arg4)
{
    bool ftz = false;

    if      (InstMod i = inst)  ftz = i.modifier().ftz();
    else if (InstCmp i = inst)  ftz = i.modifier().ftz();
    else if (InstCvt i = inst)  ftz = i.modifier().ftz();

    if (ftz)
    {
        arg0 = arg0.ftz();
        arg1 = arg1.ftz();
        arg2 = arg2.ftz();
        arg3 = arg3.ftz();
        arg4 = arg4.ftz();
    }

    return ftz;
}

static bool discardNanSign(unsigned opcode)
{
    using namespace Brig;

    switch (opcode)
    {
    case BRIG_OPCODE_ABS:
    case BRIG_OPCODE_NEG:
    case BRIG_OPCODE_CLASS:
    case BRIG_OPCODE_COPYSIGN:
        return false;
    default:
        return true;
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Helpers for instructions with packed operands

// Identify regular operations with packed data.
// Most of these operations may be reduced to non-packed operations.
static bool isCommonPacked(Inst inst)
{
    using namespace Brig;

    return (getPacking(inst) != BRIG_PACK_NONE) ||
           (isPackedType(inst.type()) &&
                (inst.opcode() == BRIG_OPCODE_SHL  ||
                 inst.opcode() == BRIG_OPCODE_SHR));
}

// Identify special (irregular) operations with packed data
// which cannot be reduced to non-packed operations
static bool isSpecialPacked(Inst inst)
{
    using namespace Brig;

    switch (inst.opcode())
    {
    case BRIG_OPCODE_SHUFFLE:   // Packed Data Operations
    case BRIG_OPCODE_UNPACKHI:
    case BRIG_OPCODE_UNPACKLO:
    case BRIG_OPCODE_PACK:
    case BRIG_OPCODE_UNPACK:
        return true;
    case BRIG_OPCODE_CMOV:
        return isPackedType(inst.type());
    case BRIG_OPCODE_PACKCVT:   // Multimedia Operations
    case BRIG_OPCODE_UNPACKCVT:
    case BRIG_OPCODE_LERP:
    case BRIG_OPCODE_SAD:
    case BRIG_OPCODE_SADHI:
        return true;
    default:
        return false;
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of packed operations

// MulHi for packed types must be handled separately because there are 2 cases
// depending on element type:
// - subword types: use 'mul' and extract result from high bits of the product;
// - 32/64 bit types: use regular 'mulhi'
static Val emulateMulHiPacked(unsigned type, unsigned baseType, Val arg1, Val arg2)
{
    assert(isPackedType(type));
    assert(arg1.getType() == baseType);
    assert(arg2.getType() == baseType);

    using namespace Brig;

    unsigned elementType = packedType2elementType(type);
    unsigned opcode = (getBrigTypeNumBits(elementType) < 32)? BRIG_OPCODE_MUL : BRIG_OPCODE_MULHI;

    Val res = emulateMod(opcode, baseType, AluMod(), arg1, arg2);

    if (opcode == BRIG_OPCODE_MUL)
    {
        res = Val(baseType, res.getAsB64() >> getBrigTypeNumBits(elementType));
    }

    return res;
}

static Val emulateSat(unsigned opcode, unsigned type, Val arg1, Val arg2)
{
    assert(isPackedType(type));
    assert(!isFloatType(packedType2elementType(type)));

    using namespace Brig;

    // Repack from base type to element type
    unsigned baseType    = packedType2baseType(type);
    unsigned elementType = packedType2elementType(type);
    arg1 = Val(elementType, arg1.getAsB64());
    arg2 = Val(elementType, arg2.getAsB64());

    Val res;

    switch (opcode)
    {
    case BRIG_OPCODE_ADD: res = emulateBinOpSat(elementType, arg1, arg2, op_add_sat()); break;
    case BRIG_OPCODE_SUB: res = emulateBinOpSat(elementType, arg1, arg2, op_sub_sat()); break;
    case BRIG_OPCODE_MUL: res = emulateBinOpSat(elementType, arg1, arg2, op_mul_sat()); break;
    default:
        return emulationFailed();
    }

    return res.isSignedInt()? Val(baseType, res.getAsS64()) : Val(baseType, res.getAsB64());
}

static Val emulateDstValPackedRegular(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    using namespace Brig;

    assert(arg0.empty());
    assert(arg3.empty());
    assert(arg4.empty());

    assert(!arg1.empty());
    assert(!arg1.isVector());
    assert(isPackedType(arg1.getType()));

    unsigned type        = inst.type();
    unsigned stype       = InstCmp(inst)? getSrcType(inst) : type;
    unsigned packing     = getPacking(inst);
    unsigned opcode      = inst.opcode();

    if (opcode == BRIG_OPCODE_SHL || opcode == BRIG_OPCODE_SHR) packing = BRIG_PACK_PP;

    unsigned baseType    = packedType2baseType(type);
    unsigned baseSrcType = packedType2baseType(stype);
    unsigned typeDim     = getPackedDstDim(stype, packing);

    // NB: operations with 's' packing control must preserve all elements
    // except for the lowest one, but this cannot be emulated.
    // So we just erase everything before emulation.
    Val dst(type, b128(0, 0));

    for (unsigned idx = 0; idx < typeDim; ++idx)
    {
        Val res;

        Val x1 = arg1.getPackedElement(idx, packing, 0);
        Val x2 = arg2.getPackedElement(idx, packing, 1);

        if (opcode == BRIG_OPCODE_SHL || opcode == BRIG_OPCODE_SHR) // Mask out insignificant shift bits for shl/shr
        {
            assert(x2.getType() == BRIG_TYPE_U32);

            unsigned elementSize = getBrigTypeNumBits(type) / typeDim;
            x2 = Val(BRIG_TYPE_U32, x2.u32() & getRangeMask(elementSize));
        }

        if (opcode == BRIG_OPCODE_MULHI) res = emulateMulHiPacked(  type,        baseType,          x1, x2);
        else if (isSatPacking(packing))  res = emulateSat(opcode,   type,                           x1, x2);
        else if (InstBasic i = inst)     res = emulateMod(opcode,   baseType,    AluMod(i),         x1, x2);
        else if (InstMod   i = inst)     res = emulateMod(opcode,   baseType,    AluMod(i.round()), x1, x2);
        else if (InstCmp   i = inst)     res = emulateCmp(baseType, baseSrcType, i.compare(),       x1, x2);
        else                             { assert(false); }

        if (res.empty())
        {
            assert(idx == 0 || 
                   (opcode == BRIG_OPCODE_CMP && isFloatPackedType(stype))); // Non-zero index is possible with signaling comparison (NaN may be in eny element)
            return unimplemented();
        }

        dst.setPackedElement(idx, res);
    }

    return dst;
}

static Val emulateDstValPackedSpecial(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    using namespace Brig;

    switch (inst.opcode())
    {
    // Packed Data Operations
    case BRIG_OPCODE_SHUFFLE:   return emulate_shuffle   (inst.type(),                    arg1, arg2, arg3);
    case BRIG_OPCODE_UNPACKHI:  return emulate_unpackHalf(inst.type(), false,             arg1, arg2);
    case BRIG_OPCODE_UNPACKLO:  return emulate_unpackHalf(inst.type(), true,              arg1, arg2);
    case BRIG_OPCODE_PACK:      return emulate_pack      (inst.type(), getSrcType(inst),  arg1, arg2, arg3);
    case BRIG_OPCODE_UNPACK:    return emulate_unpack    (inst.type(), getSrcType(inst),  arg1, arg2);
    case BRIG_OPCODE_CMOV:      return emulate_cmov      (inst.type(),                    arg1, arg2, arg3);

    // Multimedia Operations
    case BRIG_OPCODE_PACKCVT:   return emulate_packcvt   (inst.type(), getSrcType(inst),  arg1, arg2, arg3, arg4);
    case BRIG_OPCODE_UNPACKCVT: return emulate_unpackcvt (inst.type(), getSrcType(inst),  arg1, arg2);
    case BRIG_OPCODE_LERP:      return emulate_lerp      (inst.type(),                    arg1, arg2, arg3);
    case BRIG_OPCODE_SAD:       return emulate_sad       (inst.type(), getSrcType(inst),  arg1, arg2, arg3);
    case BRIG_OPCODE_SADHI:     return emulate_sadhi     (inst.type(), getSrcType(inst),  arg1, arg2, arg3);

    default:
        return emulationFailed();
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Emulation of common (non-packed) operations

static Val emulateDstValCommon(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    if      (InstBasic      i = inst)  return emulateMod(i.opcode(),        i.type(), AluMod(i),         arg1, arg2, arg3, arg4);
    else if (InstMod        i = inst)  return emulateMod(i.opcode(),        i.type(), AluMod(i.round()), arg1, arg2, arg3, arg4);
    else if (InstCmp        i = inst)  return emulateCmp(                   i.type(), i.sourceType(), i.compare(), arg1, arg2);
    else if (InstCvt        i = inst)  return emulateCvt(                   i.type(), i.sourceType(), AluMod(i.round()), arg1);
    else if (InstSourceType i = inst)  return emulateSourceType(i.opcode(), i.type(), i.sourceType(), arg1, arg2, arg3);
    else if (InstAtomic     i = inst)  return emulateAtomicDst(inst.opcode(), arg1);
    else if (InstMem        i = inst)  return emulateMemDst(i.segment(), i.opcode(), arg1);
    else                               return emulationFailed();
}

//=============================================================================
//=============================================================================
//=============================================================================
// Public interface with Emulator

// Check generic limitations on instruction being tested
// NB: Most limitations should be encoded in HSAILTestGenTestData.h
//     This function shall only check limitations which cannot be expressed there
bool testableInst(Inst inst)
{
    using namespace Brig;
    assert(inst);

    if (InstAtomic instAtomic = inst)
    {
        if (!isSupportedSegment(instAtomic.segment())) return false;
        if (instAtomic.equivClass() != 0) return false;
        //if (instAtomic.memoryOrder() == ...) return false;
        //if (instAtomic.memoryScope() == ...) return false;
    }
    else if (InstMem instMem = inst)
    {
        if (instMem.type() == BRIG_TYPE_B128 && OperandOperandList(inst.operand(0))) return false; //F1.0
        if (!isSupportedSegment(instMem.segment())) return false;
        if (instMem.width() != BRIG_WIDTH_NONE && instMem.width() != BRIG_WIDTH_1) return false;
        if (instMem.modifier().isConst()) return false;
        if (instMem.equivClass() != 0) return false;
    }
    else if (InstCvt instCvt = inst)
    {
        // Saturating signalign rounding is badly defined in spec; the behavior is unclear
        unsigned rounding = instCvt.round();
        return !(isSatRounding(rounding) && isSignalingRounding(rounding));
    }

    return true;
}

// Emulate execution of instruction 'inst' using provided input values.
// Return value stored into destination register or an empty value
// if there is no destination or if emulation failed.
Val emulateDstVal(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    Val res;

    bool ftz = emulateFtz(inst, arg0, arg1, arg2, arg3, arg4);

    if (isCommonPacked(inst))           // regular operations on packed data
    {
        res = emulateDstValPackedRegular(inst, arg0, arg1, arg2, arg3, arg4);
    }
    else if (isSpecialPacked(inst))     // irregular operations on packed data
    {
        res = emulateDstValPackedSpecial(inst, arg0, arg1, arg2, arg3, arg4);
    }
    else                                // operations with non-packed data types
    {
        res = emulateDstValCommon(inst, arg0, arg1, arg2, arg3, arg4);
    }

    if (ftz) res = res.ftz();
    return res.normalize(discardNanSign(inst.opcode())); // Clear NaN payload and sign
}

// Emulate execution of instruction 'inst' using provided input values.
// Return value stored into memory or an empty value if this
// instruction does not modify memory or if emulation failed.
Val emulateMemVal(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4)
{
    using namespace Brig;

    Val res;

    if (InstAtomic i = inst)
    {
        switch (i.opcode())
        {
        case BRIG_OPCODE_ATOMIC:      res = emulateAtomicMem(i.type(), i.atomicOperation(), arg1, arg2, arg3); break;
        case BRIG_OPCODE_ATOMICNORET: res = emulateAtomicMem(i.type(), i.atomicOperation(), arg0, arg1, arg2); break;
        default:                      res = emulationFailed();                                                 break;
        }
    }
    else if (InstMem i = inst)
    {
        res = emulateMemMem(i.segment(), i.opcode(), arg0, arg1);
    }
    else
    {
        res = emptyMemValue();
    }

    return res;
}

// Return precision of result computation for this instruction.
// If the value is 0, the precision is infinite.
// If the value is between 0 and 1, the precision is relative.
// If the value is greater than or equals to 1, the precision is specified in ULPS.
// This is a property of target HW, not emulator!
double getPrecision(Inst inst)
{
    using namespace Brig;

    switch(inst.opcode()) // Instructions with HW-specific precision
    {
    case BRIG_OPCODE_NRCP:
    case BRIG_OPCODE_NSQRT:
    case BRIG_OPCODE_NRSQRT:
    case BRIG_OPCODE_NEXP2:
    case BRIG_OPCODE_NLOG2:
    case BRIG_OPCODE_NSIN:
    case BRIG_OPCODE_NCOS:
    case BRIG_OPCODE_NFMA:
        return getNativeOpPrecision(inst.opcode(), inst.type());

    default:
        return 1; // 0.5 ULP (infinite precision)
    }
}

//=============================================================================
//=============================================================================
//=============================================================================

} // namespace TESTGEN
