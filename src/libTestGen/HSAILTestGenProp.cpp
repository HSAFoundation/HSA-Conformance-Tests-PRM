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

#include "HSAILTestGenProp.h"
#include "HSAILTestGenPropDesc.h"
#include "HSAILValidatorBase.h"
#include "HSAILTestGenBrigContext.h"
#include "HSAILTestGenUtilities.h"
#include "HSAILTestGenEmulatorTypes.h"

#include <algorithm>

using namespace HSAIL_ASM;

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================
// Mappings of abstract HDL values of extended properties to actual Brig values

// FIXME: ADD "HDL" PREFIX FOR HDL-GENERATED VALUES (THINK OVER UNIFICATION OF PREFIXES FOR HDL AND TGEN)
static const unsigned valMapDesc[] = // from HDL to TestGen
{
// FIXME: note that keys in this map may be used as indices to an array that hold expanded values (??? use array instead of map???)

    OPERAND_VAL_NULL,       O_NULL, 0,

    OPERAND_VAL_REG,        O_CREG, O_SREG, O_DREG, O_QREG, 0,

    OPERAND_VAL_VEC_2,      O_VEC2_R32_SRC,     O_VEC2_R64_SRC,     O_VEC2_R128_SRC,
                            O_VEC2_I_U8_SRC,    O_VEC2_I_S8_SRC,  
                            O_VEC2_M_U8_SRC,    O_VEC2_M_S8_SRC,  
                            O_VEC2_I_U16_SRC,   O_VEC2_I_S16_SRC,   O_VEC2_I_F16_SRC,
                            O_VEC2_M_U16_SRC,   O_VEC2_M_S16_SRC,   O_VEC2_M_F16_SRC,
                            O_VEC2_I_U32_SRC,   O_VEC2_I_S32_SRC,   O_VEC2_I_F32_SRC,
                            O_VEC2_M_U32_SRC,   O_VEC2_M_S32_SRC,   O_VEC2_M_F32_SRC,
                            O_VEC2_I_U64_SRC,   O_VEC2_I_S64_SRC,   O_VEC2_I_F64_SRC,
                            O_VEC2_M_U64_SRC,   O_VEC2_M_S64_SRC,   O_VEC2_M_F64_SRC,
                            O_VEC2_I_B128_SRC,  O_VEC2_I_B128_SRC,  O_VEC2_I_B128_SRC,
                            O_VEC2_M_B128_SRC,  O_VEC2_M_B128_SRC,  O_VEC2_M_B128_SRC,
                            O_VEC2_R32_DST,     O_VEC2_R64_DST,     O_VEC2_R128_DST,
                            O_VEC2_SIG32_SRC,
                            O_VEC2_SIG64_SRC,
                            0,

    OPERAND_VAL_VEC_3,      O_VEC3_R32_SRC,     O_VEC3_R64_SRC,     O_VEC3_R128_SRC,
                            O_VEC3_I_U8_SRC,    O_VEC3_I_S8_SRC,  
                            O_VEC3_M_U8_SRC,    O_VEC3_M_S8_SRC,  
                            O_VEC3_I_U16_SRC,   O_VEC3_I_S16_SRC,   O_VEC3_I_F16_SRC,
                            O_VEC3_M_U16_SRC,   O_VEC3_M_S16_SRC,   O_VEC3_M_F16_SRC,
                            O_VEC3_I_U32_SRC,   O_VEC3_I_S32_SRC,   O_VEC3_I_F32_SRC,
                            O_VEC3_M_U32_SRC,   O_VEC3_M_S32_SRC,   O_VEC3_M_F32_SRC,
                            O_VEC3_I_U64_SRC,   O_VEC3_I_S64_SRC,   O_VEC3_I_F64_SRC,
                            O_VEC3_M_U64_SRC,   O_VEC3_M_S64_SRC,   O_VEC3_M_F64_SRC,
                            O_VEC3_I_B128_SRC,  O_VEC3_I_B128_SRC,  O_VEC3_I_B128_SRC,
                            O_VEC3_M_B128_SRC,  O_VEC3_M_B128_SRC,  O_VEC3_M_B128_SRC,
                            O_VEC3_R32_DST,     O_VEC3_R64_DST,     O_VEC3_R128_DST,
                            O_VEC3_SIG32_SRC,
                            O_VEC3_SIG64_SRC,
                            0,

    OPERAND_VAL_VEC_4,      O_VEC4_R32_SRC,     O_VEC4_R64_SRC,     O_VEC4_R128_SRC,
                            O_VEC4_I_U8_SRC,    O_VEC4_I_S8_SRC,  
                            O_VEC4_M_U8_SRC,    O_VEC4_M_S8_SRC,  
                            O_VEC4_I_U16_SRC,   O_VEC4_I_S16_SRC,   O_VEC4_I_F16_SRC,
                            O_VEC4_M_U16_SRC,   O_VEC4_M_S16_SRC,   O_VEC4_M_F16_SRC,
                            O_VEC4_I_U32_SRC,   O_VEC4_I_S32_SRC,   O_VEC4_I_F32_SRC,
                            O_VEC4_M_U32_SRC,   O_VEC4_M_S32_SRC,   O_VEC4_M_F32_SRC,
                            O_VEC4_I_U64_SRC,   O_VEC4_I_S64_SRC,   O_VEC4_I_F64_SRC,
                            O_VEC4_M_U64_SRC,   O_VEC4_M_S64_SRC,   O_VEC4_M_F64_SRC,
                            O_VEC4_I_B128_SRC,  O_VEC4_I_B128_SRC,  O_VEC4_I_B128_SRC,
                            O_VEC4_M_B128_SRC,  O_VEC4_M_B128_SRC,  O_VEC4_M_B128_SRC,
                            O_VEC4_R32_DST,     O_VEC4_R64_DST,     O_VEC4_R128_DST,
                            O_VEC4_SIG32_SRC,
                            O_VEC4_SIG64_SRC,
                            0,

    OPERAND_VAL_IMM,        O_IMM_U8,       O_IMM_S8,
                            O_IMM_U16,      O_IMM_S16,      O_IMM_F16, 
                            O_IMM_U32,      O_IMM_S32,      O_IMM_F32, 
                            O_IMM_U64,      O_IMM_S64,      O_IMM_F64, 
                           
                            O_IMM_U8X4,     O_IMM_S8X4,     O_IMM_U16X2,    O_IMM_S16X2,    O_IMM_F16X2,
                            O_IMM_U8X8,     O_IMM_S8X8,     O_IMM_U16X4,    O_IMM_S16X4,    O_IMM_F16X4,    O_IMM_U32X2,    O_IMM_S32X2,    O_IMM_F32X2,
                            O_IMM_U8X16,    O_IMM_S8X16,    O_IMM_U16X8,    O_IMM_S16X8,    O_IMM_F16X8,    O_IMM_U32X4,    O_IMM_S32X4,    O_IMM_F32X4,    O_IMM_U64X2,    O_IMM_S64X2,    O_IMM_F64X2,

                            O_IMM_SIG32,    
                            O_IMM_SIG64,
                            
                            O_WAVESIZE, 
                            0,

    OPERAND_VAL_CNST,       O_IMM_U8,       O_IMM_S8,
                            O_IMM_U16,      O_IMM_S16,      O_IMM_F16, 
                            O_IMM_U32,      O_IMM_S32,      O_IMM_F32, 
                            O_IMM_U64,      O_IMM_S64,      O_IMM_F64, 
                            
                            0,

    OPERAND_VAL_LAB,        O_LABELREF, 0,

    OPERAND_VAL_ADDR,       O_ADDRESS_FLAT_DREG, O_ADDRESS_FLAT_OFF, O_ADDRESS_FLAT_SREG,
                            O_ADDRESS_GLOBAL_VAR, O_ADDRESS_READONLY_VAR, O_ADDRESS_GROUP_VAR, O_ADDRESS_PRIVATE_VAR,
                            O_ADDRESS_GLOBAL_ROIMG, O_ADDRESS_GLOBAL_WOIMG, O_ADDRESS_GLOBAL_RWIMG, O_ADDRESS_GLOBAL_SAMP, O_ADDRESS_GLOBAL_SIG32, O_ADDRESS_GLOBAL_SIG64,
                            O_ADDRESS_READONLY_ROIMG, O_ADDRESS_READONLY_RWIMG, O_ADDRESS_READONLY_SAMP, O_ADDRESS_READONLY_SIG32, O_ADDRESS_READONLY_SIG64, 0,

    OPERAND_VAL_FUNC,       O_FUNCTIONREF, O_IFUNCTIONREF, 0,

    OPERAND_VAL_IFUNC,      O_IFUNCTIONREF, 0,

    OPERAND_VAL_KERNEL,     O_KERNELREF, 0,
    OPERAND_VAL_SIGNATURE,  O_SIGNATUREREF, 0,

    OPERAND_VAL_ARGLIST,    0,
    OPERAND_VAL_JUMPTAB,    0,
    OPERAND_VAL_CALLTAB,    0,
    OPERAND_VAL_FBARRIER,   O_FBARRIERREF, 0,

    OPERAND_VAL_IMM0T2,     O_IMM_U32_0, O_IMM_U32_1, O_IMM_U32_2, 0,
    OPERAND_VAL_IMM0T3,     O_IMM_U32_0, O_IMM_U32_1, O_IMM_U32_2, O_IMM_U32_3, 0,

    OPERAND_VAL_INVALID,    0,

    EQCLASS_VAL_0,          EQCLASS_0, 0,
    EQCLASS_VAL_ANY,        EQCLASS_0, EQCLASS_1, EQCLASS_2, EQCLASS_255, 0,
    EQCLASS_VAL_INVALID,    0,
};

