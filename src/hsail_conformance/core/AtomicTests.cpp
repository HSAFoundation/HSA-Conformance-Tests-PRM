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

#include "AtomicTests.hpp"
#include "TestHelper.hpp"

namespace hsail_conformance {

//=====================================================================================

class AtomicTest : public TestHelper
{
public:
    static unsigned wavesize;

protected:
    static const unsigned FLAG_NONE = 0;
    static const unsigned FLAG_MEM  = 1;
    static const unsigned FLAG_DST  = 2;
    static const unsigned FLAG_DST2 = 3;
    static const unsigned FLAG_VLD_MEM = 4;
    static const unsigned FLAG_VLD_DST = 5;

protected:
    static const unsigned FLAG_NONE_VAL = 0;
    static const unsigned FLAG_MEM_VAL  = 1;
    static const unsigned FLAG_DST_VAL  = 2;
    static const unsigned FLAG_VLD_MEM_VAL = 4;
    static const unsigned FLAG_VLD_DST_VAL = 8;

protected:
    BrigAtomicOperation atomicOp;
    BrigSegment segment;
    BrigMemoryOrder memoryOrder;
    BrigMemoryScope memoryScope;
    BrigType type;
    bool atomicNoRet;
    uint8_t equivClass;
    DirectiveVariable testVar;

private:
    PointerReg atomicVarAddr;
    PointerReg resArrayAddr;
    TypedReg indexInResArray;
    TypedReg atomicDst;
    TypedReg atomicMem;

public:
    //static const uint32_t gridSize = 256;

    AtomicTest(Grid geometry_,
                BrigAtomicOperation atomicOp_,
                BrigSegment segment_,
                BrigMemoryOrder memoryOrder_,
                BrigMemoryScope memoryScope_,
                BrigType type_,
                bool noret)
    : TestHelper(KERNEL, geometry_),
        atomicOp(atomicOp_),
        segment(segment_),
        memoryOrder(memoryOrder_),
        memoryScope(memoryScope_),
        type(type_),
        atomicNoRet(noret),
        equivClass(0),
        atomicVarAddr(0),
        resArrayAddr(0),
        indexInResArray(0),
        atomicDst(0),
        atomicMem(0)
    {
    }

    void Name(std::ostream& out) const 
    { 
        out << (atomicNoRet ? "atomicnoret" : "atomic")
            << "_" << atomicOperation2str(atomicOp) << "_"
            << (segment != BRIG_SEGMENT_FLAT ? segment2str(segment) : "")
            << (segment != BRIG_SEGMENT_FLAT ? "_" : "")
            << memoryOrder2str(be.AtomicMemoryOrder(atomicOp, memoryOrder))
            << "_" << memoryScope2str(memoryScope)
            << "_" << type2str(type)
            << "/" << geometry; 
    }

    BrigType ResultType() const { return BRIG_TYPE_U32; }
    
    Value ExpectedResult() const
    {
        unsigned expected = FLAG_MEM_VAL | (Encryptable()? FLAG_VLD_MEM_VAL : FLAG_NONE_VAL);
        if (!atomicNoRet) expected |= FLAG_DST_VAL | (Encryptable()? FLAG_VLD_DST_VAL : FLAG_NONE_VAL);
        return Value(MV_UINT32, U32(expected));
    }

    void Init()
    {
        Test::Init();
    }

    void ModuleVariables()
    {
        std::string varName;
        BrigSegment varSeg = segment;

        switch (segment) 
        {
        case BRIG_SEGMENT_GLOBAL: varName = "global_var"; break;
        case BRIG_SEGMENT_GROUP:  varName = "group_var";  break;
        case BRIG_SEGMENT_FLAT:   varName = "flat_var";   varSeg = BRIG_SEGMENT_GLOBAL; break;
        default: 
            assert(false);
            break;
        }

        testVar = be.EmitVariableDefinition(varName, varSeg, type);

        if (segment != BRIG_SEGMENT_GROUP) testVar.init() = Initializer(type);
    }

    // ========================================================================

    unsigned Wavesize()  const { return wavesize; }
    uint64_t LoopCount() const { return 1; }

