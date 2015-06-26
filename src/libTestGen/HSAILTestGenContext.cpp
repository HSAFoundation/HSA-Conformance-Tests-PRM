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

#include "HSAILTestGenContext.h"

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================

#define IMM_DEFAULT

#ifdef IMM_DEFAULT
static const uint64_t imm8_x   = 0x1;
static const uint64_t imm16_x  = 0xFFFFFFFFFFFFFFFFULL;
static const uint64_t imm32_x  = 0xFFFFFFFFFFFFFFFFULL;
static const uint64_t imm64_x  = 0xFFFFFFFFFFFFFFFFULL;
static const uint64_t imm128_h = 0x0;
static const uint64_t imm128_l = 7777777777777777777ULL;
#else
static const uint64_t imm1_x   = 0x1U;
static const uint64_t imm16_x  = 0x7FFFU;
static const uint64_t imm32_x  = 0x7FFFFFFFU;
static const uint64_t imm64_x  = 0x7FFFFFFFFFFFFFFFULL;
static const uint64_t imm128_h = 0x7FFFFFFFFFFFFFFFULL;
static const uint64_t imm128_l = 0xFFFFFFFFFFFFFFFFULL;
#endif

//=============================================================================
//=============================================================================
//=============================================================================

Operand Context::getOperand(unsigned oprId)
{
    assert(O_MINID < oprId && oprId < O_MAXID);

    if (isOperandCreated(oprId)) return operandTab[oprId];

    Operand opr = Operand();

    switch(oprId)
    {
    case O_NULL:          opr = Operand(&getContainer(), 0);       break; //F revise using and creating O_NULL

    case O_CREG:          opr = emitReg(1, 0);            break;
    case O_SREG:          opr = emitReg(32, 0);           break;
    case O_DREG:          opr = emitReg(64, 0);           break;
    case O_QREG:          opr = emitReg(128, 0);          break;

    case O_IMM_U8:        opr = emitImm(BRIG_TYPE_U8,   imm8_x );    break;
    case O_IMM_S8:        opr = emitImm(BRIG_TYPE_S8,   imm8_x );    break;

    case O_IMM_U16:       opr = emitImm(BRIG_TYPE_U16,  imm16_x);    break;
    case O_IMM_S16:       opr = emitImm(BRIG_TYPE_S16,  imm16_x);    break;
    case O_IMM_F16:       opr = emitImm(BRIG_TYPE_F16,  imm16_x);    break;

    case O_IMM_U32:       opr = emitImm(BRIG_TYPE_U32,  imm32_x);    break;
    case O_IMM_S32:       opr = emitImm(BRIG_TYPE_S32,  imm32_x);    break;
    case O_IMM_F32:       opr = emitImm(BRIG_TYPE_F32,  imm32_x);    break;

    case O_IMM_U64:       opr = emitImm(BRIG_TYPE_U64,  imm64_x);    break;
    case O_IMM_S64:       opr = emitImm(BRIG_TYPE_S64,  imm64_x);    break;
    case O_IMM_F64:       opr = emitImm(BRIG_TYPE_F64,  imm64_x);    break;

    case O_IMM_U8X4:      opr = emitImm(BRIG_TYPE_U8X4,  imm32_x);    break;
    case O_IMM_S8X4:      opr = emitImm(BRIG_TYPE_S8X4,  imm32_x);    break;
    case O_IMM_U16X2:     opr = emitImm(BRIG_TYPE_U16X2, imm32_x);    break;
    case O_IMM_S16X2:     opr = emitImm(BRIG_TYPE_S16X2, imm32_x);    break;
    case O_IMM_F16X2:     opr = emitImm(BRIG_TYPE_F16X2, imm32_x);    break;

    case O_IMM_U8X8:      opr = emitImm(BRIG_TYPE_U8X8,  imm64_x);    break;
    case O_IMM_S8X8:      opr = emitImm(BRIG_TYPE_S8X8,  imm64_x);    break;
    case O_IMM_U16X4:     opr = emitImm(BRIG_TYPE_U16X4, imm64_x);    break;
    case O_IMM_S16X4:     opr = emitImm(BRIG_TYPE_S16X4, imm64_x);    break;
    case O_IMM_F16X4:     opr = emitImm(BRIG_TYPE_F16X4, imm64_x);    break;
    case O_IMM_U32X2:     opr = emitImm(BRIG_TYPE_U32X2, imm64_x);    break;
    case O_IMM_S32X2:     opr = emitImm(BRIG_TYPE_S32X2, imm64_x);    break;
    case O_IMM_F32X2:     opr = emitImm(BRIG_TYPE_F32X2, imm64_x);    break;

    case O_IMM_U8X16:     opr = emitImm(BRIG_TYPE_U8X16, imm128_l, imm128_h); break;
    case O_IMM_S8X16:     opr = emitImm(BRIG_TYPE_S8X16, imm128_l, imm128_h); break;
    case O_IMM_U16X8:     opr = emitImm(BRIG_TYPE_U16X8, imm128_l, imm128_h); break;
    case O_IMM_S16X8:     opr = emitImm(BRIG_TYPE_S16X8, imm128_l, imm128_h); break;
    case O_IMM_F16X8:     opr = emitImm(BRIG_TYPE_F16X8, imm128_l, imm128_h); break;
    case O_IMM_U32X4:     opr = emitImm(BRIG_TYPE_U32X4, imm128_l, imm128_h); break;
    case O_IMM_S32X4:     opr = emitImm(BRIG_TYPE_S32X4, imm128_l, imm128_h); break;
    case O_IMM_F32X4:     opr = emitImm(BRIG_TYPE_F32X4, imm128_l, imm128_h); break;
    case O_IMM_U64X2:     opr = emitImm(BRIG_TYPE_U64X2, imm128_l, imm128_h); break;
    case O_IMM_S64X2:     opr = emitImm(BRIG_TYPE_S64X2, imm128_l, imm128_h); break;
    case O_IMM_F64X2:     opr = emitImm(BRIG_TYPE_F64X2, imm128_l, imm128_h); break;

    case O_IMM_U32_0:     opr = emitImm(BRIG_TYPE_U32,  0);          break;
    case O_IMM_U32_1:     opr = emitImm(BRIG_TYPE_U32,  1);          break;
    case O_IMM_U32_2:     opr = emitImm(BRIG_TYPE_U32,  2);          break;
    case O_IMM_U32_3:     opr = emitImm(BRIG_TYPE_U32,  3);          break;

    case O_IMM_SIG32:     opr = emitImm(BRIG_TYPE_SIG32, 0);         break;
    case O_IMM_SIG64:     opr = emitImm(BRIG_TYPE_SIG64, 0);         break;


    case O_VEC2_R32_SRC:  opr = emitVector(2, BRIG_TYPE_B32, false);    break;
    case O_VEC3_R32_SRC:  opr = emitVector(3, BRIG_TYPE_B32, false);    break;
    case O_VEC4_R32_SRC:  opr = emitVector(4, BRIG_TYPE_B32, false);    break;
    case O_VEC2_R64_SRC:  opr = emitVector(2, BRIG_TYPE_B64, false);    break;
    case O_VEC3_R64_SRC:  opr = emitVector(3, BRIG_TYPE_B64, false);    break;
    case O_VEC4_R64_SRC:  opr = emitVector(4, BRIG_TYPE_B64, false);    break;
    case O_VEC2_R128_SRC: opr = emitVector(2, BRIG_TYPE_B128, false);    break;
    case O_VEC3_R128_SRC: opr = emitVector(3, BRIG_TYPE_B128, false);    break;
    case O_VEC4_R128_SRC: opr = emitVector(4, BRIG_TYPE_B128, false);    break;

    case O_VEC2_I_U8_SRC: opr = emitVector(2, BRIG_TYPE_U8,  false, 2); break;
    case O_VEC3_I_U8_SRC: opr = emitVector(3, BRIG_TYPE_U8,  false, 3); break;
    case O_VEC4_I_U8_SRC: opr = emitVector(4, BRIG_TYPE_U8,  false, 4); break;
    case O_VEC2_M_U8_SRC: opr = emitVector(2, BRIG_TYPE_U8,  false, 1); break;
    case O_VEC3_M_U8_SRC: opr = emitVector(3, BRIG_TYPE_U8,  false, 2); break;
    case O_VEC4_M_U8_SRC: opr = emitVector(4, BRIG_TYPE_U8,  false, 2); break;

    case O_VEC2_I_S8_SRC: opr = emitVector(2, BRIG_TYPE_S8,  false, 2); break;
    case O_VEC3_I_S8_SRC: opr = emitVector(3, BRIG_TYPE_S8,  false, 3); break;
    case O_VEC4_I_S8_SRC: opr = emitVector(4, BRIG_TYPE_S8,  false, 4); break;
    case O_VEC2_M_S8_SRC: opr = emitVector(2, BRIG_TYPE_S8,  false, 1); break;
    case O_VEC3_M_S8_SRC: opr = emitVector(3, BRIG_TYPE_S8,  false, 2); break;
    case O_VEC4_M_S8_SRC: opr = emitVector(4, BRIG_TYPE_S8,  false, 2); break;


    case O_VEC2_I_U16_SRC:  opr = emitVector(2, BRIG_TYPE_U16, false, 2); break;
    case O_VEC3_I_U16_SRC:  opr = emitVector(3, BRIG_TYPE_U16, false, 3); break;
    case O_VEC4_I_U16_SRC:  opr = emitVector(4, BRIG_TYPE_U16, false, 4); break;
    case O_VEC2_M_U16_SRC:  opr = emitVector(2, BRIG_TYPE_U16, false, 1); break;
    case O_VEC3_M_U16_SRC:  opr = emitVector(3, BRIG_TYPE_U16, false, 2); break;
    case O_VEC4_M_U16_SRC:  opr = emitVector(4, BRIG_TYPE_U16, false, 2); break;

    case O_VEC2_I_S16_SRC:  opr = emitVector(2, BRIG_TYPE_S16, false, 2); break;
    case O_VEC3_I_S16_SRC:  opr = emitVector(3, BRIG_TYPE_S16, false, 3); break;
    case O_VEC4_I_S16_SRC:  opr = emitVector(4, BRIG_TYPE_S16, false, 4); break;
    case O_VEC2_M_S16_SRC:  opr = emitVector(2, BRIG_TYPE_S16, false, 1); break;
    case O_VEC3_M_S16_SRC:  opr = emitVector(3, BRIG_TYPE_S16, false, 2); break;
    case O_VEC4_M_S16_SRC:  opr = emitVector(4, BRIG_TYPE_S16, false, 2); break;

    case O_VEC2_I_F16_SRC:  opr = emitVector(2, BRIG_TYPE_F16, false, 2); break;
    case O_VEC3_I_F16_SRC:  opr = emitVector(3, BRIG_TYPE_F16, false, 3); break;
    case O_VEC4_I_F16_SRC:  opr = emitVector(4, BRIG_TYPE_F16, false, 4); break;
    case O_VEC2_M_F16_SRC:  opr = emitVector(2, BRIG_TYPE_F16, false, 1); break;
    case O_VEC3_M_F16_SRC:  opr = emitVector(3, BRIG_TYPE_F16, false, 2); break;
    case O_VEC4_M_F16_SRC:  opr = emitVector(4, BRIG_TYPE_F16, false, 2); break;


    case O_VEC2_I_U32_SRC:  opr = emitVector(2, BRIG_TYPE_U32, false, 2); break;
    case O_VEC3_I_U32_SRC:  opr = emitVector(3, BRIG_TYPE_U32, false, 3); break;
    case O_VEC4_I_U32_SRC:  opr = emitVector(4, BRIG_TYPE_U32, false, 4); break;
    case O_VEC2_M_U32_SRC:  opr = emitVector(2, BRIG_TYPE_U32, false, 1); break;
    case O_VEC3_M_U32_SRC:  opr = emitVector(3, BRIG_TYPE_U32, false, 2); break;
    case O_VEC4_M_U32_SRC:  opr = emitVector(4, BRIG_TYPE_U32, false, 2); break;

    case O_VEC2_I_S32_SRC:  opr = emitVector(2, BRIG_TYPE_S32, false, 2); break;
    case O_VEC3_I_S32_SRC:  opr = emitVector(3, BRIG_TYPE_S32, false, 3); break;
    case O_VEC4_I_S32_SRC:  opr = emitVector(4, BRIG_TYPE_S32, false, 4); break;
    case O_VEC2_M_S32_SRC:  opr = emitVector(2, BRIG_TYPE_S32, false, 1); break;
    case O_VEC3_M_S32_SRC:  opr = emitVector(3, BRIG_TYPE_S32, false, 2); break;
    case O_VEC4_M_S32_SRC:  opr = emitVector(4, BRIG_TYPE_S32, false, 2); break;

    case O_VEC2_I_F32_SRC:  opr = emitVector(2, BRIG_TYPE_F32, false, 2); break;
    case O_VEC3_I_F32_SRC:  opr = emitVector(3, BRIG_TYPE_F32, false, 3); break;
    case O_VEC4_I_F32_SRC:  opr = emitVector(4, BRIG_TYPE_F32, false, 4); break;
    case O_VEC2_M_F32_SRC:  opr = emitVector(2, BRIG_TYPE_F32, false, 1); break;
    case O_VEC3_M_F32_SRC:  opr = emitVector(3, BRIG_TYPE_F32, false, 2); break;
    case O_VEC4_M_F32_SRC:  opr = emitVector(4, BRIG_TYPE_F32, false, 2); break;


    case O_VEC2_I_U64_SRC:  opr = emitVector(2, BRIG_TYPE_U64, false, 2); break;
    case O_VEC3_I_U64_SRC:  opr = emitVector(3, BRIG_TYPE_U64, false, 3); break;
    case O_VEC4_I_U64_SRC:  opr = emitVector(4, BRIG_TYPE_U64, false, 4); break;
    case O_VEC2_M_U64_SRC:  opr = emitVector(2, BRIG_TYPE_U64, false, 1); break;
    case O_VEC3_M_U64_SRC:  opr = emitVector(3, BRIG_TYPE_U64, false, 2); break;
    case O_VEC4_M_U64_SRC:  opr = emitVector(4, BRIG_TYPE_U64, false, 2); break;

    case O_VEC2_I_S64_SRC:  opr = emitVector(2, BRIG_TYPE_S64, false, 2); break;
    case O_VEC3_I_S64_SRC:  opr = emitVector(3, BRIG_TYPE_S64, false, 3); break;
    case O_VEC4_I_S64_SRC:  opr = emitVector(4, BRIG_TYPE_S64, false, 4); break;
    case O_VEC2_M_S64_SRC:  opr = emitVector(2, BRIG_TYPE_S64, false, 1); break;
    case O_VEC3_M_S64_SRC:  opr = emitVector(3, BRIG_TYPE_S64, false, 2); break;
    case O_VEC4_M_S64_SRC:  opr = emitVector(4, BRIG_TYPE_S64, false, 2); break;

    case O_VEC2_I_F64_SRC:  opr = emitVector(2, BRIG_TYPE_F64, false, 2); break;
    case O_VEC3_I_F64_SRC:  opr = emitVector(3, BRIG_TYPE_F64, false, 3); break;
    case O_VEC4_I_F64_SRC:  opr = emitVector(4, BRIG_TYPE_F64, false, 4); break;
    case O_VEC2_M_F64_SRC:  opr = emitVector(2, BRIG_TYPE_F64, false, 1); break;
    case O_VEC3_M_F64_SRC:  opr = emitVector(3, BRIG_TYPE_F64, false, 2); break;
    case O_VEC4_M_F64_SRC:  opr = emitVector(4, BRIG_TYPE_F64, false, 2); break;


    case O_VEC2_I_B128_SRC: opr = emitVector(2, BRIG_TYPE_B128, false, 2); break;
    case O_VEC3_I_B128_SRC: opr = emitVector(3, BRIG_TYPE_B128, false, 3); break;
    case O_VEC4_I_B128_SRC: opr = emitVector(4, BRIG_TYPE_B128, false, 4); break;
    case O_VEC2_M_B128_SRC: opr = emitVector(2, BRIG_TYPE_B128, false, 1); break;
    case O_VEC3_M_B128_SRC: opr = emitVector(3, BRIG_TYPE_B128, false, 2); break;
    case O_VEC4_M_B128_SRC: opr = emitVector(4, BRIG_TYPE_B128, false, 2); break;


    case O_VEC2_R32_DST:    opr = emitVector(2, BRIG_TYPE_B32);           break;
    case O_VEC3_R32_DST:    opr = emitVector(3, BRIG_TYPE_B32);           break;
    case O_VEC4_R32_DST:    opr = emitVector(4, BRIG_TYPE_B32);           break;
    case O_VEC2_R64_DST:    opr = emitVector(2, BRIG_TYPE_B64);           break;
    case O_VEC3_R64_DST:    opr = emitVector(3, BRIG_TYPE_B64);           break;
    case O_VEC4_R64_DST:    opr = emitVector(4, BRIG_TYPE_B64);           break;
    case O_VEC2_R128_DST:   opr = emitVector(2, BRIG_TYPE_B128);          break;
    case O_VEC3_R128_DST:   opr = emitVector(3, BRIG_TYPE_B128);          break;
    case O_VEC4_R128_DST:   opr = emitVector(4, BRIG_TYPE_B128);          break;


    case O_VEC2_SIG32_SRC:  opr = emitVector(2, BRIG_TYPE_SIG32, false, 1); break;
    case O_VEC3_SIG32_SRC:  opr = emitVector(3, BRIG_TYPE_SIG32, false, 2); break;
    case O_VEC4_SIG32_SRC:  opr = emitVector(4, BRIG_TYPE_SIG32, false, 3); break;

    case O_VEC2_SIG64_SRC:  opr = emitVector(2, BRIG_TYPE_SIG64, false, 1); break;
    case O_VEC3_SIG64_SRC:  opr = emitVector(3, BRIG_TYPE_SIG64, false, 2); break;
    case O_VEC4_SIG64_SRC:  opr = emitVector(4, BRIG_TYPE_SIG64, false, 3); break;

    case O_WAVESIZE:        opr = emitWavesize();                           break;

    case O_ADDRESS_GLOBAL_DREG:    opr = emitAddrRef(DirectiveVariable(), emitReg(64, 0), BRIG_SEGMENT_GLOBAL);   break;
    case O_ADDRESS_READONLY_DREG:  opr = emitAddrRef(DirectiveVariable(), emitReg(64, 0), BRIG_SEGMENT_READONLY); break;
    case O_ADDRESS_GROUP_DREG:     opr = emitAddrRef(DirectiveVariable(), emitReg(64, 0), BRIG_SEGMENT_GROUP);    break;
    case O_ADDRESS_PRIVATE_DREG:   opr = emitAddrRef(DirectiveVariable(), emitReg(64, 0), BRIG_SEGMENT_PRIVATE);  break;
                            
    case O_ADDRESS_GLOBAL_SREG:    opr = emitAddrRef(DirectiveVariable(), emitReg(32, 0), BRIG_SEGMENT_GLOBAL);   break;
    case O_ADDRESS_READONLY_SREG:  opr = emitAddrRef(DirectiveVariable(), emitReg(32, 0), BRIG_SEGMENT_READONLY); break;
    case O_ADDRESS_GROUP_SREG:     opr = emitAddrRef(DirectiveVariable(), emitReg(32, 0), BRIG_SEGMENT_GROUP);    break;
    case O_ADDRESS_PRIVATE_SREG:   opr = emitAddrRef(DirectiveVariable(), emitReg(32, 0), BRIG_SEGMENT_PRIVATE);  break;

    case O_ADDRESS_OFFSET:         opr = emitAddrRef(0, true); break; // size does not matter because offset = 0

    case O_ADDRESS_GLOBAL_VAR:
    case O_ADDRESS_READONLY_VAR:
    case O_ADDRESS_GROUP_VAR:
    case O_ADDRESS_PRIVATE_VAR:
    case O_ADDRESS_SPILL_VAR:
    case O_ADDRESS_GLOBAL_ROIMG:
    case O_ADDRESS_GLOBAL_WOIMG:
    case O_ADDRESS_GLOBAL_RWIMG:
    case O_ADDRESS_GLOBAL_SAMP:
    case O_ADDRESS_GLOBAL_SIG32:
    case O_ADDRESS_GLOBAL_SIG64:
    case O_ADDRESS_READONLY_ROIMG:
    case O_ADDRESS_READONLY_RWIMG:
    case O_ADDRESS_READONLY_SAMP:
    case O_ADDRESS_READONLY_SIG32:
    case O_ADDRESS_READONLY_SIG64:
    case O_FUNCTIONREF:
    case O_IFUNCTIONREF:
    case O_KERNELREF:
    case O_FBARRIERREF:
    case O_SIGNATUREREF:
    case O_LABELREF:               opr = emitOperandRef(operandId2SymId(oprId)); break;

    case O_JUMPTAB: assert(false); break; // Currently not used
    case O_CALLTAB: assert(false); break; // Currently not used

    }

    operandTab[oprId] = opr;
    return opr;
}