const unsigned* getValMapDesc(unsigned* size) { *size = sizeof(valMapDesc) / sizeof(unsigned); return valMapDesc; };

//=============================================================================
//=============================================================================
//=============================================================================

unsigned operandId2SymId(unsigned operandId)
{
    switch(operandId)
    {
    case O_ADDRESS_GLOBAL_VAR:      return SYM_GLOBAL_VAR;
    case O_ADDRESS_READONLY_VAR:    return SYM_READONLY_VAR;
    case O_ADDRESS_GROUP_VAR:       return SYM_GROUP_VAR;
    case O_ADDRESS_PRIVATE_VAR:     return SYM_PRIVATE_VAR;

    case O_ADDRESS_GLOBAL_ROIMG:    return SYM_GLOBAL_ROIMG;
    case O_ADDRESS_READONLY_ROIMG:  return SYM_READONLY_ROIMG;
    case O_ADDRESS_GLOBAL_RWIMG:    return SYM_GLOBAL_RWIMG;
    case O_ADDRESS_READONLY_RWIMG:  return SYM_READONLY_RWIMG;
    case O_ADDRESS_GLOBAL_WOIMG:    return SYM_GLOBAL_WOIMG;

    case O_ADDRESS_GLOBAL_SAMP:     return SYM_GLOBAL_SAMP;
    case O_ADDRESS_READONLY_SAMP:   return SYM_READONLY_SAMP;

    case O_ADDRESS_GLOBAL_SIG32:    return SYM_GLOBAL_SIG32;
    case O_ADDRESS_READONLY_SIG32:  return SYM_READONLY_SIG32;
    case O_ADDRESS_GLOBAL_SIG64:    return SYM_GLOBAL_SIG64;
    case O_ADDRESS_READONLY_SIG64:  return SYM_READONLY_SIG64;

    case O_FBARRIERREF:             return SYM_FBARRIER;
    case O_FUNCTIONREF:             return SYM_FUNC;
    case O_IFUNCTIONREF:            return SYM_IFUNC;
    case O_KERNELREF:               return SYM_KERNEL;
    case O_SIGNATUREREF:            return SYM_SIGNATURE;
    case O_LABELREF:                return SYM_LABEL;

    default:                        return SYM_NONE;
    }
}

