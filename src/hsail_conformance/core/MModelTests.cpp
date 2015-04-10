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

// ========================================================================
// This is a test for compliance with memory model requirements
//
// The purpose of this code is to test the result of execution of 
// a pair of write instructions A and B which should not be reordered.
// These instructions are executed by one workitem (X) but results are 
// inspected by another workitem (Y). If X execute A and B in this order,
// Y must see the result of A after the result of B is visible.
//
// Each workitem in this test does the following:
//  1. prepare test data at index wi.id:
//      - write test data T0 to array0 at index wi.id (instruction A)
//      - execute a memory fence if instructions A and B may be reordered
//      - write test data T1 to array1 at index wi.id (instruction B)
//  2. read test data written by another workitem at another index:
//      - read test data H1 from array1 at index testId (instruction C)
//      - read test data H0 from array0 at index testId (instruction D)
//  3. validate test data read on the previous step:
//      - if (H1 == T1) validate(H0 == T0)
//  
// There are the following types of this test:
//  1. WAVE kind.
//     Items withing a wave test data written by other items within the same wave.
//  2. WGROUP kind.
//     Items withing a wave test data written by other items within the same workgroup but in another wave.
//  3. AGENT kind.
//     Items withing a workgroup N+1 (N>0) test data written by items within the workgroup N.
//     Items withing a workgroup 0 test data written by itself.
//
// Legend:
//  - wi.id:   workitemflatabsid
//  - wg.id:   workgroupid(0)
//  - wg.size:  workgroup size in X dimension 
//  - grid.size:  grid size in X dimension 
//  - test.size:  number of workitems which participate in the test:
//                   - wavesize for WAVE kind
//                   - wg.size for WGROUP kind
//                   - grid.size for AGENT kind
//  - delta:      distance (absolute flat) between a workitem which generates test data 
//                and a workitem which validates the same test data:
//                   - 1 for WAVE kind
//                   - wavesize for WGROUP kind
//                   - wg.size for AGENT kind
//
// --------------------------------------------------------------------------------
// For WAVE and WGROUP test kinds, the code looks like this:
//
//                                           // Define and initialize test arrays
//                                           // NB: initialization is only possible for arrays in GLOBAL segment
//    <type0> <seg0> testArray0[(seg0 == GROUP)? wg.size : grid.size] = {InitialValue(0), InitialValue(0), ...}; 
//    <type1> <seg1> testArray1[(seg1 == GROUP)? wg.size : grid.size] = {InitialValue(1), InitialValue(1), ...}; 
//    
//    kernel(unsigned global ok[grid.size]) // output array
//    {
//        private unsigned testId = ((wi.id % test.size) < delta)? wi.id + test.size - delta : wi.id - delta;
//
//        // testComplete flag is used to only record result
//        // at first successful 'synchronized-with' attempt
//        private bool testComplete = 0;
//
//        private loopIdx = MAX_LOOP_CNT;
//
//        testArray0[wi.id] = InitialValue(0);	    // init test array (for group segment only)
//        testArray1[wi.id] = InitialValue(1); 
//        ok[wi.id] = 0;                            // clear result flag
//        
//        // make sure all workitems have completed initialization
//        // This is important because otherwise some workitems 
//        // may see uninitialized values
//        
//        memfence_screl_wg;
//        (wave)barrier;
//        memfence_scacq_wg;
//        
//        // These are the instructions being tested
//        // Note that write attributes (order, scope, etc) are not shown here for brevity
//        
//        testArray0[wi.id] = ExpectedValue(0);     // Instruction A
//        memory_fence;                             // optional fence - inserted only if instructions A and B may be reordered
//        testArray1[wi.id] = ExpectedValue(1);     // Instruction B
//        
//        //NB: execution model for workitems within a wave or workgroup is not defined.
//        //    The code below attempts to synchronize with the second write (instruction B)
//        //    executing a limited number of iterations. If this attempt fails, the code
//        //    waits at barrier and does one more synchrinization attempt which should
//        //    succeed regardless of used execution model.
//
//        do
//        {
//            private bool syncWith = (testArray1[testId] == ExpectedValue(1));     // Instruction C
//            private bool resultOk = (testArray0[testId] == ExpectedValue(0));     // Instruction D
//            ok[wi.id] |= (!testComplete && syncWith && resultOk)? PASSED : FAILED;
//            testComplete |= syncWith;
//        }
//        while (--loopIdx != 0);
//
//        // make sure all workitems have completed writing test data.
//        // note that (wave)barrier below cannot be removed because it is the only 
//        // way to ensure pseudo-parallel execution.
//        
//        memfence_screl_wg;
//        (wave)barrier;
//        memfence_scacq_wg;
//        
//        // Validate test values written by another workitem
//        // Note that read attributes (order, scope, etc) are not shown here for brevity
//        
//        private bool syncWith = (testArray1[testId] == ExpectedValue(1));         // Instruction C'
//        private bool resultOk = (testArray0[testId] == ExpectedValue(0));         // Instruction D'
//        ok[wi.id] |= (!testComplete && syncWith && resultOk)? PASSED : FAILED;
//    }
// 
// --------------------------------------------------------------------------------
// For AGENT test kind, the code looks like this:
//
//                                           // Define and initialize test arrays
//    <type0> GLOBAL testArray0[grid.size] = {InitialValue(0), InitialValue(0), ...}; 
//    <type1> GLOBAL testArray1[grid.size] = {InitialValue(1), InitialValue(1), ...}; 
//
//    // Array used to check if all workitems in the previous workgroup have finished
//    // When workitem i finishes, it increments element at index i+1
//    // First element is initialized to ensure completion of first group
//    unsigned global finished[grid.size / wg.size + 1] = {wg.size, 0, 0, ...};
//    
//    kernel(unsigned global ok[grid.size]) // output array
//    {
//        // workitems in first workgroup test data written by itself;
//        // workitems in other workgroups test data written by items in previous wgs
//        private unsigned testId = (wi.id < wg.size)? wi.id : wi.id - wg.size;
//
//        // testComplete flag is used to only record result
//        // at first successful 'synchronized-with' attempt
//        private bool testComplete = 0;
//
//        ok[wi.id] = 0;                            // clear result flag
//        
//        // These are the instructions being tested
//        // Note that write attributes (order, scope, etc) are not shown here for brevity
//        
//        testArray0[wi.id] = ExpectedValue(0);     // Instruction A
//        memory_fence;                             // optional fence - inserted only if instructions A and B may be reordered
//        testArray1[wi.id] = ExpectedValue(1);     // Instruction B
//        
//        // Wait for test values written by another workitem
//        do
//        {
//            private bool syncWith = (testArray1[testId] == ExpectedValue(1));         // Instruction C
//            private bool resultOk = (testArray0[testId] == ExpectedValue(0));         // Instruction D
//            ok[wi.id] |= (!testComplete && syncWith && resultOk)? PASSED : FAILED;
//            testComplete |= syncWith;
//        }
//        while (finished[wg.id] < wg.size); // wait for previous wg to complete
//
//        finished[wg.id + 1]++; // Label this wi as completed
//    }
// 