    uint64_t Key() const
    {
        uint64_t maxValue = geometry->GridSize() * LoopCount();
        uint64_t mask     = (getBrigTypeNumBits(type) == 32) ? 0xFFFFFFFFULL : 0xFFFFFFFFFFFFFFFFULL;
        
        if (maxValue <= 0x40)       return 0x0101010101010101ULL & mask;
        if (maxValue <= 0x4000)     return 0x0001000100010001ULL & mask;
        if (maxValue <= 0x40000000) return 0x0000000100000001ULL & mask;
        return 1;
    }

    uint64_t Encode(uint64_t val) const { return val * Key(); }
    //uint64_t Decode(uint64_t val) const { return val / Key(); }
    //uint64_t Verify(uint64_t val) const { return val % Key(); }

    TypedReg EncodeRt(TypedReg val)
    {
        if (Key() != 1)
        {
            TypedReg e = be.AddTReg(val->Type());
            EmitArith(BRIG_OPCODE_MUL, e, val, be.Immed(ArithType(BRIG_OPCODE_MUL, val->Type()), Key()));
            return e;
        }
        return val;
    }

    TypedReg DecodeRt(TypedReg val)
    {
        if (Key() != 1)
        {
            TypedReg e = be.AddTReg(val->Type());
            EmitArith(BRIG_OPCODE_DIV, e, val, be.Immed(ArithType(BRIG_OPCODE_DIV, val->Type()), Key()));
            return e;
        }
        return val;
    }

    TypedReg VerifyRt(TypedReg val)
    {
        if (Key() != 1)
        {
            TypedReg e = be.AddTReg(val->Type());
            EmitArith(BRIG_OPCODE_REM, e, val, be.Immed(ArithType(BRIG_OPCODE_REM, val->Type()), Key()));
            return e;
        }
        return val;
    }

    Operand Initializer(BrigType t)
    {
        uint64_t init = InitialValue();
        if (Encryptable()) init = Encode(init);
        return be.Immed(t, init);
    }

    // ========================================================================

    bool Encryptable() const
    {
        switch (atomicOp)
        {
        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:
        case BRIG_ATOMIC_AND:
        case BRIG_ATOMIC_WRAPINC:
        case BRIG_ATOMIC_WRAPDEC:   return false;

        case BRIG_ATOMIC_ADD:
        case BRIG_ATOMIC_SUB:
        case BRIG_ATOMIC_MAX:
        case BRIG_ATOMIC_MIN:
        case BRIG_ATOMIC_EXCH:
        case BRIG_ATOMIC_CAS:
        case BRIG_ATOMIC_ST:        return Key() != 1;

        case BRIG_ATOMIC_LD:
        default:
            assert(false);
            return 0;
        }
    }

    uint64_t InitialValue()
    {
        switch (atomicOp)
        {
        case BRIG_ATOMIC_ADD:       return 0;
        case BRIG_ATOMIC_SUB:       return geometry->GridSize();

        case BRIG_ATOMIC_WRAPINC:   return 0;
        case BRIG_ATOMIC_WRAPDEC:   return geometry->GridSize() - 1;

        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:       return 0;
        case BRIG_ATOMIC_AND:       return -1;

        case BRIG_ATOMIC_MAX:       return 0;
        case BRIG_ATOMIC_MIN:       return geometry->GridSize() - 1;

        case BRIG_ATOMIC_EXCH:      return geometry->GridSize();

        case BRIG_ATOMIC_CAS:       return geometry->GridSize();

        case BRIG_ATOMIC_ST:        return geometry->GridSize();

        case BRIG_ATOMIC_LD:
        default:
            assert(false);
            return 0;
        }
    }

