/*
   Copyright 2014-2015 Heterogeneous System Architecture (HSA) Foundation

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

#ifndef HC_ATOMIC_TEST_HELPER_HPP
#define HC_ATOMIC_TEST_HELPER_HPP

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

#define LAB_NAME ("@LoopStart")

#define COND(x, cnd, y)       Cond(BRIG_COMPARE_##cnd, x, y)
#define STARTIF(x, cond, y) { string lab__ = IfCond(BRIG_COMPARE_##cond, x, y);
#define ENDIF                 EndIfCond(lab__); }

#define START_LOOP(cond)          if (cond) { be.EmitLabel("@LoopStart"); }
#define END_LOOP(cond, cReg)      if (cond) { EndWhile(cReg, "@LoopStart"); }

//=====================================================================================

class AtomicTestHelper : public Test
{
public:
    static unsigned wavesize;

protected:
    static const unsigned TEST_KIND_WAVE   = 1;
    static const unsigned TEST_KIND_WGROUP = 2;
    static const unsigned TEST_KIND_AGENT  = 3;

protected:
    static const BrigType WG_COMPLETE_TYPE = BRIG_TYPE_U32; // Type of elements in "group_complete" array

protected:
    unsigned            testKind;

protected:
    DirectiveVariable   wgComplete;                         
    PointerReg          wgCompleteAddr;

public:
    AtomicTestHelper(Location codeLocation, Grid geometry) : 
        Test(codeLocation, geometry),
        wgCompleteAddr(0)
    {
    }

    virtual ~AtomicTestHelper() {}

    // ========================================================================
public:

    unsigned Wavesize()  const { return wavesize; }

    uint64_t Groups() const 
    { 
        assert(geometry->GridSize() % geometry->WorkgroupSize() == 0);
        return geometry->GridSize() / geometry->WorkgroupSize(); 
    }

    uint64_t Waves() const 
    { 
        // Note that there may be partial wavefronts
        uint64_t ws = (Wavesize() <= geometry->WorkgroupSize())? Wavesize() : geometry->WorkgroupSize();
        assert(geometry->GridSize() % ws == 0);
        return geometry->GridSize() / ws;
    }

    std::string TestName() const 
    { 
        switch(testKind)
        {
        case TEST_KIND_WAVE:	return "wave";
        case TEST_KIND_WGROUP:	return "workgroup";
        case TEST_KIND_AGENT:	return "grid";
        default:
            assert(false);
            return 0;
        }
    }

    // ========================================================================
    // Helper code for working with wgComplete array
public:

    void DefineWgCompletedArray()
    {
        if (testKind == TEST_KIND_AGENT)
        {
            ArbitraryData values;
            unsigned typeSize = getBrigTypeNumBytes(WG_COMPLETE_TYPE);
            for (unsigned pos = 0; pos <= Groups(); ++pos) 
            {
                uint32_t value = (pos == 0)? geometry->WorkgroupSize() : (uint32_t)0;
                values.write(&value, typeSize, pos * typeSize);
            }
            Operand init = be.Brigantine().createOperandConstantBytes(values.toSRef(), WG_COMPLETE_TYPE, true);

            wgComplete = be.EmitVariableDefinition("group_complete", BRIG_SEGMENT_GLOBAL, WG_COMPLETE_TYPE, BRIG_ALIGNMENT_NONE, Groups() + 1);
            wgComplete.init() = init;
        }
    }

    PointerReg LoadWgCompleteAddr()
    {
        if (testKind == TEST_KIND_AGENT)
        {
            if (!wgCompleteAddr)
            {
                Comment("Load 'wgComplete' array address");
                wgCompleteAddr = be.AddAReg(wgComplete.segment());
                be.EmitLda(wgCompleteAddr, wgComplete);
            }
        }
        return wgCompleteAddr;
    }

    TypedReg LdWgComplete()
    {
        ItemList operands;

        BrigType t = (BrigType)type2bitType(WG_COMPLETE_TYPE);
        TypedReg atomicDst = be.AddTReg(t);
        TypedReg idx = TestWgId(LoadWgCompleteAddr()->IsLarge());
        OperandAddress target = TargetAddr(LoadWgCompleteAddr(), idx, WG_COMPLETE_TYPE);

        operands.push_back(atomicDst->Reg());
        operands.push_back(target);

        InstAtomic inst = Atomic(t, BRIG_ATOMIC_LD, BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_SCOPE_AGENT, BRIG_SEGMENT_GLOBAL, 0);
        inst.operands() = operands;

        return atomicDst;
    }

    void IncWgComplete()
    {
        assert(testKind == TEST_KIND_AGENT);

        TypedReg id = TestWgId(LoadWgCompleteAddr()->IsLarge());

        ItemList operands;

        TypedReg src0 = be.AddTReg(WG_COMPLETE_TYPE);
        be.EmitMov(src0, be.Immed(WG_COMPLETE_TYPE, 1));

        OperandAddress target = TargetAddr(LoadWgCompleteAddr(), Add(id, 1), WG_COMPLETE_TYPE);

        operands.push_back(target);
        operands.push_back(src0->Reg());

        InstAtomic inst = Atomic(WG_COMPLETE_TYPE, BRIG_ATOMIC_ADD, BRIG_MEMORY_ORDER_RELAXED, BRIG_MEMORY_SCOPE_AGENT, BRIG_SEGMENT_GLOBAL, 0, false);
        inst.operands() = operands;
    }

    void CheckPrevWg()
    {
        assert(testKind == TEST_KIND_AGENT);

        Comment("Check if all workitems in the previous workgroup have completed");
        TypedReg cnt = LdWgComplete();
        TypedReg cond = COND(cnt, LT, geometry->WorkgroupSize());
        be.EmitCbr(cond, LAB_NAME);

        Comment("Increment number of completed workitems in the current workgroup");
        IncWgComplete();
    }

    // ========================================================================
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

    OperandAddress TargetAddr(PointerReg addr, TypedReg index, BrigType elemType)
    {
        assert(isUnsignedType(addr->Type()));

        if (addr->TypeSizeBits() != index->TypeSizeBits())
        {
            TypedReg reg = be.AddTReg(addr->Type());
            EmitCvt(reg, index);
            index = reg;
        }

        PointerReg res = be.AddAReg(addr->Segment());
        EmitArith(BRIG_OPCODE_MAD, res, index, be.Immed(addr->Type(), getBrigTypeNumBytes(elemType)), addr);
        return be.Address(res);
    }

    Inst Atomic(BrigType t, BrigAtomicOperation op, BrigMemoryOrder order, BrigMemoryScope scope, BrigSegment segment, uint8_t eqclass, bool ret = true)
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

    void St(BrigType t, BrigSegment segment, OperandAddress target, TypedReg val)
    {
        InstMem inst = be.Brigantine().addInst<InstMem>(BRIG_OPCODE_ST, getUnsignedType(getBrigTypeNumBits(t)));
        inst.segment() = segment;
        inst.equivClass() = 0;
        inst.align() = BRIG_ALIGNMENT_1;
        inst.width() = BRIG_WIDTH_NONE;
        inst.modifier().isConst() = 0;
        inst.operands() = be.Operands(val->Reg(), target);
    }

    void Ld(BrigType t, BrigSegment segment, OperandAddress target, TypedReg dst)
    {
        InstMem inst = be.Brigantine().addInst<InstMem>(BRIG_OPCODE_LD, getUnsignedType(getBrigTypeNumBits(t)));
        inst.segment() = segment;
        inst.equivClass() = 0;
        inst.align() = BRIG_ALIGNMENT_1;
        inst.width() = BRIG_WIDTH_1;
        inst.modifier().isConst() = 0;
        inst.operands() = be.Operands(dst->Reg(), target);
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

    virtual TypedReg Index(unsigned arrayId, unsigned access)
    { 
        assert(false); 
        return 0;
    }

    virtual TypedReg Index() 
    { 
        assert(false); 
        return 0;
    }

    // ========================================================================

    TypedReg Cond(unsigned cond, TypedReg val1, uint64_t val2)
    {
        assert(val1);

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
        assert(val1);
        assert(val2);

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
        assert(x);
        assert(y);
        assert(cond);
        assert(x->Type() == y->Type());

        TypedReg res = be.AddTReg(x->Type());
        EmitCMov(BRIG_OPCODE_CMOV, res, cond, x, y);
        return res;
    }
    
    TypedReg CondAssign(BrigType type, int64_t x, int64_t y, TypedReg cond)
    {
        assert(cond);
        assert(x != y);

        TypedReg res = be.AddTReg(type);
        EmitCMov(BRIG_OPCODE_CMOV, res, cond, be.Immed(type, x), be.Immed(type, y));
        return res;
    }
    
    TypedReg CondAssign(TypedReg res, int64_t x, int64_t y, TypedReg cond)
    {
        assert(cond);
        assert(x != y);

        EmitCMov(BRIG_OPCODE_CMOV, res, cond, be.Immed(res->Type(), x), be.Immed(res->Type(), y));
        return res;
    }
    
    TypedReg CondAssign(TypedReg res, int64_t x, TypedReg y, TypedReg cond)
    {
        assert(cond);

        EmitCMov(BRIG_OPCODE_CMOV, res, cond, be.Immed(res->Type(), x), y);
        return res;
    }
    
    TypedReg Not(TypedReg x)
    {
        assert(x);

        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_NOT, res, x->Reg());
        return res;
    }
    
    TypedReg Or(TypedReg res, TypedReg x, TypedReg y)
    {
        assert(x);
        assert(y);
        assert(res);

        assert(res->Type() == x->Type());
        be.EmitArith(BRIG_OPCODE_OR, res, x->Reg(), y->Reg());
        return res;
    }
    
    TypedReg Or(TypedReg x, TypedReg y)
    {
        assert(x);
        assert(y);
        assert(x->Type() == y->Type());

        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_OR, res, x->Reg(), y->Reg());
        return res;
    }
    
    TypedReg Or(TypedReg x, uint64_t y)
    {
        assert(x);

        TypedReg res = be.AddTReg(x->Type());
        BrigType type = (BrigType)be.LegalizeSourceType(BRIG_OPCODE_OR, x->Type());
        be.EmitArith(BRIG_OPCODE_OR, res, x->Reg(), be.Immed(type, y));
        return res;
    }
    
    TypedReg And(TypedReg x, TypedReg y)
    {
        assert(x);
        assert(y);
        assert(x->Type() == y->Type());

        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_AND, res, x->Reg(), y->Reg());
        return res;
    }
    
    TypedReg And(TypedReg x, uint64_t y)
    {
        assert(x);

        TypedReg res = be.AddTReg(x->Type());
        BrigType type = (BrigType)be.LegalizeSourceType(BRIG_OPCODE_AND, x->Type());
        be.EmitArith(BRIG_OPCODE_AND, res, x->Reg(), be.Immed(type, y));
        return res;
    }
    
    TypedReg Add(TypedReg x, uint64_t y)
    {
        assert(x);

        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_ADD, res, x->Reg(), be.Immed(x->Type(), y));
        return res;
    }
    
    TypedReg Sub(TypedReg x, Operand y)
    {
        assert(x);
        assert(y);

        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_SUB, res, x->Reg(), y);
        return res;
    }
    
    TypedReg Sub(TypedReg x, uint64_t y)
    {
        assert(x);

        TypedReg res = be.AddTReg(x->Type());
        be.EmitArith(BRIG_OPCODE_SUB, res, x->Reg(), be.Immed(x->Type(), y));
        return res;
    }
    
    TypedReg Sub(TypedReg res, TypedReg x, uint64_t y)
    {
        assert(res);
        assert(x);

        assert(res->Type() == x->Type());
        be.EmitArith(BRIG_OPCODE_SUB, res, x->Reg(), be.Immed(x->Type(), y));
        return res;
    }
    
    TypedReg Mul(TypedReg x, uint64_t y)
    {
        assert(x);
        
        TypedReg res = be.AddTReg(x->Type());
        EmitArith(BRIG_OPCODE_MUL, res, x, y);
        return res;
    }
    
    TypedReg Div(TypedReg x, uint64_t y)
    {
        assert(x);

        TypedReg res = be.AddTReg(x->Type());
        EmitArith(BRIG_OPCODE_DIV, res, x, y);
        return res;
    }
    
    TypedReg Rem(TypedReg x, uint64_t y)
    {
        assert(x);

        TypedReg res = be.AddTReg(x->Type());
        EmitArith(BRIG_OPCODE_REM, res, x, y);
        return res;
    }
    
    TypedReg Min(TypedReg val, uint64_t max)
    {
        assert(val);

        TypedReg res = be.AddTReg(val->Type());
        InstBasic inst = be.EmitArith(BRIG_OPCODE_MIN, res, val, be.Immed(val->Type(), max));
        if (isBitType(inst.type()))
        {
            inst.type() = getUnsignedType(getBrigTypeNumBits(inst.type()));
        }

        return res;
    }

    TypedReg Shl(BrigType type, uint64_t val, TypedReg shift)
    {
        assert(shift);
        assert(getBrigTypeNumBits(shift->Type()) == 32);

        type = ArithType(BRIG_OPCODE_SHL, type);
        TypedReg res = be.AddTReg(type); 
        be.EmitArith(BRIG_OPCODE_SHL, res, be.Immed(type, val), shift->Reg());

        return res;
    }

    TypedReg Mov(BrigType type, uint64_t val) const 
    { 
        TypedReg reg = be.AddTReg(type); 
        be.EmitMov(reg, be.Immed(type2bitType(type), val)); 
        return reg; 
    }

    TypedReg Cvt(const TypedReg& src)
    {
        assert(isUnsignedType(src->Type()));
        assert(src->TypeSizeBits() == 32 || src->TypeSizeBits() == 64);
        
        BrigType type = (src->TypeSizeBits() == 32)? BRIG_TYPE_U64 : BRIG_TYPE_U32;
        TypedReg dst = be.AddTReg(type);
        EmitCvt(dst, src);
        return dst;
    }

    string IfCond(unsigned cond, TypedReg val1, uint64_t val2)
    {
        assert(val1);

        string label = be.AddLabel();
        TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
        be.EmitCmp(cReg->Reg(), val1, be.Immed(val1->Type(), val2), InvertCond(cond));
        be.EmitCbr(cReg, label);
    
        return label;
    }

    string IfCond(TypedReg cond)
    {
        assert(cond);
        assert(cond->Type() == BRIG_TYPE_B1);

        string label = be.AddLabel();
        be.EmitCbr(Not(cond), label);
    
        return label;
    }

    void EndIfCond(string label)
    {
        be.EmitLabel(label);
    }

    void EndWhile(TypedReg cond, string label)
    {
        assert(cond);

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

    static BrigType ArithType(BrigOpcode opcode, unsigned operandType)
    {
        switch (opcode) {
        case BRIG_OPCODE_SHL:
        case BRIG_OPCODE_SHR:
        case BRIG_OPCODE_MAD:
        case BRIG_OPCODE_MUL:
        case BRIG_OPCODE_DIV:
        case BRIG_OPCODE_REM: 
            return (BrigType)getUnsignedType(getBrigTypeNumBits(operandType));

        case BRIG_OPCODE_CMOV:
            return (BrigType)getBitType(getBrigTypeNumBits(operandType));

        default: return (BrigType)operandType;
        }
    }

    InstBasic EmitArith(BrigOpcode opcode, const TypedReg& dst, const TypedReg& src0, TypedReg& src1)
    {
        assert(getBrigTypeNumBits(dst->Type()) == getBrigTypeNumBits(src0->Type()));
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1->Reg());
        return inst;
    }

    InstBasic EmitArith(BrigOpcode opcode, const TypedReg& dst, const TypedReg& src0, uint64_t src1)
    {
        assert(getBrigTypeNumBits(dst->Type()) == getBrigTypeNumBits(src0->Type()));

        BrigType type = ArithType(opcode, src0->Type());
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, type);
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), be.Immed(type, src1));
        return inst;
    }

    InstBasic EmitArith(BrigOpcode opcode, const TypedReg& dst, const TypedReg& src0, Operand o)
    {
        assert(getBrigTypeNumBits(dst->Type()) == getBrigTypeNumBits(src0->Type()));
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), o);
        return inst;
    }

    InstBasic EmitArith(BrigOpcode opcode, const TypedReg& dst, const TypedReg& src0, const TypedReg& src1, Operand o)
    {
        assert(getBrigTypeNumBits(dst->Type()) == getBrigTypeNumBits(src0->Type()));
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1->Reg(), o);
        return inst;
    }

    InstBasic EmitArith(BrigOpcode opcode, const TypedReg& dst, const TypedReg& src0, Operand src1, const TypedReg& src2)
    {
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1, src2->Reg());
        return inst;
    }

    InstBasic EmitCMov(BrigOpcode opcode, const TypedReg& dst, const TypedReg& src0, TypedReg& src1, const TypedReg& src2)
    {
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, dst->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1->Reg(), src2->Reg());
        return inst;
    }

    InstBasic EmitCMov(BrigOpcode opcode, const TypedReg& dst, const TypedReg& src0, Operand src1, Operand src2)
    {
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, dst->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1, src2);
        return inst;
    }

    InstBasic EmitCMov(BrigOpcode opcode, const TypedReg& dst, const TypedReg& src0, Operand src1, TypedReg& src2)
    {
        InstBasic inst = be.Brigantine().addInst<InstBasic>(opcode, ArithType(opcode, dst->Type()));
        inst.operands() = be.Operands(dst->Reg(), src0->Reg(), src1, src2->Reg());
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
//=====================================================================================
//=====================================================================================

class FenceOpProp
{
public:
    BrigMemoryOrder     order;
    BrigMemoryScope     scope;

public:
    FenceOpProp() : order(BRIG_MEMORY_ORDER_NONE), scope(BRIG_MEMORY_SCOPE_NONE) {}

public:
    bool IsRequired() const { return order != BRIG_MEMORY_ORDER_NONE; }

public:
    void Acquire(BrigMemoryScope scope) { this->scope = scope; order = BRIG_MEMORY_ORDER_SC_ACQUIRE; }
    void Release(BrigMemoryScope scope) { this->scope = scope; order = BRIG_MEMORY_ORDER_SC_RELEASE; }
};

class MemOpProp
{
public:
    BrigAtomicOperation op;
    BrigSegment         seg;
    BrigMemoryOrder     order;
    BrigMemoryScope     scope;
    BrigType            type;
    uint8_t             eqClass;
    bool                isNoRet;
    bool                isPlainOp;
    unsigned            arrayId;

public:
    MemOpProp() {}
    MemOpProp(BrigAtomicOperation op,
              BrigSegment seg,
              BrigMemoryOrder order,
              BrigMemoryScope scope,
              BrigType type,
              uint8_t eqClass,
              bool isNoRet,
              bool isPlainOp,
              unsigned arrayId)
    {
        SetMemOpProps(op, seg, order, scope, type, eqClass, isNoRet, isPlainOp, arrayId);
    }

public:
    void SetMemOpProps(BrigAtomicOperation op,
                       BrigSegment seg,
                       BrigMemoryOrder order,
                       BrigMemoryScope scope,
                       BrigType type,
                       uint8_t eqClass,
                       bool isNoRet,
                       bool isPlainOp,
                       unsigned arrayId)
    {
        this->op        = op;
        this->seg       = seg;
        this->order     = order;
        this->scope     = scope;
        this->type      = type;
        this->eqClass   = eqClass;
        this->isNoRet   = isNoRet;
        this->isPlainOp = isPlainOp;
        this->arrayId   = arrayId;
    }

    bool IsRelaxed() const { return isPlainOp || order == BRIG_MEMORY_ORDER_RELAXED; }
    bool IsAcquire() const { return order == BRIG_MEMORY_ORDER_SC_ACQUIRE || order == BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE; }
    bool IsRelease() const { return order == BRIG_MEMORY_ORDER_SC_RELEASE || order == BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE; }
};

class TestProp : public MemOpProp
{
protected:
    static const uint64_t ZERO = 0;

private:
    AtomicTestHelper* test;

public:
    virtual ~TestProp() {}

public:
    void setup(AtomicTestHelper* test_)
    {
        assert(test_);

        test = test_;
    }

protected:
    TypedReg Mov(uint64_t val) const;
    TypedReg Min(TypedReg val, uint64_t max) const;
    TypedReg Cond(unsigned cond, TypedReg val1, uint64_t val2) const;
    TypedReg Cond(unsigned cond, TypedReg val1, TypedReg val2) const;
    TypedReg And(TypedReg x, TypedReg y) const;
    TypedReg And(TypedReg x, uint64_t y) const;
    TypedReg Or(TypedReg x, TypedReg y) const;
    TypedReg Or(TypedReg x, uint64_t y) const;
    TypedReg Add(TypedReg x, uint64_t y) const;
    TypedReg Sub(TypedReg x, uint64_t y) const;
    TypedReg Mul(TypedReg x, uint64_t y) const;
    TypedReg Shl(uint64_t x, TypedReg y) const;
    TypedReg Not(TypedReg x) const;
    TypedReg PopCount(TypedReg x) const;
    TypedReg WgId() const;                                  // workgroup id (32 bit)
    uint64_t MaxWgId() const;                               // max workgroup id
    TypedReg Id() const;                                    // local test id (32/64 bit depending on type)
    TypedReg Id32() const;                                  // local test id (32 bit)

protected:
    virtual TypedReg Idx() const;                           // global test index (32/64 bit depending address size)
    virtual TypedReg Idx(unsigned idx, unsigned acc) const; // global test index (32/64 bit depending address size)
};

//=====================================================================================
//=====================================================================================
//=====================================================================================

template<class Prop, unsigned size = 1> class TestPropFactory
{
private: 
    static TestPropFactory<Prop, size>* factory[size]; // singleton

private:
    static const unsigned ATOMIC_OPS = BRIG_ATOMIC_XOR + 1; //F
    Prop* prop[ATOMIC_OPS];

public:
    TestPropFactory(unsigned dim = 0)
    { 
        assert(dim < size);

        for (unsigned i = 0; i < ATOMIC_OPS; ++i) prop[i] = 0;

        factory[dim] = this;
    }

    virtual ~TestPropFactory() 
    { 
        for (unsigned i = 0; i < ATOMIC_OPS; ++i) delete prop[i]; 
    }

public:
    Prop* GetProp(AtomicTestHelper* test, 
                  BrigAtomicOperation op,
                  BrigSegment seg,
                  BrigMemoryOrder order,
                  BrigMemoryScope scope,
                  BrigType type,
                  uint8_t eqClass,
                  bool isNoRet,
                  bool isPlainOp = false,
                  unsigned arrayId = 0)
    {
        assert(0 <= op && op < ATOMIC_OPS);

        if (prop[op] == 0) prop[op] = CreateProp(op);
        prop[op]->SetMemOpProps(op, seg, order, scope, type, eqClass, isNoRet, isPlainOp, arrayId);
        prop[op]->setup(test);
        return prop[op];
    }

    Prop* GetProp(AtomicTestHelper* test, MemOpProp& op)
    {
        return GetProp(test, 
                       op.op, 
                       op.seg, 
                       op.order, 
                       op.scope, 
                       op.type, 
                       op.eqClass, 
                       op.isNoRet,
                       op.isPlainOp,
                       op.arrayId);
    }

    virtual Prop* CreateProp(BrigAtomicOperation op) { assert(false); return 0; }

public:
    static TestPropFactory<Prop, size>* Get(unsigned dim = 0) { assert(factory[dim]); return factory[dim]; }
};

//=====================================================================================

}

#endif // HC_ATOMIC_TEST_HELPER_HPP