#include "MModelTests.hpp"
#include "TestHelper.hpp"

namespace hsail_conformance {

//#define QUICK_TEST

//=====================================================================================

#define LAB_NAME ("@LoopStart")

class MModelTest : public TestHelper
{
public:
    static unsigned wavesize;

protected:
    static const unsigned TEST_KIND_WAVE   = 1;
    static const unsigned TEST_KIND_WGROUP = 2;
    static const unsigned TEST_KIND_AGENT  = 3;

protected:
    static const BrigType RES_TYPE       = BRIG_TYPE_U32; // Type of elements in output array
    static const unsigned RES_VAL_FAILED = 0;
    static const unsigned RES_VAL_PASSED = 1;

protected:
    static const BrigType WG_COMPLETE_TYPE = BRIG_TYPE_U32; // Type of elements in "group_complete" array

protected:
    static const unsigned MAX_LOOP = 1000;

protected:                                                  // attributes of write operations which are being tested
    BrigAtomicOperation atomicOp[2];
    BrigSegment         segment[2];
    BrigMemoryOrder     memoryOrder[2];
    BrigMemoryScope     memoryScope[2];
    BrigType            type[2];
    uint8_t             equivClass[2];
    bool                atomicNoRet[2];                     // atomic or atomicNoRet
    bool                isPlainOp[2];                       // atomic or plain ld/st

private:
    DirectiveVariable   testArray[2];
    PointerReg          testArrayAddr[2];
    TypedReg            indexInTestArray[2][2];

private:
    DirectiveVariable   wgComplete;                         
    PointerReg          wgCompleteAddr;

private:
    PointerReg          resArrayAddr;
    TypedReg            indexInResArray;

private:
    unsigned            testKind;
    bool                mapFlat2Group;                     // if true, map flat to group, if false, map flat to global
    TypedReg            loopIdx;

public:

#ifdef QUICK_TEST

    MModelTest(Grid geometry_,
                BrigAtomicOperation atomicOp_0,
                BrigSegment segment_0,
                BrigSegment segment_1,
                BrigMemoryOrder memoryOrder_0,
                BrigMemoryOrder memoryOrder_1,
                BrigMemoryScope memoryScope_0,
                BrigMemoryScope memoryScope_1,
                BrigType type_1,
                bool isPlainOp_0
                )
    : TestHelper(KERNEL, geometry_),
        resArrayAddr(0),
        wgCompleteAddr(0),
        indexInResArray(0),
        loopIdx(0)
    {
        SetTestKind();

        BrigType type_0 = type_1;                       // This is to minimize total number of tests
        BrigAtomicOperation atomicOp_1 = atomicOp_0;    // This is to minimize total number of tests
        mapFlat2Group = isBitType(type_1);              // This is to minimize total number of tests
        bool isPlainOp_1 = false;                       // Second write must be an atomic to make synchromization possible

        for (unsigned i = 0; i < 2; ++i)
        {
            atomicOp[i]    = (i == 0)? atomicOp_0    : atomicOp_1;
            segment[i]     = (i == 0)? segment_0     : segment_1;
            memoryOrder[i] = (i == 0)? memoryOrder_0 : memoryOrder_1;
            memoryScope[i] = (i == 0)? memoryScope_0 : memoryScope_1;
            type[i]        = (i == 0)? type_0        : type_1;
            isPlainOp[i]   = (i == 0)? isPlainOp_0 : isPlainOp_1;
            atomicNoRet[i] = (i == 0);
            equivClass[i]  = 0;

            indexInTestArray[i][0] = 0;
            indexInTestArray[i][1] = 0;
            testArrayAddr[i] = 0;
        }

        // As quich test does not enumerate all possible combinations for first write, 
        // the following code ensures that attributes of this operation are valid
        if (isPlainOp[0])
        {
            type[0]         = BRIG_TYPE_S32;
            atomicOp[0]     = BRIG_ATOMIC_ST;
            atomicNoRet[0]  = true;
        }
        else
        {
            if (!IsValidAtomicOp(atomicOp[0], atomicNoRet[0])) atomicOp[0] = BRIG_ATOMIC_ST;
            if (!IsValidAtomicType(atomicOp[0], type[0]))      type[0]     = BRIG_TYPE_B32;
        }
    }

#else