    ItemList AtomicOperands()
    {
        Comment("Load atomic operands");

        TypedReg src0 = be.AddTReg(type);
        TypedReg src1 = 0;

        switch (atomicOp)
        {
        case BRIG_ATOMIC_ADD:
        case BRIG_ATOMIC_SUB:
            be.EmitMov(src0, be.Immed(type2bitType(type), 1));
            break;

        case BRIG_ATOMIC_WRAPINC:
        case BRIG_ATOMIC_WRAPDEC:
            be.EmitMov(src0, be.Immed(type2bitType(type), -1)); // max value
            break;

        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:
            be.EmitArith(BRIG_OPCODE_SHL, src0, be.Immed(type, 1), TestAbsId(false)->Reg());
            break;

        case BRIG_ATOMIC_AND:
            be.EmitArith(BRIG_OPCODE_SHL, src0, be.Immed(type, 1), TestAbsId(false)->Reg());
            be.EmitArith(BRIG_OPCODE_NOT, src0, src0->Reg());
            break;

        case BRIG_ATOMIC_MAX:
        case BRIG_ATOMIC_MIN:
            src0 = TestAbsId(getBrigTypeNumBits(type) == 64);
            break;

        case BRIG_ATOMIC_EXCH:
            src0 = TestAbsId(getBrigTypeNumBits(type) == 64);
            break;

        case BRIG_ATOMIC_CAS:
            be.EmitMov(src0, be.Immed(type2bitType(type), InitialValue())); // value which is being compared
            src1 = TestAbsId(getBrigTypeNumBits(type) == 64);    // value to swap
            break;

        case BRIG_ATOMIC_ST:
            src0 = TestAbsId(getBrigTypeNumBits(type) == 64);
            break;

        case BRIG_ATOMIC_LD:
            src0 = 0;
            break;

        default: 
            assert(false);
            break;
        }

        if (Encryptable())
        {
            if (src0) src0 = EncodeRt(src0);
            if (src1) src1 = EncodeRt(src1);
        }

        ItemList operands;

        if (!atomicNoRet)
        { 
            assert(!atomicDst);
            atomicDst = be.AddTReg(getUnsignedType(getBrigTypeNumBits(type)));
            operands.push_back(atomicDst->Reg());
        }
        operands.push_back(be.Address(LoadVarAddr()));
        if (src0) operands.push_back(src0->Reg());
        if (src1) operands.push_back(src1->Reg());

        return operands;
    }

    TypedReg Index(unsigned flag)
    {
        if (flag == FLAG_DST)
        {
            assert(atomicDst);

            TypedReg tmp;

            switch (atomicOp)
            {
            case BRIG_ATOMIC_ADD:
            case BRIG_ATOMIC_WRAPINC:
            case BRIG_ATOMIC_WRAPDEC:
                Comment("Normalize dst index");
                return MinVal(atomicDst, geometry->GridSize() - 1);

            case BRIG_ATOMIC_SUB:
                Comment("Normalize dst index");
                tmp = be.AddTReg(atomicDst->Type());
                be.EmitArith(BRIG_OPCODE_SUB, tmp, atomicDst->Reg(), be.Immed(tmp->Type(), 1));
                return MinVal(tmp, geometry->GridSize() - 1);

            case BRIG_ATOMIC_AND:
                Comment("Compute and normalize dst index");
                tmp = Popcount(atomicDst);
                be.EmitArith(BRIG_OPCODE_SUB, tmp, tmp->Reg(), be.Immed(tmp->Type(), 1));
                return MinVal(tmp, geometry->GridSize() - 1);

            case BRIG_ATOMIC_OR:
            case BRIG_ATOMIC_XOR:
                Comment("Compute and normalize dst index");
                tmp = Popcount(atomicDst);
                return MinVal(tmp, geometry->GridSize() - 1);

            case BRIG_ATOMIC_MAX:
            case BRIG_ATOMIC_MIN:
                return Index();

            case BRIG_ATOMIC_EXCH:
                Comment("Normalize dst index");
                return MinVal(atomicDst, geometry->GridSize() - 1);

            case BRIG_ATOMIC_CAS:
                return Index();

            case BRIG_ATOMIC_LD:
            case BRIG_ATOMIC_ST:
            default: 
                assert(false);
                return 0;
            }
        }
        else if (flag == FLAG_DST2)
        {
            assert(atomicDst);

            if (atomicOp == BRIG_ATOMIC_EXCH)
            {
                Comment("Compute and normalize special dst index");
                TypedReg result = LdVar();
                return MinVal(result, geometry->GridSize() - 1);
            }
            return 0;
        }
        else if (flag == FLAG_MEM || flag == FLAG_VLD_MEM || flag == FLAG_VLD_DST)
        {
            return Index();
        }
        else
        {
            assert(false);
            return 0;
        }
    }