Directive Context::emitSymbol(unsigned symId)
{
    assert(SYM_MINID < symId && symId < SYM_MAXID);

    const char* name = getSymName(symId);

    if (symId == SYM_FBARRIER)
    {
        return emitFBarrier(name);
    }
    else if (symId == SYM_FUNC)
    {
        DirectiveFunction fn = emitSbrStart(BRIG_KIND_DIRECTIVE_FUNCTION, name);
        startSbrBody();
        emitSbrEnd();
        return fn;
    }
    else if (symId == SYM_IFUNC)
    {
        DirectiveIndirectFunction fn = emitSbrStart(BRIG_KIND_DIRECTIVE_INDIRECT_FUNCTION, name);
        startSbrBody();
        emitSbrEnd();
        return fn;
    }
    else if (symId == SYM_KERNEL)
    {
        DirectiveKernel k = emitSbrStart(BRIG_KIND_DIRECTIVE_KERNEL, name);
        startSbrBody();
        emitSbrEnd();
        return k;
    }
    else if (symId == SYM_SIGNATURE)
    {
        DirectiveSignature sig = emitSbrStart(BRIG_KIND_DIRECTIVE_SIGNATURE, name);
        emitSbrEnd();
        return sig;
    }
    else
    {
        return BrigContext::emitSymbol(getSymType(symId), name, getSymSegment(symId), getSymDim(symId));
    }
}