    MModelTest(Grid geometry_,
                BrigAtomicOperation atomicOp_0,
                BrigAtomicOperation atomicOp_1,
                BrigSegment segment_0,
                BrigSegment segment_1,
                BrigMemoryOrder memoryOrder_0,
                BrigMemoryOrder memoryOrder_1,
                BrigMemoryScope memoryScope_0,
                BrigMemoryScope memoryScope_1,
                BrigType type_0,
                BrigType type_1,
                bool isPlainOp_0
                )
    : TestHelper(KERNEL, geometry_),
        resArrayAddr(0),
        wgCompleteAddr(0),
        indexInResArray(0),
        loopIdx(0)
    {
        SetTestKind();

        mapFlat2Group = isBitType(type_1);              // This is to minimize total number of tests
        bool isPlainOp_1 = false;                       // Second write must be an atomic to make synchromization possible

        for (unsigned i = 0; i < 2; ++i)
        {
            atomicOp[i]    = (i == 0)? atomicOp_0    : atomicOp_1;
            segment[i]     = (i == 0)? segment_0     : segment_1;
            memoryOrder[i] = (i == 0)? memoryOrder_0 : memoryOrder_1;
            memoryScope[i] = (i == 0)? memoryScope_0 : memoryScope_1;
            type[i]        = (i == 0)? type_0        : type_1;
            isPlainOp[i]   = (i == 0)? isPlainOp_0   : isPlainOp_1;
            atomicNoRet[i] = (i == 0);
            equivClass[i]  = 0;

            indexInTestArray[i][0] = 0;
            indexInTestArray[i][1] = 0;
            testArrayAddr[i] = 0;
        }

        // To avoid enumeration of all possible combinations of atomicNoRet for
        // both writes, deduce required value based on other attributes.
        // This is quite sufficient for test purposes.

        if (isPlainOp[0])
        {
            atomicNoRet[0] = true;
        }
        else
        {
            if (!IsValidAtomicOp(atomicOp[0], atomicNoRet[0])) atomicNoRet[0] = !atomicNoRet[0];
        }

        if (!IsValidAtomicOp(atomicOp[1], atomicNoRet[1]))
        {
            atomicNoRet[1] = !atomicNoRet[1];
        }
    }

#endif

    // ========================================================================
    // Test Name

    void Name(std::ostream& out) const 
    { 
        if (isPlainOp[0]) StName(out, 0); else AtomicName(out, 0);
        if (FenceRequired()) FenceName(out);
        out << "__";
        if (isPlainOp[1]) StName(out, 1); else AtomicName(out, 1);
        out << "/" << geometry; 
    }

    void AtomicName(std::ostream& out, unsigned idx) const 
    { 
        assert(idx <= 1);
        out << (atomicNoRet[idx] ? "atomicnoret" : "atomic")
            << "_" << atomicOperation2str(atomicOp[idx])
                   << SegName(segment[idx])
            << "_" << memoryOrder2str(memoryOrder[idx])
            << "_" << memoryScope2str(memoryScope[idx])
            << "_" << type2str(type[idx]);
    }

    void StName(std::ostream& out, unsigned idx) const 
    { 
        assert(idx <= 1);
        out << "st" << SegName(segment[idx]) << "_" << type2str(type[idx]);
    }

    std::string SegName(BrigSegment seg) const
    {
        if (seg == BRIG_SEGMENT_FLAT) return "_flat";
        else                          return string("_") + segment2str(seg);
    }

    void FenceName(std::ostream& out) const 
    { 
        out << "__"
            << "fence"
            << "_" << memoryOrder2str(FenceOrder())
            << "_" << memoryScope2str(FenceScope());
    }

    // ========================================================================
    // Definition of test variables and arrays

    BrigType ResultType() const { return RES_TYPE; }
    
    Value ExpectedResult() const
    {
        return Value(MV_UINT32, U32(1));
    }

    void Init()
    {
        Test::Init();
    }

    void ModuleVariables()
    {
        Comment("Testing memory operations within " + TestName());

        DefineArray(0);
        DefineArray(1);

        if (testKind == TEST_KIND_AGENT) DefineWgCompletedArray();
    }

    void DefineArray(unsigned idx)
    {
        assert(idx <= 1);

        std::string arrayName;
        BrigSegment seg = segment[idx];

        switch (seg) 
        {
        case BRIG_SEGMENT_GLOBAL: arrayName = (idx == 0)? "global_array_0" : "global_array_1"; break;
        case BRIG_SEGMENT_GROUP:  arrayName = (idx == 0)? "group_array_0"  : "group_array_1";  break;
        case BRIG_SEGMENT_FLAT:   arrayName = (idx == 0)? "flat_array_0"   : "flat_array_1";   break;
        default: 
            assert(false);
            break;
        }

        testArray[idx] = be.EmitVariableDefinition(arrayName, ArraySegment(idx), ArrayElemType(idx), BRIG_ALIGNMENT_NONE, ArraySize(idx));
        if (ArraySegment(idx) != BRIG_SEGMENT_GROUP) testArray[idx].init() = ArrayInitializer(idx);
    }