    TypedReg Condition(unsigned flag)
    {
        TypedReg tmp;

        if (flag == FLAG_DST)
        {
            assert(atomicDst);

            switch (atomicOp)
            {
            case BRIG_ATOMIC_ADD:
            case BRIG_ATOMIC_WRAPINC:
            case BRIG_ATOMIC_WRAPDEC:   return COND(atomicDst, LT, geometry->GridSize()); 

            case BRIG_ATOMIC_SUB:
                tmp = be.AddTReg(atomicDst->Type());
                be.EmitArith(BRIG_OPCODE_SUB, tmp, atomicDst->Reg(), be.Immed(tmp->Type(), 1));
                return COND(tmp, LT, geometry->GridSize()); 

            case BRIG_ATOMIC_AND:
                                        tmp = Popcount(atomicDst);
                                        be.EmitArith(BRIG_OPCODE_SUB, tmp, tmp->Reg(), be.Immed(tmp->Type(), 1));
                                        return COND(tmp, LT, geometry->GridSize()); 

            case BRIG_ATOMIC_OR:
            case BRIG_ATOMIC_XOR:
                                        tmp = Popcount(atomicDst);
                                        return COND(tmp, LT, geometry->GridSize());

            case BRIG_ATOMIC_MAX:
            case BRIG_ATOMIC_MIN:
                                        return COND(atomicDst, LT, geometry->GridSize());

            case BRIG_ATOMIC_EXCH:      return COND(atomicDst, LT, geometry->GridSize());

            case BRIG_ATOMIC_CAS:       return Or(
                                                And(
                                                    COND(atomicDst, EQ, InitialValue()),
                                                    COND(atomicMem, EQ, TestAbsId(getBrigTypeNumBits(type) == 64)->Reg())
                                                   ),
                                                And(
                                                    COND(atomicDst, EQ, atomicMem->Reg()),
                                                    COND(atomicMem, NE, TestAbsId(getBrigTypeNumBits(type) == 64)->Reg())
                                                   )
                                               );


            case BRIG_ATOMIC_LD:
            case BRIG_ATOMIC_ST:
            default: 
                assert(false);
                return 0;
            }
        }
        else if (flag == FLAG_DST2)
        {
            assert(atomicDst);

            if (atomicOp == BRIG_ATOMIC_EXCH)
            {
                return COND(atomicDst, EQ, geometry->GridSize());
            }

            assert(false);
            return 0;
        }
        else if (flag == FLAG_MEM)
        {
            TypedReg result = LdVar();

            switch (atomicOp)
            {
            case BRIG_ATOMIC_ADD:
            case BRIG_ATOMIC_WRAPINC:   return COND(result, EQ, geometry->GridSize());

            case BRIG_ATOMIC_SUB:       return COND(result, EQ, 0);
            case BRIG_ATOMIC_WRAPDEC:   return COND(result, EQ, -1);

            case BRIG_ATOMIC_AND:       return COND(result, EQ, 0);

            case BRIG_ATOMIC_OR:
            case BRIG_ATOMIC_XOR:       return COND(result, EQ, -1);

            case BRIG_ATOMIC_MAX:       return COND(result, EQ, geometry->GridSize() - 1);
            case BRIG_ATOMIC_MIN:       return COND(result, EQ, 0);

            case BRIG_ATOMIC_EXCH:      return COND(result, LT, geometry->GridSize());

            case BRIG_ATOMIC_CAS:       return COND(result, LT, geometry->GridSize());

            case BRIG_ATOMIC_ST:        return COND(result, LT, geometry->GridSize());

            case BRIG_ATOMIC_LD:
            default:
                assert(false);
                return 0;
            }
        }
        else if (flag == FLAG_VLD_MEM)
        {
            TypedReg result = VerifyRt(LdVar());
            return COND(result, EQ, 0);
        }
        else if (flag == FLAG_VLD_DST)
        {
            assert(atomicDst);
            TypedReg result = VerifyRt(atomicDst);
            return COND(result, EQ, 0);
        }
        else
        {
            assert(false);
            return 0;
        }
    }