//==============================================================================
//==============================================================================
//==============================================================================

void Context::genSymbol(unsigned symId)
{
    assert((SYM_MINID < symId && symId < SYM_MAXID) || symId == SYM_NONE);

    if (symId == SYM_NONE || symId == SYM_LABEL || !isSupportedSym(symId)) return;
    if (!symTab[symId]) symTab[symId] = emitSymbol(symId);
}

Operand Context::emitOperandRef(unsigned symId)
{
    assert(SYM_MINID < symId && symId < SYM_MAXID);
    assert(isSupportedSym(symId));

    switch(symId)
    {
    case SYM_LABEL:                           return emitLabelAndRef(getSymName(symId));
    case SYM_FUNC:     assert(symTab[symId]); return emitOperandCodeRef(symTab[symId]);
    case SYM_IFUNC:    assert(symTab[symId]); return emitOperandCodeRef(symTab[symId]);
    case SYM_KERNEL:   assert(symTab[symId]); return emitOperandCodeRef(symTab[symId]);
    case SYM_SIGNATURE:assert(symTab[symId]); return emitOperandCodeRef(symTab[symId]);
    case SYM_FBARRIER: assert(symTab[symId]); return emitOperandCodeRef(symTab[symId]);
    default:           assert(symTab[symId]); return emitAddrRef(DirectiveVariable(symTab[symId]));
    }
}

//==============================================================================
//==============================================================================
//==============================================================================

} // namespace TESTGEN