bool isSupportedOperand(unsigned oprId)
{
    unsigned symId = operandId2SymId(oprId);
    return symId == SYM_NONE || isSupportedSym(symId);
}

const SymDesc symDescTab[SYM_MAXID] =
{
    {},

    {SYM_FUNC,           "&TestFunc",      BRIG_TYPE_NONE,   0,                               BRIG_SEGMENT_NONE},
    {SYM_IFUNC,          "&TestIndirFunc", BRIG_TYPE_NONE,   0,                               BRIG_SEGMENT_NONE},
    {SYM_KERNEL,         "&TestKernel",    BRIG_TYPE_NONE,   0,                               BRIG_SEGMENT_NONE},
    {SYM_SIGNATURE,      "&TestSignature", BRIG_TYPE_NONE,   0,                               BRIG_SEGMENT_NONE},
    {SYM_GLOBAL_VAR,     "&GlobalVar",     BRIG_TYPE_S32,    TEST_ARRAY_SIZE / sizeof(u32_t), BRIG_SEGMENT_GLOBAL},
    {SYM_GROUP_VAR,      "&GroupVar",      BRIG_TYPE_S32,    TEST_ARRAY_SIZE / sizeof(u32_t), BRIG_SEGMENT_GROUP},
    {SYM_PRIVATE_VAR,    "&PrivateVar",    BRIG_TYPE_S32,    TEST_ARRAY_SIZE / sizeof(u32_t), BRIG_SEGMENT_PRIVATE},
    {SYM_READONLY_VAR,   "&ReadonlyVar",   BRIG_TYPE_S32,    TEST_ARRAY_SIZE / sizeof(u32_t), BRIG_SEGMENT_READONLY},
    {SYM_GLOBAL_ROIMG,   "&GlobalROImg",   BRIG_TYPE_ROIMG,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_GLOBAL},
    {SYM_GLOBAL_WOIMG,   "&GlobalWOImg",   BRIG_TYPE_WOIMG,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_GLOBAL},
    {SYM_GLOBAL_RWIMG,   "&GlobalRWImg",   BRIG_TYPE_RWIMG,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_GLOBAL},
    {SYM_READONLY_ROIMG, "&ReadonlyROImg", BRIG_TYPE_ROIMG,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_READONLY},
    {SYM_READONLY_RWIMG, "&ReadonlyRWImg", BRIG_TYPE_RWIMG,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_READONLY},
    {SYM_GLOBAL_SAMP,    "&GlobalSamp",    BRIG_TYPE_SAMP,   TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_GLOBAL},
    {SYM_READONLY_SAMP,  "&ReadonlySamp",  BRIG_TYPE_SAMP,   TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_READONLY},
    {SYM_GLOBAL_SIG32,   "&GlobalSig32",   BRIG_TYPE_SIG32,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_GLOBAL},
    {SYM_READONLY_SIG32, "&ReadonlySig32", BRIG_TYPE_SIG32,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_READONLY},
    {SYM_GLOBAL_SIG64,   "&GlobalSig64",   BRIG_TYPE_SIG64,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_GLOBAL},
    {SYM_READONLY_SIG64, "&ReadonlySig64", BRIG_TYPE_SIG64,  TEST_ARRAY_SIZE / sizeof(u64_t), BRIG_SEGMENT_READONLY},
    {SYM_FBARRIER,       "&Fbarrier",      BRIG_TYPE_NONE,   0,                               BRIG_SEGMENT_NONE},
    {SYM_LABEL,          "@TestLabel",     BRIG_TYPE_NONE,   0,                               BRIG_SEGMENT_NONE},

};