    // ========================================================================

    void KernelCode()
    {
        assert(codeLocation == emitter::KERNEL);

        LoadVarAddr();
        InitVar();

        LoadResAddr();
        InitRes();

        Comment("Synchronize");
        Barrier();

        ItemList operands = AtomicOperands();
        Atomic(operands);

        Comment("Synchronize");
        Barrier();

        ValidateDst();
        ValidateMem();

        CheckResult(FLAG_MEM);
        if (!atomicNoRet)
        {
            CheckResult(FLAG_DST);
            CheckResult(FLAG_DST2);
        }
    }

    PointerReg LoadVarAddr()
    {
        if (!atomicVarAddr)
        {
            Comment("Load variable address");
            atomicVarAddr = be.AddAReg(testVar.segment());
            be.EmitLda(atomicVarAddr, testVar);
        }
        return atomicVarAddr;
    }

    PointerReg LoadResAddr()
    {
        if (!resArrayAddr)
        {
            Comment("Load result address");
            resArrayAddr = output->Address();
        }
        return resArrayAddr;
    }

    TypedReg Index()
    {
        if (!indexInResArray)
        {
            indexInResArray = be.EmitWorkitemFlatAbsId(LoadResAddr()->IsLarge());
        }
        return indexInResArray;
    }

    void InitVar()
    {
        if (segment == BRIG_SEGMENT_GROUP)
        {
            PointerReg varAddr = LoadVarAddr(); // NB: unconditional

            Comment("Init variable");

            TypedReg id = be.EmitWorkitemFlatAbsId(false);
            STARTIF(id, EQ, 0)

            InstAtomic inst = AtomicInst(type, BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_SC_RELEASE, memoryScope, segment, equivClass, false);
            inst.operands() = be.Operands(be.Address(varAddr), Initializer((BrigType)type2bitType(type)));

            ENDIF
        }
    }

    void InitRes()
    {
        Comment("Clear result array");

        OperandAddress target = TargetAddr(LoadResAddr(), Index());
        InstAtomic inst = AtomicInst(ResultType(), BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_SC_RELEASE, memoryScope, (BrigSegment)LoadResAddr()->Segment(), 0, false);
        inst.operands() = be.Operands(target, be.Immed(ResultType(), FLAG_NONE_VAL));
    }

    void Atomic(ItemList operands)
    {
        Comment("This is the instruction being tested:");

        InstAtomic inst = AtomicInst(type, atomicOp, memoryOrder, memoryScope, segment, equivClass, !atomicNoRet);
        inst.operands() = operands;
    }

    TypedReg LdVar()
    {
        if (!atomicMem)
        {
            Comment("Load final value from memory");

            atomicMem = be.AddTReg(type);
            InstAtomic inst = AtomicInst(type, BRIG_ATOMIC_LD, BRIG_MEMORY_ORDER_SC_ACQUIRE, memoryScope, segment, equivClass);
            inst.operands() = be.Operands(atomicMem->Reg(), be.Address(LoadVarAddr()));
        }
        return atomicMem;
    }

    void CheckResult(unsigned flag)
    {
        TypedReg idx = Index(flag);
        if (idx)
        {
            Comment(GetFlagComment(flag));
            SetFlag(idx, flag);
        }
    }

    void ValidateDst()
    {
        if (!atomicNoRet && Encryptable())
        {
            assert(atomicDst);
            CheckResult(FLAG_VLD_DST);
            Comment("Decode dst value");
            atomicDst = DecodeRt(atomicDst);
        }
    }

    void ValidateMem()
    {
        LdVar();

        if (Encryptable())
        {
            CheckResult(FLAG_VLD_MEM);
            Comment("Decode memory value");
            atomicMem = DecodeRt(atomicMem);
        }
    }

    // ========================================================================