    void DefineWgCompletedArray()
    {
        assert(testKind == TEST_KIND_AGENT);

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

    Operand ArrayInitializer(unsigned idx)
    {
        assert(idx <= 1);
        assert(isIntType(type[idx]));

        ArbitraryData values;
        uint64_t value = InitialValue(idx);
        unsigned typeSize = getBrigTypeNumBytes(type[idx]);
        for (unsigned pos = 0; pos < geometry->GridSize(); ++pos) values.write(&value, typeSize, pos * typeSize);
        return be.Brigantine().createOperandConstantBytes(values.toSRef(), ArrayElemType(idx), true);
    }

    BrigType ArrayElemType(unsigned idx)
    {
        assert(idx <= 1);
        return isBitType(type[idx])? (BrigType)getUnsignedType(getBrigTypeNumBits(type[idx])) : type[idx];
    }

    BrigSegment ArraySegment(unsigned idx) const
    {
        assert(idx <= 1);
        if (segment[idx] == BRIG_SEGMENT_FLAT) return mapFlat2Group? BRIG_SEGMENT_GROUP : BRIG_SEGMENT_GLOBAL;
        return segment[idx];
    }

    // ========================================================================
    // Test properties

    unsigned Wavesize()  const { return wavesize; }

    uint64_t Groups() const 
    { 
        assert(geometry->GridSize() % geometry->WorkgroupSize() == 0);
        return geometry->GridSize() / geometry->WorkgroupSize(); 
    }

    uint64_t ArraySize(unsigned idx) const 
    { 
        assert(idx <= 1);
        return (ArraySegment(idx) == BRIG_SEGMENT_GROUP)? (uint64_t)geometry->WorkgroupSize() : geometry->GridSize();
    }
    
    void SetTestKind()
    {
        if      (geometry->WorkgroupSize() == Wavesize())             testKind = TEST_KIND_WAVE;
        else if (geometry->WorkgroupSize() == Wavesize() * 4)         testKind = TEST_KIND_WGROUP;
        else                                                          testKind = TEST_KIND_AGENT;
    }

    bool Loopable()    const { return testKind != TEST_KIND_AGENT; }

    unsigned Delta() const 
    { 
        switch(testKind)
        {
        case TEST_KIND_WAVE:	return 1;
        case TEST_KIND_WGROUP:	return Wavesize();
        case TEST_KIND_AGENT:	return geometry->WorkgroupSize();
        default:
            assert(false);
            return 0;
        }
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

    uint64_t TestSize() const 
    { 
        switch(testKind)
        {
        case TEST_KIND_WAVE:	return Wavesize();
        case TEST_KIND_WGROUP:	return geometry->WorkgroupSize();
        case TEST_KIND_AGENT:	return geometry->GridSize();
        default:
            assert(false);
            return 0;
        }
    }

    // ========================================================================
    // Initialization of test arrays

    uint64_t InitialValue(unsigned idx)
    {
        assert(idx <= 1);
        switch (atomicOp[idx])
        {
        case BRIG_ATOMIC_ADD:       return 0;
        case BRIG_ATOMIC_SUB:       return 2; //TestSize() + 1;

        case BRIG_ATOMIC_WRAPINC:   return 0;
        case BRIG_ATOMIC_WRAPDEC:   return 2; //TestSize() + 1;

        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:       return 0;
        case BRIG_ATOMIC_AND:       return -1;

        case BRIG_ATOMIC_MAX:       return 0;
        case BRIG_ATOMIC_MIN:       return 2; //TestSize() + 1;

        case BRIG_ATOMIC_EXCH:      return 0;

        case BRIG_ATOMIC_CAS:       return 0;

        case BRIG_ATOMIC_ST:        return 0;

        case BRIG_ATOMIC_LD:
        default:
            assert(false);
            return 0;
        }
    }

    uint64_t ExpectedValue(unsigned idx)
    {
        assert(idx <= 1);
        uint64_t expected = 1;
        assert(InitialValue(idx) != expected);
        return expected;
    }

    Operand Initializer(unsigned idx)
    {
        assert(idx <= 1);
        assert(isIntType(type[idx]));

        uint64_t init = InitialValue(idx);
        return be.Immed(getUnsignedType(getBrigTypeNumBits(type[idx])), init);
    }

    // ========================================================================
    // Encoding of atomic read and write operations

    ItemList AtomicOperands(unsigned idx)
    {
        assert(idx <= 1);

        TypedReg src0 = be.AddTReg(type[idx]);
        TypedReg src1 = 0;

        switch (atomicOp[idx])
        {
        case BRIG_ATOMIC_ADD:
        case BRIG_ATOMIC_SUB:
        case BRIG_ATOMIC_ST:
            be.EmitMov(src0, be.Immed(type2bitType(type[idx]), 1));
            break;

        case BRIG_ATOMIC_WRAPINC:
        case BRIG_ATOMIC_WRAPDEC:
            be.EmitMov(src0, be.Immed(type2bitType(type[idx]), -1)); // operand is max value
            break;
        
        case BRIG_ATOMIC_AND:
        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:
            be.EmitMov(src0, be.Immed(type2bitType(type[idx]), 1));
            break;
        
        case BRIG_ATOMIC_MAX:
        case BRIG_ATOMIC_MIN:
            be.EmitMov(src0, be.Immed(type2bitType(type[idx]), 1));
            break;

        case BRIG_ATOMIC_EXCH:
            be.EmitMov(src0, be.Immed(type2bitType(type[idx]), 1));
            break;

        case BRIG_ATOMIC_CAS:
            src1 = be.AddTReg(type[idx]);
            be.EmitMov(src0, be.Immed(type2bitType(type[idx]), InitialValue(idx))); // value which is being compared
            be.EmitMov(src1, be.Immed(type2bitType(type[idx]), 1));                 // value to swap
            break;

        default: 
            assert(false);
            break;
        }

        ItemList operands;

        if (!atomicNoRet[idx])
        { 
            TypedReg atomicDst = be.AddTReg(getUnsignedType(getBrigTypeNumBits(type[idx])));
            operands.push_back(atomicDst->Reg());
        }
        OperandAddress target = TargetAddr(LoadArrayAddr(idx), ArrayIndex(idx, 0), type[idx]);
        operands.push_back(target);
        if (src0) operands.push_back(src0->Reg());
        if (src1) operands.push_back(src1->Reg());

        return operands;
    }

    void AtomicSt(unsigned idx)
    {
        assert(idx <= 1);

        ItemList operands = AtomicOperands(idx);
        InstAtomic inst = AtomicInst(type[idx], atomicOp[idx], memoryOrder[idx], memoryScope[idx], segment[idx], equivClass[idx], !atomicNoRet[idx]);
        inst.operands() = operands;
    }

    TypedReg AtomicLd(unsigned idx)
    {
        assert(idx <= 1);

        ItemList operands;
        TypedReg atomicDst = be.AddTReg(getUnsignedType(getBrigTypeNumBits(type[idx])));
        OperandAddress target = TargetAddr(LoadArrayAddr(idx), ArrayIndex(idx, 1), type[idx]);

        operands.push_back(atomicDst->Reg());
        operands.push_back(target);

        // NB: reading is done in reversed order: 1 -> 0
        BrigMemoryOrder readOrder = (idx == 0)? BRIG_MEMORY_ORDER_RELAXED : 
                                                BRIG_MEMORY_ORDER_SC_ACQUIRE; // NB: ACQUIRE_RELEASE is not supported by 'ld'

        InstAtomic inst = AtomicInst(type[idx], BRIG_ATOMIC_LD, readOrder, memoryScope[idx], segment[idx], equivClass[idx]);
        inst.operands() = operands;

        return atomicDst;
    }

    // ========================================================================
    // Encoding of plain read and write operations

    void PlainSt(unsigned idx = 0)
    {
        assert(idx <= 1);

        ItemList operands;
        uint64_t val = ExpectedValue(idx);
        OperandAddress target = TargetAddr(LoadArrayAddr(idx), ArrayIndex(idx, 0), type[idx]);
        operands.push_back(be.Immed(type2bitType(type[idx]), val));
        operands.push_back(target);

        InstMem inst = be.Brigantine().addInst<InstMem>(BRIG_OPCODE_ST, getUnsignedType(getBrigTypeNumBits(type[idx])));
        inst.segment() = segment[idx];
        inst.equivClass() = equivClass[idx];
        inst.align() = BRIG_ALIGNMENT_1;
        inst.width() = BRIG_WIDTH_NONE;
        inst.modifier().isConst() = 0;
        inst.operands() = operands;
    }

    TypedReg PlainLd(unsigned idx = 0)
    {
        assert(idx <= 1);

        ItemList operands;
        TypedReg dst = be.AddTReg(getUnsignedType(getBrigTypeNumBits(type[idx])));
        OperandAddress target = TargetAddr(LoadArrayAddr(idx), ArrayIndex(idx, 1), type[idx]);

        operands.push_back(dst->Reg());
        operands.push_back(target);

        InstMem inst = be.Brigantine().addInst<InstMem>(BRIG_OPCODE_LD, type[idx]);
        inst.segment() = segment[idx];
        inst.equivClass() = equivClass[idx];
        inst.align() = BRIG_ALIGNMENT_1;
        inst.width() = BRIG_WIDTH_1;
        inst.modifier().isConst() = 0;
        inst.operands() = operands;

        return dst;
    }

    // ========================================================================
    // Kernel code

    void KernelCode()
    {
        assert(codeLocation == emitter::KERNEL);
        
        LoadArrayAddr(0);
        LoadArrayAddr(1);
        LoadResAddr();
        LoadWgCompleteAddr();

        ArrayIndex(0, 0);
        ArrayIndex(0, 1);
        ArrayIndex(1, 0);
        ArrayIndex(1, 1);
        ResIndex();

        InitArray(0);
        InitArray(1);
        InitRes();

        InitLoop();

        Comment("Clear 'testComplete' flag");
        TypedReg testComplete = be.AddTReg(BRIG_TYPE_B1);       // testComplete flag is only used to record the result
        be.EmitMov(testComplete, be.Immed(BRIG_TYPE_B1, 0));    // at first successful 'synchronized-with' attempt

        if (ArraySegment(0) == BRIG_SEGMENT_GROUP ||
            ArraySegment(1) == BRIG_SEGMENT_GROUP) 
        {
            assert(testKind == TEST_KIND_WAVE || testKind == TEST_KIND_WGROUP);
            Comment("Make sure all workitems have completed initialization before starting test code", 
                    "This is important because otherwise some workitems may see uninitialized values");
            MemFence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_WORKGROUP);
            Barrier(testKind == TEST_KIND_WAVE);
            MemFence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_WORKGROUP);
        }

        Comment("This is the pair of instructions which should not be reordered");
        if (isPlainOp[0]) PlainSt(0); else AtomicSt(0); // instruction A
        if (FenceRequired()) Fence();
        if (isPlainOp[1]) PlainSt(1); else AtomicSt(1); // instruction B

        //NB: execution model for workitems within a wave or workgroup is not defined.
        //    The code below attempts to synchronize with the second write (instruction B)
        //    executing a limited number of iterations. If this attempt fails, the code
        //    waits at barrier and does one more synchrinization attempt which should
        //    succeed regardless of used execution model.

        StartLoop();

            TypedReg synchronizedWith = CheckResult(testComplete);  // instructions C and D

            Comment("Update 'testComplete' flag");
            Or(testComplete, testComplete, synchronizedWith);

        EndLoop();

        if (testKind == TEST_KIND_WAVE || testKind == TEST_KIND_WGROUP)
        {
            Comment("this is the last attempt to synchronize with the second write",
                    "make sure all workitems within a workgroup have completed writing test data");
            if (!isValidTestOrder(memoryOrder[1], BRIG_MEMORY_ORDER_RELAXED)) MemFence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_WORKGROUP);
            Barrier(testKind == TEST_KIND_WAVE);
            MemFence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_WORKGROUP);