const char* getSymName(unsigned symId)    { assert(SYM_MINID < symId && symId < SYM_MAXID && symDescTab[symId].id == symId); return symDescTab[symId].name;    }
unsigned    getSymType(unsigned symId)    { assert(SYM_MINID < symId && symId < SYM_MAXID && symDescTab[symId].id == symId); return symDescTab[symId].type;    }
unsigned    getSymDim(unsigned symId)     { assert(SYM_MINID < symId && symId < SYM_MAXID && symDescTab[symId].id == symId); return symDescTab[symId].dim;     }
unsigned    getSymSegment(unsigned symId) { assert(SYM_MINID < symId && symId < SYM_MAXID && symDescTab[symId].id == symId); return symDescTab[symId].segment; }

bool isSupportedSym(unsigned symId)
{
    assert(SYM_MINID < symId && symId < SYM_MAXID);
    return validateProp(PROP_TYPE, getSymType(symId), BrigSettings::getModel(), BrigSettings::getProfile(), BrigSettings::imgInstEnabled()) == 0;
}

//=============================================================================
//=============================================================================
//=============================================================================

string prop2str(unsigned id)
{
    return PropValidator::prop2str(id);
}

string operand2str(unsigned operandId)
{
    switch(operandId)
    {
    case O_NULL:          return "none";

    case O_CREG:          return "$c0";
    case O_SREG:          return "$s0";
    case O_DREG:          return "$d0";
    case O_QREG:          return "$q0";

    case O_VEC2_R32_SRC:  return "($s0, $s0)";
    case O_VEC3_R32_SRC:  return "($s0, $s0, $s0)";
    case O_VEC4_R32_SRC:  return "($s0, $s0, $s0, $s0)";
    case O_VEC2_R64_SRC:  return "($d0, $d0)";
    case O_VEC3_R64_SRC:  return "($d0, $d0, $d0)";
    case O_VEC4_R64_SRC:  return "($d0, $d0, $d0, $d0)";
    case O_VEC2_R128_SRC: return "($q0, $q0)";
    case O_VEC3_R128_SRC: return "($q0, $q0, $q0)";
    case O_VEC4_R128_SRC: return "($q0, $q0, $q0, $q0)";

    case O_VEC2_I_U8_SRC: return "(WS, IMM#u8)";
    case O_VEC3_I_U8_SRC: return "(WS, IMM#u8, IMM#u8)";
    case O_VEC4_I_U8_SRC: return "(WS, IMM#u8, IMM#u8, IMM#u8)";
    case O_VEC2_M_U8_SRC: return "(IMM#u8, $s0)";
    case O_VEC3_M_U8_SRC: return "(IMM#u8, IMM#u8, $s0)";
    case O_VEC4_M_U8_SRC: return "(IMM#u8, IMM#u8, $s0, $s0)";

    case O_VEC2_I_S8_SRC: return "(WS, IMM#s8)";
    case O_VEC3_I_S8_SRC: return "(WS, IMM#s8, IMM#s8)";
    case O_VEC4_I_S8_SRC: return "(WS, IMM#s8, IMM#s8, IMM#s8)";
    case O_VEC2_M_S8_SRC: return "(IMM#s8, $s0)";
    case O_VEC3_M_S8_SRC: return "(IMM#s8, IMM#s8, $s0)";
    case O_VEC4_M_S8_SRC: return "(IMM#s8, IMM#s8, $s0, $s0)";


    case O_VEC2_I_U16_SRC:  return "(WS, IMM#u16)";
    case O_VEC3_I_U16_SRC:  return "(WS, IMM#u16, IMM#u16)";
    case O_VEC4_I_U16_SRC:  return "(WS, IMM#u16, IMM#u16, IMM#u16)";
    case O_VEC2_M_U16_SRC:  return "(IMM#u16, $s0)";
    case O_VEC3_M_U16_SRC:  return "(IMM#u16, IMM#u16, $s0)";
    case O_VEC4_M_U16_SRC:  return "(IMM#u16, IMM#u16, $s0, $s0)";

    case O_VEC2_I_S16_SRC:  return "(WS, IMM#s16)";
    case O_VEC3_I_S16_SRC:  return "(WS, IMM#s16, IMM#s16)";
    case O_VEC4_I_S16_SRC:  return "(WS, IMM#s16, IMM#s16, IMM#s16)";
    case O_VEC2_M_S16_SRC:  return "(IMM#s16, $s0)";
    case O_VEC3_M_S16_SRC:  return "(IMM#s16, IMM#s16, $s0)";
    case O_VEC4_M_S16_SRC:  return "(IMM#s16, IMM#s16, $s0, $s0)";

    case O_VEC2_I_F16_SRC:  return "(WS, IMM#f16)";
    case O_VEC3_I_F16_SRC:  return "(WS, IMM#f16, IMM#f16)";
    case O_VEC4_I_F16_SRC:  return "(WS, IMM#f16, IMM#f16, IMM#f16)";
    case O_VEC2_M_F16_SRC:  return "(IMM#f16, $s0)";
    case O_VEC3_M_F16_SRC:  return "(IMM#f16, IMM#f16, $s0)";
    case O_VEC4_M_F16_SRC:  return "(IMM#f16, IMM#f16, $s0, $s0)";


    case O_VEC2_I_U32_SRC:  return "(WS, IMM#u32)";
    case O_VEC3_I_U32_SRC:  return "(WS, IMM#u32, IMM#u32)";
    case O_VEC4_I_U32_SRC:  return "(WS, IMM#u32, IMM#u32, IMM#u32)";
    case O_VEC2_M_U32_SRC:  return "(IMM#u32, $s0)";
    case O_VEC3_M_U32_SRC:  return "(IMM#u32, IMM#u32, $s0)";
    case O_VEC4_M_U32_SRC:  return "(IMM#u32, IMM#u32, $s0, $s0)";

    case O_VEC2_I_S32_SRC:  return "(WS, IMM#s32)";
    case O_VEC3_I_S32_SRC:  return "(WS, IMM#s32, IMM#s32)";
    case O_VEC4_I_S32_SRC:  return "(WS, IMM#s32, IMM#s32, IMM#s32)";
    case O_VEC2_M_S32_SRC:  return "(IMM#s32, $s0)";
    case O_VEC3_M_S32_SRC:  return "(IMM#s32, IMM#s32, $s0)";
    case O_VEC4_M_S32_SRC:  return "(IMM#s32, IMM#s32, $s0, $s0)";

    case O_VEC2_I_F32_SRC:  return "(WS, IMM#f32)";
    case O_VEC3_I_F32_SRC:  return "(WS, IMM#f32, IMM#f32)";
    case O_VEC4_I_F32_SRC:  return "(WS, IMM#f32, IMM#f32, IMM#f32)";
    case O_VEC2_M_F32_SRC:  return "(IMM#f32, $s0)";
    case O_VEC3_M_F32_SRC:  return "(IMM#f32, IMM#f32, $s0)";
    case O_VEC4_M_F32_SRC:  return "(IMM#f32, IMM#f32, $s0, $s0)";


    case O_VEC2_I_U64_SRC:  return "(WS, IMM#u64)";
    case O_VEC3_I_U64_SRC:  return "(WS, IMM#u64, IMM#u64)";
    case O_VEC4_I_U64_SRC:  return "(WS, IMM#u64, IMM#u64, IMM#u64)";
    case O_VEC2_M_U64_SRC:  return "(IMM#u64, $d0)";
    case O_VEC3_M_U64_SRC:  return "(IMM#u64, IMM#u64, $d0)";
    case O_VEC4_M_U64_SRC:  return "(IMM#u64, IMM#u64, $d0, $d0)";

    case O_VEC2_I_S64_SRC:  return "(WS, IMM#s64)";
    case O_VEC3_I_S64_SRC:  return "(WS, IMM#s64, IMM#s64)";
    case O_VEC4_I_S64_SRC:  return "(WS, IMM#s64, IMM#s64, IMM#s64)";
    case O_VEC2_M_S64_SRC:  return "(IMM#s64, $d0)";
    case O_VEC3_M_S64_SRC:  return "(IMM#s64, IMM#s64, $d0)";
    case O_VEC4_M_S64_SRC:  return "(IMM#s64, IMM#s64, $d0, $d0)";

    case O_VEC2_I_F64_SRC:  return "(WS, IMM#f64)";
    case O_VEC3_I_F64_SRC:  return "(WS, IMM#f64, IMM#f64)";
    case O_VEC4_I_F64_SRC:  return "(WS, IMM#f64, IMM#f64, IMM#f64)";
    case O_VEC2_M_F64_SRC:  return "(IMM#f64, $d0)";
    case O_VEC3_M_F64_SRC:  return "(IMM#f64, IMM#f64, $d0)";
    case O_VEC4_M_F64_SRC:  return "(IMM#f64, IMM#f64, $d0, $d0)";


    case O_VEC2_I_B128_SRC:  return "(IMM#b128, IMM#b128)";
    case O_VEC3_I_B128_SRC:  return "(IMM#b128, IMM#b128, IMM#b128)";
    case O_VEC4_I_B128_SRC:  return "(IMM#b128, IMM#b128, IMM#b128, IMM#b128)";
    case O_VEC2_M_B128_SRC:  return "(IMM#b128, $d0)";
    case O_VEC3_M_B128_SRC:  return "(IMM#b128, IMM#b128, $d0)";
    case O_VEC4_M_B128_SRC:  return "(IMM#b128, IMM#b128, $d0, $d0)";


    case O_VEC2_R32_DST:  return "($s0, $s1)";
    case O_VEC3_R32_DST:  return "($s0, $s1, $s2)";
    case O_VEC4_R32_DST:  return "($s0, $s1, $s2, $s3)";
    case O_VEC2_R64_DST:  return "($d0, $d1)";
    case O_VEC3_R64_DST:  return "($d0, $d1, $d2)";
    case O_VEC4_R64_DST:  return "($d0, $d1, $d2, $d3)";
    case O_VEC2_R128_DST: return "($q0, $q1)";
    case O_VEC3_R128_DST: return "($q0, $q1, $q2)";
    case O_VEC4_R128_DST: return "($q0, $q1, $q2, $q3)";

    case O_VEC2_SIG32_SRC:  return "(sig32(0), $d1)";
    case O_VEC3_SIG32_SRC:  return "(sig32(0), sig32(0), $d2)";
    case O_VEC4_SIG32_SRC:  return "(sig32(0), sig32(0), sig32(0), $d3)";

    case O_VEC2_SIG64_SRC:  return "(sig64(0), $d1)";
    case O_VEC3_SIG64_SRC:  return "(sig64(0), sig64(0), $d2)";
    case O_VEC4_SIG64_SRC:  return "(sig64(0), sig64(0), sig64(0), $d3)";

    case O_IMM_U8:        return "IMM#u8";
    case O_IMM_S8:        return "IMM#s8";

    case O_IMM_U16:       return "IMM#u16";
    case O_IMM_S16:       return "IMM#s16";
    case O_IMM_F16:       return "IMM#f16";

    case O_IMM_U32:       return "IMM#u32";
    case O_IMM_S32:       return "IMM#s32";
    case O_IMM_F32:       return "IMM#f32";

    case O_IMM_U64:       return "IMM#u64";
    case O_IMM_S64:       return "IMM#s64";
    case O_IMM_F64:       return "IMM#f64";

    case O_IMM_U8X4:      return "IMM#u8x4";
    case O_IMM_S8X4:      return "IMM#s8x4";
    case O_IMM_U16X2:     return "IMM#u16x2";
    case O_IMM_S16X2:     return "IMM#s16x2";
    case O_IMM_F16X2:     return "IMM#f16x2";

    case O_IMM_U8X8:      return "IMM#u8x8";
    case O_IMM_S8X8:      return "IMM#s8x8";
    case O_IMM_U16X4:     return "IMM#u16x4";
    case O_IMM_S16X4:     return "IMM#s16x4";
    case O_IMM_F16X4:     return "IMM#f16x4";
    case O_IMM_U32X2:     return "IMM#u32x2";
    case O_IMM_S32X2:     return "IMM#s32x2";
    case O_IMM_F32X2:     return "IMM#f32x2";

    case O_IMM_U8X16:     return "IMM#u8x16";
    case O_IMM_S8X16:     return "IMM#s8x16";
    case O_IMM_U16X8:     return "IMM#u16x8";
    case O_IMM_S16X8:     return "IMM#s16x8";
    case O_IMM_F16X8:     return "IMM#f16x8";
    case O_IMM_U32X4:     return "IMM#u32x4";
    case O_IMM_S32X4:     return "IMM#s32x4";
    case O_IMM_F32X4:     return "IMM#f32x4";
    case O_IMM_U64X2:     return "IMM#u64x2";
    case O_IMM_S64X2:     return "IMM#s64x2";
    case O_IMM_F64X2:     return "IMM#f64x2";

    case O_IMM_U32_0:     return "0";
    case O_IMM_U32_1:     return "1";
    case O_IMM_U32_2:     return "2";
    case O_IMM_U32_3:     return "3";

    case O_IMM_SIG32:     return "IMM#SIG32";
    case O_IMM_SIG64:     return "IMM#SIG64";

    case O_WAVESIZE:      return "WAVESIZE";

    case O_LABELREF:
    case O_FUNCTIONREF:
    case O_IFUNCTIONREF:
    case O_KERNELREF:
    case O_SIGNATUREREF:
    case O_FBARRIERREF:   return getSymName(operandId2SymId(operandId));


    case O_ADDRESS_FLAT_DREG:           return "[$d0]";
    case O_ADDRESS_FLAT_SREG:           return "[$s0]";
    case O_ADDRESS_FLAT_OFF:            return "[0]";

    case O_ADDRESS_GLOBAL_VAR:
    case O_ADDRESS_READONLY_VAR:
    case O_ADDRESS_GROUP_VAR:
    case O_ADDRESS_PRIVATE_VAR:
    case O_ADDRESS_GLOBAL_ROIMG:
    case O_ADDRESS_GLOBAL_RWIMG:
    case O_ADDRESS_GLOBAL_WOIMG:
    case O_ADDRESS_GLOBAL_SAMP:
    case O_ADDRESS_GLOBAL_SIG32:
    case O_ADDRESS_GLOBAL_SIG64:
    case O_ADDRESS_READONLY_ROIMG:
    case O_ADDRESS_READONLY_RWIMG:
    case O_ADDRESS_READONLY_SAMP:
    case O_ADDRESS_READONLY_SIG32:
    case O_ADDRESS_READONLY_SIG64:      return string("[") + getSymName(operandId2SymId(operandId)) + "]";

    case O_JUMPTAB:                     return "[Jumptab]"; // currently unused
    case O_CALLTAB:                     return "[Calltab]"; // currently unused

    default: assert(false); return "";
    }
}