    void SetFlag(TypedReg index, unsigned flag)
    {
        TypedReg flagValue = GetFlag(flag);
        OperandAddress target = TargetAddr(LoadResAddr(), index);
        InstAtomic inst = AtomicInst(ResultType(), BRIG_ATOMIC_ADD, BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE, memoryScope, (BrigSegment)LoadResAddr()->Segment(), 0, false);
        inst.operands() = be.Operands(target, flagValue->Reg());
    }
    
    TypedReg GetFlag(unsigned flag)
    {
        TypedReg cond = Condition(flag);
        TypedReg dst = be.AddTReg(ResultType());
        InstBasic inst = be.Brigantine().addInst<InstBasic>(BRIG_OPCODE_CMOV, type2bitType(ResultType()));
        inst.operands() = be.Operands(dst->Reg(), cond->Reg(), be.Immed(ResultType(), GetFlagVal(flag)), be.Immed(ResultType(), GetFlagVal(FLAG_NONE_VAL)));
        return dst;
    }

    const char* GetFlagComment(unsigned flag)
    {
        switch(flag)
        {
        case FLAG_MEM:  return "Check final value in memory";
        case FLAG_DST:  return "Check atomic dst";
        case FLAG_DST2: return "Check atomic dst (special)";
        case FLAG_VLD_DST:  return "Validate atomic dst";
        case FLAG_VLD_MEM:  return "Validate final value in memory";
        default:
            assert(false);
            return "";
        }
    }

    unsigned GetFlagVal(unsigned flag)
    {
        switch(flag)
        {
        case FLAG_NONE: return FLAG_NONE_VAL;
        case FLAG_MEM:  return FLAG_MEM_VAL;
        case FLAG_DST:  return FLAG_DST_VAL;
        case FLAG_DST2: return FLAG_DST_VAL;
        case FLAG_VLD_DST:  return FLAG_VLD_DST_VAL;
        case FLAG_VLD_MEM:  return FLAG_VLD_MEM_VAL;
        default:
            assert(false);
            return FLAG_NONE_VAL;
        }
    }

    // ========================================================================

    bool IsValid() const
    {
        if (!IsValidAtomic(atomicOp, segment, memoryOrder, memoryScope, type, atomicNoRet)) return false;
        if (!IsValidGrid()) return false;

        // List of current limitations (features that require special testing setup)
        // Tests for the following features should be implemented separately
        if (atomicOp == BRIG_ATOMIC_LD) return false;
        if (memoryScope == BRIG_MEMORY_SCOPE_SYSTEM) return false;

        // List of current limitations which should be alleviated
        if (atomicOp == BRIG_ATOMIC_ST && memoryOrder != BRIG_MEMORY_ORDER_SC_RELEASE) return false;            //F todo
        if (atomicOp != BRIG_ATOMIC_ST && memoryOrder != BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE) return false;    //F todo

        return true;
    }

    bool IsValidGrid() const
    {
        if (memoryScope == BRIG_MEMORY_SCOPE_WAVEFRONT && (geometry->GridSize() != geometry->WorkgroupSize() || geometry->WorkgroupSize() > Wavesize())) return false;
        if (memoryScope == BRIG_MEMORY_SCOPE_WORKGROUP &&  geometry->GridSize() != geometry->WorkgroupSize()) return false;

        switch (atomicOp)
        {
        case BRIG_ATOMIC_AND:
        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:
            return getBrigTypeNumBits(type) == geometry->GridSize();
            
        default:
            return true;
        }
    }

}; // class AtomicTest

unsigned AtomicTest::wavesize;

//=====================================================================================

void AtomicTests::Iterate(hexl::TestSpecIterator& it)
{
    CoreConfig* cc = CoreConfig::Get(context);
    AtomicTest::wavesize = cc->Wavesize(); //F: how to get the value from inside of AtomicTest?
    Arena* ap = cc->Ap();
    TestForEach<AtomicTest>(ap, it, "atomicity", cc->Grids().AtomicSet(), cc->Memory().AllAtomics(), cc->Segments().Atomic(), cc->Memory().AllMemoryOrders(), cc->Memory().AllMemoryScopes(), cc->Types().Atomic(), Bools::All());
}

//=====================================================================================

} // namespace hsail_conformance