            CheckResult(testComplete);
        }
    }

    TypedReg CheckResult(TypedReg testComplete)
    {
        Comment("Load test values");
        TypedReg sync = AtomicLd(1);
        TypedReg res  = isPlainOp[0]? PlainLd(0) : AtomicLd(0); //F

        Comment("Compare test values with expected values");
        TypedReg synchronizedWith = COND(sync, EQ, ExpectedValue(1));
        TypedReg isResSet         = COND(res,  EQ, ExpectedValue(0));

        Comment("Set test result flag");
        TypedReg ok = And(Not(testComplete), And(synchronizedWith, isResSet));

        Comment("Update result in output array");
        TypedReg result = CondAssign(BRIG_TYPE_U32, RES_VAL_PASSED, RES_VAL_FAILED, ok);
        SetRes(BRIG_ATOMIC_OR, result->Reg());

        return synchronizedWith;
    }

    void Fence()
    {
        be.EmitMemfence(FenceOrder(), FenceScope(), FenceScope(), BRIG_MEMORY_SCOPE_NONE);
    }

    // ========================================================================
    // Helper code for array access

    PointerReg LoadArrayAddr(unsigned idx)
    {
        assert(idx <= 1);

        if (!testArrayAddr[idx])
        {
            Comment("Load array address");
            testArrayAddr[idx] = be.AddAReg(testArray[idx].segment());
            be.EmitLda(testArrayAddr[idx], testArray[idx]);
            if (segment[idx] == BRIG_SEGMENT_FLAT && ArraySegment(idx) == BRIG_SEGMENT_GROUP) // NB: conversion is not required for global segment
            {
                PointerReg flat = be.AddAReg(BRIG_SEGMENT_FLAT);
                be.EmitStof(flat, testArrayAddr[idx]);
                testArrayAddr[idx] = flat;
            }
        }
        return testArrayAddr[idx];
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

    void InitArray(unsigned idx)
    {
        assert(idx <= 1);

        if (ArraySegment(idx) == BRIG_SEGMENT_GROUP)
        {
            Comment("Init array element");
        
            OperandAddress target = TargetAddr(LoadArrayAddr(idx), ArrayIndex(idx, 0), type[idx]);
            InstAtomic inst = AtomicInst(type[idx], BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_SC_RELEASE, memoryScope[idx], segment[idx], equivClass[idx], false);
            inst.operands() = be.Operands(target, Initializer(idx));
        }
    }

    void InitRes()
    {
        Comment("Clear result array");
        SetRes(BRIG_ATOMIC_ST, be.Immed(type2bitType(ResultType()), RES_VAL_FAILED));
    }

    void SetRes(BrigAtomicOperation op, Operand res)
    {
        OperandAddress target = TargetAddr(LoadResAddr(), ResIndex(), ResultType());
        InstAtomic inst = AtomicInst(ResultType(), op, BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_AGENT, (BrigSegment)LoadResAddr()->Segment(), 0, false);
        inst.operands() = be.Operands(target, res);
    }

    TypedReg ResIndex()
    {
        if (!indexInResArray)
        {
            Comment("Init result array index");
            indexInResArray = TestAbsId(LoadResAddr()->IsLarge());
        }
        return indexInResArray;
    }

    TypedReg TestIndex(unsigned idx)
    {
        assert(idx <= 1);

        if (ArraySegment(idx) == BRIG_SEGMENT_GLOBAL)
            return TestAbsId(LoadArrayAddr(idx)->IsLarge());
        else
            return TestId(LoadArrayAddr(idx)->IsLarge());
    }

    TypedReg ArrayIndex(unsigned idx, unsigned access)
    {
        assert(idx <= 1);
        assert(access <= 1);

        if (!indexInTestArray[idx][access])
        {
            if (access == 0)
            {
                if (idx == 0) Comment("Init first write array index"); else Comment("Init second write array index");
                indexInTestArray[idx][access] = TestIndex(idx);
            }
            else
            {
                if (idx == 0) Comment("Init first read array index"); else Comment("Init second read array index");

                TypedReg id = TestIndex(idx);

                if (testKind == TEST_KIND_AGENT)
                {
                    assert(ArraySegment(idx) == BRIG_SEGMENT_GLOBAL);

                    // index == (id < wg.size)? id : id - wg.size;
                    TypedReg testId = Sub(id, Delta());
                    indexInTestArray[idx][access] = CondAssign(id, testId, COND(id, LT, Delta()));
                }
                else if (testKind == TEST_KIND_WGROUP || testKind == TEST_KIND_WAVE)
                {
                    // index == ((id % test.size) < delta)? id + test.size - delta : id - delta;
                    TypedReg localId = Rem(id, TestSize());
                    TypedReg testId1 = Add(id, TestSize() - Delta());
                    TypedReg testId2 = Sub(id, Delta());
                    indexInTestArray[idx][access] = CondAssign(testId1, testId2, COND(localId, LT, Delta()));
                }
                else
                {
                    assert(false);
                }
            }
        }
        return indexInTestArray[idx][access];
    }

    // ========================================================================
    // Helper loop code

    void InitLoop()
    {
        if (testKind == TEST_KIND_WAVE || testKind == TEST_KIND_WGROUP)
        {
            Comment("Init loop index");

            loopIdx = be.AddTReg(BRIG_TYPE_U32);
            be.EmitMov(loopIdx, be.Immed(BRIG_TYPE_U32, MAX_LOOP));
        }
    }
    
    void StartLoop()
    {
        be.EmitLabel(LAB_NAME);
    }

    void EndLoop()
    {
        if (testKind == TEST_KIND_AGENT)
        {
            Comment("Check if all workitems in the previous workgroup have completed");
            TypedReg cnt = LdWgComplete();
            TypedReg cond = COND(cnt, LT, geometry->WorkgroupSize());
            be.EmitCbr(cond, LAB_NAME);

            Comment("Increment number of completed workitems in the current workgroup");
            IncWgComplete();
        }
        else
        {
            Comment("Decrement loop index and continue if not zero");

            Sub(loopIdx, loopIdx, 1);
            TypedReg cReg = be.AddTReg(BRIG_TYPE_B1);
            be.EmitCmp(cReg->Reg(), loopIdx, be.Immed(loopIdx->Type(), 0), BRIG_COMPARE_NE);
            be.EmitCbr(cReg, LAB_NAME, BRIG_WIDTH_ALL);
        }
    }

    // ========================================================================
    // Helper code for working with wgComplete array

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

        InstAtomic inst = AtomicInst(t, BRIG_ATOMIC_LD, BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_AGENT, BRIG_SEGMENT_GLOBAL, 0);
        inst.operands() = operands;

        return atomicDst;
    }

    void IncWgComplete()
    {
        TypedReg id = TestWgId(LoadWgCompleteAddr()->IsLarge());

        ItemList operands;

        TypedReg src0 = be.AddTReg(WG_COMPLETE_TYPE);
        be.EmitMov(src0, be.Immed(WG_COMPLETE_TYPE, 1));

        OperandAddress target = TargetAddr(LoadWgCompleteAddr(), Add(id, 1), WG_COMPLETE_TYPE);

        operands.push_back(target);
        operands.push_back(src0->Reg());

        InstAtomic inst = AtomicInst(WG_COMPLETE_TYPE, BRIG_ATOMIC_ADD, BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE, BRIG_MEMORY_SCOPE_AGENT, BRIG_SEGMENT_GLOBAL, 0, false);
        inst.operands() = operands;
    }

    // ========================================================================
    // Detection of fence attributes

    bool FenceRequired() const { return !isValidTestOrder(memoryOrder[0], memoryOrder[1]); }

    BrigMemoryOrder FenceOrder() const
    {
        assert(FenceRequired());

        if (isPlainOp[0]) 
        {
            return isBitType(type[1])? BRIG_MEMORY_ORDER_SC_RELEASE : 
                                       BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE;
        }
        else
        {
            return isBitType(type[1])?    BRIG_MEMORY_ORDER_SC_ACQUIRE :
                   isSignedType(type[1])? BRIG_MEMORY_ORDER_SC_RELEASE : 
                                          BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE;
        }
    }

    BrigMemoryScope FenceScope() const
    {
        assert(FenceRequired());

        switch(testKind)
        {
        case TEST_KIND_WAVE:	return BRIG_MEMORY_SCOPE_WAVEFRONT;
        case TEST_KIND_WGROUP:	return BRIG_MEMORY_SCOPE_WORKGROUP;
        case TEST_KIND_AGENT:	return BRIG_MEMORY_SCOPE_AGENT;
        default:
            assert(false);
            return BRIG_MEMORY_SCOPE_NONE;
        }
    }

    // ========================================================================
    // Validation of test attributes

    bool IsValid() const
    {
        if (isPlainOp[0])
        {
            if (!IsValidPlainStTest(0)) return false;
        }
        else
        {
            if (!IsValidAtomicTest(0)) return false;
        }

        if (isPlainOp[1])
        {
            if (!IsValidPlainStTest(1)) return false;
        }
        else
        {
            if (!IsValidAtomicTest(1)) return false;
        }

        if(!isValidTestSegment(0)) return false;
        if(!isValidTestSegment(1)) return false;

        if(!isValidTestScope(0)) return false;
        if(!isValidTestScope(1)) return false;

        return true;
    }

    bool isValidTestSegment(unsigned idx) const
    {
        assert(idx <= 1);
        if (testKind == TEST_KIND_AGENT) return ArraySegment(idx) != BRIG_SEGMENT_GROUP;
        return true;
    }

    bool isValidTestScope(unsigned idx)  const
    {
        assert(idx <= 1);
        if (testKind == TEST_KIND_WGROUP) return memoryScope[idx] != BRIG_MEMORY_SCOPE_WAVEFRONT;
        if (testKind == TEST_KIND_AGENT)  return memoryScope[idx] != BRIG_MEMORY_SCOPE_WAVEFRONT &&
                                                 memoryScope[idx] != BRIG_MEMORY_SCOPE_WORKGROUP;
        return true;
    }

    bool IsValidPlainStTest(unsigned idx) const
    {
        assert(idx <= 1);
        BrigMemoryScope scope = (ArraySegment(idx) == BRIG_SEGMENT_GROUP)? BRIG_MEMORY_SCOPE_WORKGROUP : BRIG_MEMORY_SCOPE_AGENT;
        return isValidStType(type[idx]) &&
               atomicOp[idx]    == BRIG_ATOMIC_ST &&
               memoryOrder[idx] == BRIG_MEMORY_ORDER_RELAXED &&
               memoryScope[idx] == scope &&
               atomicNoRet[idx];
    }

    bool isValidStType(BrigType t) const
    {
        return isSignedType(t) || 
               isUnsignedType(t) ||
               isFloatType(t) ||
               t == BRIG_TYPE_B128;
    }

    bool isValidTestOrder(BrigMemoryOrder order0, BrigMemoryOrder order1) const
    {
        switch(order0)
        {
        case BRIG_MEMORY_ORDER_RELAXED:             return order1 == BRIG_MEMORY_ORDER_SC_RELEASE || 
                                                           order1 == BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE;
        case BRIG_MEMORY_ORDER_SC_RELEASE:          return order1 != BRIG_MEMORY_ORDER_RELAXED;
        case BRIG_MEMORY_ORDER_SC_ACQUIRE:          return true;
        case BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE:  return true;
        default:
            assert(false);
            return false;
        }
    }

    bool IsValidAtomicTest(unsigned idx) const
    {
        assert(idx <= 1);

        if (!IsValidAtomic(atomicOp[idx], segment[idx], memoryOrder[idx], memoryScope[idx], type[idx], atomicNoRet[idx])) return false;
        if (!IsValidGrid(idx)) return false;

        if (atomicOp[idx] == BRIG_ATOMIC_LD) return false;
        if (memoryScope[idx] == BRIG_MEMORY_SCOPE_SYSTEM) return false;

        return true;
    }

    bool IsValidGrid(unsigned idx) const
    {
        assert(idx <= 1);

        switch (atomicOp[idx])
        {
        case BRIG_ATOMIC_AND:
        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:
            return getBrigTypeNumBits(type[idx]) == TestSize();
            
        default:
            return true;
        }
    }

}; // class AtomicTest