string eqclass2str(unsigned id)
{
    switch(id)
    {
    case EQCLASS_0:         return "0";
    case EQCLASS_1:         return "1";
    case EQCLASS_2:         return "2";
    case EQCLASS_255:       return "255";

    default: assert(false); return "";
    }
}

string val2str(unsigned id, unsigned val)
{
    if (isOperandProp(id)) // TestGen-specific
    {
        return operand2str(val);
    }
    else if (id == PROP_EQUIVCLASS)        // TestGen-specific
    {
        return eqclass2str(val);
    }
    else
    {
        return PropValidator::val2str(id, val);
    }
}

//=============================================================================
//=============================================================================
//=============================================================================

struct ImmOperandDetector { bool operator()(unsigned val) { return isImmOperandId(val); }};

// This is not a generic soultion but rather a hack.
// It is because removal of imm operands may cause TestGen fail finding valid combinations of opernads.
void Prop::tryRemoveImmOperands()
{
    vector<unsigned> copy = pValues;

    pValues.erase(std::remove_if(pValues.begin(), pValues.end(), ImmOperandDetector()), pValues.end());

    // There are instructions which accept imm operands only - for these keep operand list unchanged.
    if (pValues.empty()) pValues = copy;
}

void Prop::init(const unsigned* pVals, unsigned pValsNum, const unsigned* nVals, unsigned nValsNum)
{
    for (unsigned i = 0; i < pValsNum; ++i) appendPositive(pVals[i]);
    for (unsigned i = 0; i < nValsNum; ++i) appendNegative(nVals[i]); // NB: positive values may be excluded for neutral props

    //if (!OPT::enableImmOperands && isOperandProp(propId)) tryRemoveImmOperands();

    // This is to minimize deps from HDL-generated code
    std::sort(pValues.begin(), pValues.end());
    std::sort(nValues.begin(), nValues.end());
}

Prop* Prop::create(unsigned propId, const unsigned* pVals, unsigned pValsNum, const unsigned* nVals, unsigned nValsNum)
{
    Prop* prop;
    if (PropDesc::isBrigProp(propId))
    {
        prop = new Prop(propId);
    }
    else
    {
        prop = new ExtProp(propId);
    }
    prop->init(pVals, pValsNum, nVals, nValsNum);
    return prop;
}

map<unsigned, const unsigned*> ExtProp::valMap;

//=============================================================================
//=============================================================================
//=============================================================================

}; // namespace TESTGEN
