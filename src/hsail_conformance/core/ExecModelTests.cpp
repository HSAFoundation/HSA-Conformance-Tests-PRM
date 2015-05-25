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

//=====================================================================================
//=====================================================================================
//=====================================================================================
// OVERVIEW
//
// This is a set of tests that check compliance with execution model requirements
//
// The purpose of this code is to test the result of execution of several
// workgroups which have one-way data dependencies. 
// 
// According to PRM, "any program can count on one-way communication and later work-groups
// (in work-group flattened ID order) can wait for values written by
// earlier work-groups without deadlock."
//
//=====================================================================================
// DETAILED DESCRIPTION OF TESTS
//
// Legend:
//  - wi.id:   workitemflatabsid
//  - wg.id:   workgroupid(0)
//  - wg.size:  workgroup size in X dimension 
//  - grid.size:  grid size in X dimension 
//
//=====================================================================================
// TEST STRUCTURE 
//
//                                                  // Array used to check if all workitems 
//                                                  // in the previous workgroup have finished
//                                                  // When workitem i finishes, it increments 
//                                                  // element at index i+1.
//                                                  // First element is initialized to ensure 
//                                                  // completion of first group.
//
//    unsigned global finished[grid.size / wg.size + 1] = {wg.size, 0, 0, ...};
//    
//    kernel(unsigned global ok[grid.size])         // output array
//    {
//        ok[wi.id] = 0;                            // clear result flag
//        
//        do {} while (finished[wg.id] < wg.size);  // wait for previous wg to complete
//
//        finished[wg.id + 1]++;                    // Label this wi as completed
//        ok[wi.id] = 1;                            // set 'passed' flag
//    }
// 
//=====================================================================================

#include "ExecModelTests.hpp"
#include "AtomicTestHelper.hpp"

namespace hsail_conformance {

//=====================================================================================

class ExecModelTest : public AtomicTestHelper
{
private:
    static const BrigType RES_TYPE       = BRIG_TYPE_U32; // Type of elements in output array
    static const unsigned RES_VAL_FAILED = 0;
    static const unsigned RES_VAL_PASSED = 1;

private:
    PointerReg          resArrayAddr;
    TypedReg            indexInResArray;

//=====================================================================================
public:

    ExecModelTest(Grid geometry)
    : AtomicTestHelper(KERNEL, geometry),
        resArrayAddr(0),
        indexInResArray(0)
    {
        testKind = TEST_KIND_AGENT;
    }

    // ========================================================================
    // Test Name

    void Name(std::ostream& out) const 
    { 
        out << "ExecModel/" << geometry; 
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
        Comment("Testing execution model for workgroups");

        DefineWgCompletedArray();
    }

    // ========================================================================
    // Kernel code

    void KernelCode()
    {
        assert(codeLocation == emitter::KERNEL);
        
        LoadResAddr();
        LoadWgCompleteAddr();

        ResIndex();

        InitRes();

        StartLoop();
        EndLoop();

        ResOk();
    }

    // ========================================================================
    // Helper code for array access

    PointerReg LoadResAddr()
    {
        if (!resArrayAddr)
        {
            Comment("Load result address");
            resArrayAddr = output->Address();
        }
        return resArrayAddr;
    }

    void InitRes()
    {
        Comment("Clear result array");
        SetRes(BRIG_ATOMIC_ST, be.Immed(type2bitType(ResultType()), RES_VAL_FAILED));
    }

    void ResOk()
    {
        Comment("Set 'PASSED' flag in result array");
        SetRes(BRIG_ATOMIC_ST, be.Immed(type2bitType(ResultType()), RES_VAL_PASSED));
    }

    void SetRes(BrigAtomicOperation op, Operand res)
    {
        OperandAddress target = TargetAddr(LoadResAddr(), ResIndex(), ResultType());
        InstAtomic inst = Atomic(ResultType(), op, BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_AGENT, (BrigSegment)LoadResAddr()->Segment(), 0, false);
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

    // ========================================================================
    // Helper loop code

    void StartLoop()
    {
        be.EmitLabel(LAB_NAME);
    }

    void EndLoop()
    {
        CheckPrevWg();
    }

    // ========================================================================
    // Validation of test attributes

    bool IsValid() const
    {
        return true;
    }

}; // class ExecModelTest

//=====================================================================================

void ExecModelTests::Iterate(hexl::TestSpecIterator& it)
{
    CoreConfig* cc = CoreConfig::Get(context);
    ExecModelTest::wavesize = cc->Wavesize(); //F: how to get the value from inside of AtomicTest?
    Arena* ap = cc->Ap();
    TestForEach<ExecModelTest>(ap, it, "execmodel", cc->Grids().EModelSet());
}

//=====================================================================================

} // namespace hsail_conformance

// TODO
// - base initial values on id