unsigned MModelTest::wavesize;

//=====================================================================================

#ifdef QUICK_TEST

void MModelTests::Iterate(hexl::TestSpecIterator& it)
{
    CoreConfig* cc = CoreConfig::Get(context);
    MModelTest::wavesize = cc->Wavesize(); //F: how to get the value from inside of AtomicTest?
    Arena* ap = cc->Ap();
    TestForEach<MModelTest>(ap, it, "mmodel", 
                            cc->Grids().MModelSet(),                                            // grid
                            cc->Memory().LimitedAtomics(),                                      // op
                            cc->Segments().Atomic(),        cc->Segments().Atomic(),            // segment
                            cc->Memory().AllMemoryOrders(), cc->Memory().AllMemoryOrders(),     // order
                            cc->Memory().AllMemoryScopes(), cc->Memory().AllMemoryScopes(),     // scope
                            cc->Types().MemModel(),                                             // type
                            Bools::All()                                                        // isPlainOp_0
                            );
}

#else 

void MModelTests::Iterate(hexl::TestSpecIterator& it)
{
    CoreConfig* cc = CoreConfig::Get(context);
    MModelTest::wavesize = cc->Wavesize(); //F: how to get the value from inside of AtomicTest?
    Arena* ap = cc->Ap();
    TestForEach<MModelTest>(ap, it, "mmodel", 
                            cc->Grids().MModelSet(),                                            // grid
                            cc->Memory().AllAtomics(),      cc->Memory().AllAtomics(),          // op
                            cc->Segments().Atomic(),        cc->Segments().Atomic(),            // segment
                            cc->Memory().AllMemoryOrders(), cc->Memory().AllMemoryOrders(),     // order
                            cc->Memory().AllMemoryScopes(), cc->Memory().AllMemoryScopes(),     // scope
                            cc->Types().Atomic(),           cc->Types().Atomic(),               // type
                            Bools::All()                                                        // isPlainOp_0
                            );
}

#endif
//=====================================================================================

} // namespace hsail_conformance
