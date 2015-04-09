/*
   Copyright 2014 Heterogeneous System Architecture (HSA) Foundation

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

#ifndef HC_TEST_HELPER_HPP
#define HC_TEST_HELPER_HPP

#include "HexlTest.hpp"
#include "HsailRuntime.hpp"
#include "BrigEmitter.hpp"
#include "HCTests.hpp"
#include "Scenario.hpp"

using namespace hexl;
using namespace hexl::scenario;
using namespace hexl::emitter;
using namespace HSAIL_ASM;

namespace hsail_conformance {

//=====================================================================================

#define COND(x, cnd, y)       Cond(BRIG_COMPARE_##cnd, x, y)
#define STARTIF(x, cond, y) { string lab__ = IfCond(BRIG_COMPARE_##cond, x, y);
#define ENDIF                 EndIfCond(lab__); }

#define START_LOOP(cond)          if (cond) { be.EmitLabel("@LoopStart"); }
#define END_LOOP(cond, cReg)      if (cond) { EndWhile(cReg, "@LoopStart"); }

//=====================================================================================

class TestHelper : public Test
{
public:
    TestHelper(Location codeLocation, Grid geometry) : Test(codeLocation, geometry) {}

public:
    void Comment(string s)
    {
        s = "// " + s;
        be.Brigantine().addComment("//");
        be.Brigantine().addComment(s.c_str());
        be.Brigantine().addComment("//");
    }
    void Comment(string s0, string s1)
    {
        s0 = "// " + s0;
        s1 = "// " + s1;
        be.Brigantine().addComment("//");
        be.Brigantine().addComment(s0.c_str());
        be.Brigantine().addComment(s1.c_str());
        be.Brigantine().addComment("//");
    }

    OperandAddress TargetAddr(PointerReg addr, TypedReg index)
    {
        assert(isUnsignedType(addr->Type()));

        if (addr->TypeSizeBits() != index->TypeSizeBits())
        {
            TypedReg reg = be.AddTReg(addr->Type());
            EmitCvt(reg, index);
            index = reg;
        }

        PointerReg res = be.AddAReg(addr->Segment());
        EmitArith(BRIG_OPCODE_MAD, res, index, be.Immed(addr->Type(), getBrigTypeNumBytes(ResultType())), addr);
        return be.Address(res);
    }

    Inst AtomicInst(BrigType t, BrigAtomicOperation op, BrigMemoryOrder order, BrigMemoryScope scope, BrigSegment segment, uint8_t eqclass, bool ret = true)
    {
        switch(op)
        {
        case BRIG_ATOMIC_LD:
        case BRIG_ATOMIC_ST:
        case BRIG_ATOMIC_AND:
        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:
        case BRIG_ATOMIC_EXCH:
        case BRIG_ATOMIC_CAS:
            t = (BrigType)type2bitType(t);
            break;

        case BRIG_ATOMIC_ADD:
        case BRIG_ATOMIC_SUB:
        case BRIG_ATOMIC_MAX:
        case BRIG_ATOMIC_MIN:
            if (!isSignedType(t) && !isUnsignedType(t)) t = (BrigType)getUnsignedType(getBrigTypeNumBits(t));
            break;

        case BRIG_ATOMIC_WRAPINC:
        case BRIG_ATOMIC_WRAPDEC:
            t = (BrigType)getUnsignedType(getBrigTypeNumBits(t));
            break;

        default:
            assert(false);
        }

        InstAtomic inst = be.Brigantine().addInst<InstAtomic>(ret? BRIG_OPCODE_ATOMIC : BRIG_OPCODE_ATOMICNORET, t);
        inst.segment() = segment;
        inst.atomicOperation() = op;
        inst.memoryOrder() = order;
        inst.memoryScope() = scope;
        inst.equivClass() = eqclass;

        return inst;
    }

    void WaveBarrier()
    {
        InstBr inst = be.Brigantine().addInst<InstBr>(BRIG_OPCODE_WAVEBARRIER, BRIG_TYPE_NONE);
        inst.width() = BRIG_WIDTH_WAVESIZE;
        inst.operands() = ItemList();
    }

    void Barrier(bool isWaveBarrier = false)
    {
        if (isWaveBarrier) WaveBarrier(); else be.EmitBarrier();
    }

    void MemFence(BrigMemoryOrder memoryOrder, BrigMemoryScope memoryScope)
    {
        be.EmitMemfence(memoryOrder, memoryScope, memoryScope, BRIG_MEMORY_SCOPE_NONE);
    }

    TypedReg MinVal(TypedReg val, uint64_t max)
    {
        TypedReg res = be.AddTReg(val->Type());
        InstBasic inst = be.EmitArith(BRIG_OPCODE_MIN, res, val, be.Immed(val->Type(), max));
        if (isBitType(inst.type()))
        {
            inst.type() = getUnsignedType(getBrigTypeNumBits(inst.type()));
        }

        return res;
    }

    TypedReg Popcount(TypedReg src)
    {
        TypedReg dst = be.AddTReg(BRIG_TYPE_U32);
        InstSourceType inst = be.Brigantine().addInst<InstSourceType>(BRIG_OPCODE_POPCOUNT, BRIG_TYPE_U32);
        inst.sourceType() = type2bitType(src->Type());
        inst.operands() = be.Operands(dst->Reg(), src->Reg());
        return dst;
    }

    TypedReg TestAbsId(bool isLarge) { return be.EmitWorkitemFlatAbsId(isLarge); }

    TypedReg TestId(bool isLarge) 
    { 
        TypedReg id = be.EmitWorkitemFlatId();
        if (!isLarge) return id;
        
        TypedReg dest = be.AddTReg(BRIG_TYPE_U64);
        EmitCvt(dest, id);
        return dest;
    }

    TypedReg TestWgId(bool isLarge) 
    { 
        TypedReg id = be.EmitWorkgroupId(0);
        if (!isLarge) return id;
        
        TypedReg dest = be.AddTReg(BRIG_TYPE_U64);
        EmitCvt(dest, id);
        return dest;
    }

    // ========================================================================

    TypedReg Cond(unsigned cond, TypedReg val1, uint64_t val2)
    {
        TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
        InstCmp inst = be.EmitCmp(cReg->Reg(), val1, be.Immed(val1->Type(), val2), cond);
        if (inst.compare() != BRIG_COMPARE_EQ && inst.compare() != BRIG_COMPARE_NE && isBitType(inst.sourceType()))
        {
            inst.sourceType() = getUnsignedType(getBrigTypeNumBits(inst.sourceType()));
        }
        return cReg;
    }

    TypedReg Cond(unsigned cond, TypedReg val1, Operand val2)
    {
        TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
        InstCmp inst = be.EmitCmp(cReg->Reg(), val1, val2, cond);
        if (inst.compare() != BRIG_COMPARE_EQ && inst.compare() != BRIG_COMPARE_NE && isBitType(inst.sourceType()))
        {
            inst.sourceType() = getUnsignedType(getBrigTypeNumBits(inst.sourceType()));
        }
        return cReg;
    }
    
    TypedReg CondAssign(TypedReg x, TypedReg y, TypedReg cond)
    {
        assert(x->Type() == y->Type());
        TypedReg res = be.AddTReg(x->Type());
        EmitCMov(BRIG_OPCODE_CMOV, res, cond, x, y);
        return res;
    }
    
    TypedReg CondAssign(BrigType type, int64_t x, int64_t y, TypedReg cond)
    {
        assert(x != y);
        TypedReg res = be.AddTReg(type);
        EmitCMov(BRIG_OPCODE_CMOV, res, cond, be.Immed(type, x), be.Immed(type, y));
        return res;
    }
    
    TypedReg CondAssign(TypedReg res, int64_t x, int64_t y, TypedReg cond)
    {
        assert(x != y);
        EmitCMov(BRIG_OPCODE_CMOV, res, cond, be.Immed(res->Type(), x), be.Immed(res->Type(), y));
        return res;
    }
    
    TypedReg Not(TypedReg x)
    {
        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_NOT, res, x->Reg());
        return res;
    }
    
    TypedReg Or(TypedReg res, TypedReg x, TypedReg y)
    {
        assert(res->Type() == x->Type());
        be.EmitArith(BRIG_OPCODE_OR, res, x->Reg(), y->Reg());
        return res;
    }
    
    TypedReg Or(TypedReg x, TypedReg y)
    {
        assert(x->Type() == y->Type());

        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_OR, res, x->Reg(), y->Reg());
        return res;
    }
    
    TypedReg And(TypedReg x, TypedReg y)
    {
        assert(x->Type() == y->Type());

        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_AND, res, x->Reg(), y->Reg());
        return res;
    }
    
    TypedReg Add(TypedReg x, uint64_t y)
    {
        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_ADD, res, x->Reg(), be.Immed(x->Type(), y));
        return res;
    }
    
    TypedReg Sub(TypedReg x, Operand y)
    {
        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_SUB, res, x->Reg(), y);
        return res;
    }
    
    TypedReg Sub(TypedReg x, uint64_t y)
    {
        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_SUB, res, x->Reg(), be.Immed(x->Type(), y));
        return res;
    }
    
    TypedReg Sub(TypedReg res, TypedReg x, uint64_t y)
    {
        assert(res->Type() == x->Type());
        be.EmitArith(BRIG_OPCODE_SUB, res, x->Reg(), be.Immed(x->Type(), y));
        return res;
    }
    
    TypedReg Rem(TypedReg x, uint64_t y)
    {
        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_REM, res, x->Reg(), be.Immed(x->Type(), y));
        return res;
    }
    
    string IfCond(unsigned cond, TypedReg val1, uint64_t val2)
    {
        string label = be.AddLabel();
        TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
        be.EmitCmp(cReg->Reg(), val1, be.Immed(val1->Type(), val2), InvertCond(cond));
        be.EmitCbr(cReg, label);
    
        return label;
    }

    void EndIfCond(string label)
    {
        be.EmitLabel(label);
    }

    void EndWhile(TypedReg cond, string label)
    {
        be.EmitCbr(cond, label, BRIG_WIDTH_ALL);
    }

    static unsigned InvertCond(unsigned cond)
    {
        switch(cond)
        {
        case BRIG_COMPARE_EQ : return BRIG_COMPARE_NE;
        case BRIG_COMPARE_NE : return BRIG_COMPARE_EQ;
        case BRIG_COMPARE_GE : return BRIG_COMPARE_LT;
        case BRIG_COMPARE_LT : return BRIG_COMPARE_GE;
        case BRIG_COMPARE_GT : return BRIG_COMPARE_LE;
        case BRIG_COMPARE_LE : return BRIG_COMPARE_GT;
        default :
            assert(false);
            return cond;
        }
    }

    // ========================================================================

    static BrigType16_t ArithType(BrigOpcode16_t opcode, BrigType16_t operandType)
    {
        switch (opcode) {
        case BRIG_OPCODE_SHL:
        case BRIG_OPCODE_SHR:
        case BRIG_OPCODE_MAD:
        case BRIG_OPCODE_MUL:
        case BRIG_OPCODE_DIV:
        case BRIG_OPCODE_REM:
            return getUnsignedType(getBrigTypeNumBits(operandType));

        case BRIG_OPCODE_CMOV:
            return getBitType(getBrigTypeNumBits(operandType));

        default: return operandType;
        }
    }

    InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, TypedReg& src1)
    {
        assert(getBrigTypeNumBits(dst->Type()) == getBrigTypeNumBits(src0->Type()));
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1->Reg());
        return inst;
    }

    InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, Operand o)
    {
        assert(getBrigTypeNumBits(dst->Type()) == getBrigTypeNumBits(src0->Type()));
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), o);
        return inst;
    }

    InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, const TypedReg& src1, Operand o)
    {
        assert(getBrigTypeNumBits(dst->Type()) == getBrigTypeNumBits(src0->Type()));
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1->Reg(), o);
        return inst;
    }

    InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, Operand src1, const TypedReg& src2)
    {
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1, src2->Reg());
        return inst;
    }

    InstBasic EmitCMov(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, TypedReg& src1, const TypedReg& src2)
    {
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, dst->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1->Reg(), src2->Reg());
        return inst;
    }

    InstBasic EmitCMov(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, Operand src1, Operand src2)
    {
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, dst->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1, src2);
        return inst;
    }

    InstCvt EmitCvt(const TypedReg& dst, const TypedReg& src)
    {
        assert(isUnsignedType(dst->Type()));
        assert(dst->TypeSizeBits() != src->TypeSizeBits());

        InstCvt inst = be.Brigantine().addInst<InstCvt>(BRIG_OPCODE_CVT, dst->Type());
        inst.sourceType() = getUnsignedType(getBrigTypeNumBits(src->Type()));
        inst.operands() = be.Operands(dst->Reg(), src->Reg());
        return inst;
    }

    // ========================================================================

    static bool IsValidAtomic(BrigAtomicOperation op, BrigSegment segment, BrigMemoryOrder order, BrigMemoryScope scope, BrigType type, bool atomicNoRet)
    {
        if (!IsValidAtomicOp(op, atomicNoRet)) return false;
        if (!IsValidAtomicType(op, type)) return false;
        if (!IsValidAtomicOrder(op, order)) return false;
        if (!IsValidScope(segment, scope)) return false;
        return true;
    }

    static bool IsValidAtomicType(BrigAtomicOperation op, BrigType type)
    {
        switch (op)
        {
        case BRIG_ATOMIC_WRAPINC:
        case BRIG_ATOMIC_WRAPDEC:       return isUnsignedType(type);

        case BRIG_ATOMIC_SUB:
        case BRIG_ATOMIC_ADD:           return isSignedType(type) || isUnsignedType(type);

        case BRIG_ATOMIC_AND:
        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:           return isBitType(type);

        case BRIG_ATOMIC_MAX:
        case BRIG_ATOMIC_MIN:           return isSignedType(type) || isUnsignedType(type);

        case BRIG_ATOMIC_EXCH:
        case BRIG_ATOMIC_CAS:           return isBitType(type);

        case BRIG_ATOMIC_ST:            return isBitType(type);

        case BRIG_ATOMIC_LD:            return isBitType(type);

        default:                        
            assert(false);
            return false;
        }
    }

    static bool IsValidAtomicOrder(BrigAtomicOperation op, BrigMemoryOrder order)
    {
        if (op == BRIG_ATOMIC_ST) return IsValidStOrder(order);
        if (op == BRIG_ATOMIC_LD) return IsValidLdOrder(order);
        return true;
    }

    static bool IsValidAtomicOp(BrigAtomicOperation op, bool atomicNoRet)
    {
        switch (op)
        {
        case BRIG_ATOMIC_EXCH:
        case BRIG_ATOMIC_CAS:           
        case BRIG_ATOMIC_LD:            return !atomicNoRet;

        case BRIG_ATOMIC_ST:            return atomicNoRet;

        default:                        
            return true;
        }
    }

    static bool IsValidStOrder(BrigMemoryOrder order)
    {
        return order == BRIG_MEMORY_ORDER_SC_RELEASE || order == BRIG_MEMORY_ORDER_RELAXED;
    }

    static bool IsValidLdOrder(BrigMemoryOrder order)
    {
        return order == BRIG_MEMORY_ORDER_SC_ACQUIRE || order == BRIG_MEMORY_ORDER_RELAXED;
    }

    static bool IsValidScope(BrigSegment segment, BrigMemoryScope scope)
    {
        switch (segment)
        {
        case BRIG_SEGMENT_FLAT:
        case BRIG_SEGMENT_GLOBAL:
            return scope == BRIG_MEMORY_SCOPE_AGENT || 
                   scope == BRIG_MEMORY_SCOPE_SYSTEM || 
                   scope == BRIG_MEMORY_SCOPE_WORKGROUP || 
                   scope == BRIG_MEMORY_SCOPE_WAVEFRONT;

        case BRIG_SEGMENT_GROUP:
            return scope == BRIG_MEMORY_SCOPE_WORKGROUP || 
                   scope == BRIG_MEMORY_SCOPE_WAVEFRONT;

        default:
            assert(false);
            return false;
        }
    }
};

//=====================================================================================

}

#endif // HC_TEST_HELPER_HPP
