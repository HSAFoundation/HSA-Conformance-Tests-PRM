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
// This is a set of tests that check compliance with memory model requirements
//
// The purpose of this code is to test most "happens-before" (HB) and "synchronizes-with"
// (SYNC) scenarios. Each of these scenarios involve read/write pairs (R and W) 
// of instructions executed by different workitems w1 and w2:
//
//                  (w1)                         (w2)
//                                              
//                   |                            |
//                   |      happens-before        |
//                 HB-W   <-----------------------|----------
//                   |                            |         |
//                  F-W     synchronizes-with     |         |
//                 SYNC-W <------------------  SYNC-R       |
//                   |                           F-R        |
//                   |                            |         |
//                   |                          HB-R --------
//                   |                            |
//                  ...                          ...
// 
// Write instructions are executed by one workitem (w1), results are 
// inspected by another workitem (w2). The test expects that a value X written by HB-W 
// operation and a value Y read by HB-R operation are the same (X=Y).
//
//=====================================================================================
// ATTRIBUTES
//
// The following table enumerates valid test attributes for each operation
//
// SYNC-W: an atomic store (or rmw atomic)
//     --------------------------------------------------------------------------------
//      prop      arg              valid values
//     --------------------------------------------------------------------------------
//      op      = sync_op       -- any (except for LD)
//      order   = sync_order    -- any
//      scope   = sync_scope    -- any (consistent with test kind and sync segment)
//      seg     = sync_seg      -- any (consistent with test kind)
//     --------------------------------------------------------------------------------
// 
// SYNC-R: an atomic load (or rmw atomic)
//     --------------------------------------------------------------------------------
//      prop      arg              valid values
//     --------------------------------------------------------------------------------
//      op      = ld            -- (actually any operation which does not change memory)
//      order   = sync_order_r  -- any
//      scope   = sync_scope    -- (may be wider)
//      seg     = sync_seg 
//     --------------------------------------------------------------------------------
// 
// F-W: memory fence (required if SYNC-W.order != rel/ar)
//     --------------------------------------------------------------------------------
//      prop      arg              valid values
//     --------------------------------------------------------------------------------
//      order  = rel/ar
//      scope  = scope1         -- any (consistent with test type)
//     --------------------------------------------------------------------------------
// 
// F-R: memory fence (required if SYNC-R.order != acq/ar)
//     --------------------------------------------------------------------------------
//      prop      arg              valid values
//     --------------------------------------------------------------------------------
//      order  = acq/ar
//      scope  = scope1         -- any (consistent with test type)
//     --------------------------------------------------------------------------------
// 
// HB-W: an plain store or an atomic store (or rmw atomic)
//     --------------------------------------------------------------------------------
//      prop      arg              valid values
//     --------------------------------------------------------------------------------
//      op      = hb_op         -- any (except for LD)
//      order   = hb_order      -- any 
//      scope   = hb_scope      -- any (consistent with hb segment)
//      seg     = hb_seg        -- any (consistent with test kind)
//     --------------------------------------------------------------------------------
// 
// HB-R: an plain load or an atomic load (or rmw atomic)
//     --------------------------------------------------------------------------------
//      prop      arg              valid values
//     --------------------------------------------------------------------------------
//      op      = ld            -- any (except for LD)
//      order   = hb_order_r    -- any 
//      scope   = hb_scope_r    -- any (consistent with hb segment)
//      seg     = hb_seg        -- 
//     --------------------------------------------------------------------------------
// 
//=====================================================================================
// GENERIC TEST STRUCTURE
//
// Each workitem in this test does the following:
//  1. prepare test data at write index:                                      (workitem w1)
//      - write test data: hb_array[wi.id] = T0                                   (instruction HB-W)
//      - execute a memory fence if required for 'synchronizes-with' scenario     (instruction F-W)
//      - write synchronization data: sync_array[wi.id] = T1                      (instruction SYNC-W)
//  2. read test data written by another workitem at read index i:            (workitem w2)
//      - read synchronization data: H1 = sync_array[i]                           (instruction SYNC-R)
//      - execute a memory fence if required for 'synchronizes-with' scenario     (instruction F-R)
//      - if (H1=expected) read test data: H0 = hb_array0[i]                      (instruction HB-R)
//  3. validate test data H0 read on the previous step
//  
//=====================================================================================
// TEST KINDS
//
// There are the following types of this test:
//  1. WAVE kind.
//     Items within a wave test data written by other items within the same wave.
//  2. WGROUP kind.
//     Items within a wave test data written by other items within the same workgroup but in another wave.
//  3. AGENT kind.
//     Items within a workgroup N+1 (N>0) test data written by items within the workgroup N.
//     Items within a workgroup 0 test data written by itself.
//
//=====================================================================================
// DETAILED DESCRIPTION OF TESTS
//
// Legend:
//  - wi.id:      workitemflatabsid
//  - wg.id:      workgroupid(0)
//  - wg.size:    workgroup size in X dimension 
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
//
// Interface functions:
//
//  <type>   InitialValue(unsigned arrayId)    initial value
//  TypedReg Operand(unsigned arrayId)         generate code for the first source operand of store instruction
//  TypedReg Operand1(unsigned arrayId)        generate code for the second source operand of store instruction 
//  <type>   ExpectedValue(unsigned arrayId)   expected value
//
//=====================================================================================
// TEST STRUCTURE FOR WAVE AND WGROUP TEST KINDS
//
//                                                  // Define and initialize test arrays
//                                                  // NB: initialization is only possible for arrays in GLOBAL segment
//    <hb_type>   <hb_seg>   hb_array  [(hb_seg   == GROUP)? wg.size : grid.size] = {InitialValue(HB),   InitialValue(HB),   ...}; 
//    <sync_type> <sync_seg> sync_array[(sync_seg == GROUP)? wg.size : grid.size] = {InitialValue(SYNC), InitialValue(SYNC), ...}; 
//    
//    kernel(unsigned global ok[grid.size])         // output array
//    {
//        private unsigned testId = ((wi.id % test.size) < delta)? wi.id + test.size - delta : wi.id - delta;
//
//                                                  // testComplete flag is used to record only result
//                                                  // at first successful 'synchronized-with' attempt
//        private bool testComplete = 0;
//
//                                                  // syncWith flag is set at first successful 
//                                                  // 'synchronized-with' attempt
//        private bool syncWith = false;
//        private bool resultOk = false;
//        private bool passed   = false;
//
//        private loopIdx = MAX_LOOP_CNT;
//
//        hb_array  [wi.id] = InitialValue(HB);     // init test array (for group segment only)
//        sync_array[wi.id] = InitialValue(SYNC); 
//
//        ok[wi.id] = 0;                            // clear result flag
//        
//                                                  // make sure all workitems have completed initialization
//                                                  // This is important because otherwise some workitems 
//                                                  // may see uninitialized values
//        
//        memfence_screl_wg;
//        (wave)barrier;
//        memfence_scacq_wg;
//                                                  // this is a part of "happens-before" code
//                                                  // this instruction should "happen-before" HB-R                    
//        StoreOp(hb_op, &hb_array[wi.id], Operand(HB), Operand1(HB));         // HB-W
//
//                                                  // this is a part of 'synchronized-with' code
//        optional memory_fence;                                               // F-W
//        StoreOp(sync_op, &sync_array[wi.id], Operand(SYNC), Operand1(SYNC)); // SYNC-W
//
//                                                  //NB: execution model for workitems within a wave 
//                                                  //    or workgroup is not defined. The code below 
//                                                  //    attempts to synchronize with SYNC-W executing
//                                                  //    a limited number of iterations. If this attempt 
//                                                  //    fails, the code waits at barrier and does one 
//                                                  //    more synchrinization attempt which should
//                                                  //    succeed regardless of used execution model.
//
//        do
//        {
//            syncWith = (sync_array[testId] == ExpectedValue(SYNC));               // SYNC-R
//            optional memory_fence;                                                // F-R
//            if (syncWith) resultOk = (hb_array[testId] == ExpectedValue(HB));     // HB-R
//            passed |= (!testComplete && syncWith && resultOk)? PASSED : FAILED;
//            testComplete |= syncWith;
//        }
//        while (--loopIdx != 0);
//
//                                                  // make sure all workitems have completed writing test data.
//                                                  // note that (wave)barrier below cannot be removed because 
//                                                  // it is the only way to ensure (pseudo) parallel execution.
//        
//        memfence_screl_wg;
//        (wave)barrier;
//        memfence_scacq_wg;
//        
//                                                  // This is the last attempt to synchronize-with SYNC-W
//        
//        syncWith = (sync_array[testId] == ExpectedValue(SYNC));               // SYNC-R
//        optional memory_fence;                                                // F-R
//        if (syncWith) resultOk = (hb_array[testId] == ExpectedValue(HB));     // HB-R
//        passed |= (!testComplete && syncWith && resultOk)? PASSED : FAILED;
//
//        ok[wi.id] = passed;
//    }
// 
//=====================================================================================
// TEST STRUCTURE FOR AGENT TEST KIND
//
//                                                  // Define and initialize test arrays
//    <hb_type>   GLOBAL hb_array  [grid.size] = {InitialValue(HB),   InitialValue(HB),   ...}; 
//    <sync_type> GLOBAL sync_array[grid.size] = {InitialValue(SYNC), InitialValue(SYNC), ...}; 
//
//                                                  // Array used to check if all workitems in the previous 
//                                                  // workgroup have finished. When workitem i finishes, 
//                                                  // it increments element at index i+1. First element 
//                                                  // is initialized to ensure completion of first group
//    unsigned global finished[grid.size / wg.size + 1] = {wg.size, 0, 0, ...};
//    
//    kernel(unsigned global ok[grid.size])         // output array
//    {
//                                                  // workitems in first workgroup test data written by itself;
//                                                  // workitems in other workgroups test data written 
//                                                  // by items in previous workgroups
//        private unsigned testId = (wi.id < wg.size)? wi.id : wi.id - wg.size;
//
//                                                  // testComplete flag is used to only record result
//                                                  // at first successful 'synchronized-with' attempt
//        private bool testComplete = 0;
//
//                                                  // syncWith flag is set at first successful 
//                                                  // 'synchronized-with' attempt
//        private bool syncWith = false;
//        private bool resultOk = false;
//        private bool passed   = false;
//
//        ok[wi.id] = 0;                            // clear result flag
//
//                                                  // this is a part of "happens-before" code
//                                                  // this instruction should "happen-before" HB-R                    
//        StoreOp(hb_op, &hb_array[wi.id], Operand(HB), Operand1(HB));         // HB-W
//
//                                                  // this is a part of 'synchronized-with' code
//        memory_fence;                                                        // F-W
//        StoreOp(sync_op, &sync_array[wi.id], Operand(SYNC), Operand1(SYNC)); // SYNC-W
//        
//        do                                        // try synchronizing with SYNC-W 
//        {
//            syncWith = (sync_array[testId] == ExpectedValue(SYNC));               // SYNC-R
//            optional memory_fence;                                                // F-R
//            if (syncWith) resultOk = (hb_array[testId] == ExpectedValue(HB));     // HB-R
//            passed |= (!testComplete && syncWith && resultOk)? PASSED : FAILED;
//            testComplete |= syncWith;
//        }
//        while (finished[wg.id] < wg.size);        // wait for previous wg to complete
//
//        finished[wg.id + 1]++;                    // Label this wi as completed
//
//                                                  // This is the last attempt to synchronize-with SYNC-W
//        
//        syncWith = (sync_array[testId] == ExpectedValue(SYNC));               // SYNC-R
//        optional memory_fence;                                                // F-R
//        if (syncWith) resultOk = (hb_array[testId] == ExpectedValue(HB));     // HB-R
//        passed |= (!testComplete && syncWith && resultOk)? PASSED : FAILED;
//
//        ok[wi.id] = passed;
//    }
// 
//=====================================================================================
// TODO:
//
// Add tests:
// - with barrier
// - for seq consistency (visible in the same order to all workitems)
// - asynch data exchange using non-synchronized atomics
//
//=====================================================================================

#include "MModelTests.hpp"
#include "AtomicTestHelper.hpp"

using std::string;
using std::ostringstream;

namespace hsail_conformance {

#define QUICK_TEST
//#define TINY_TEST

//=====================================================================================

enum
{
    WRITE_IDX   = 0,
    READ_IDX    = 1,
    ACCESS_NUM
};

//=====================================================================================

class MModelTestProp : public TestProp
{
private:
    unsigned accessIdx;

protected:
    TypedReg Idx() const { return TestProp::Idx(arrayId, accessIdx); }

public:
    virtual TypedReg InitialValue()               { accessIdx = WRITE_IDX; return InitialVal();    }
    virtual uint64_t InitialValue(unsigned idx)   { accessIdx = WRITE_IDX; return InitialVal(idx); }
    virtual TypedReg ExpectedValue(unsigned acc)  { accessIdx = acc;       return ExpectedVal();   }
    virtual TypedReg AtomicOperand()              { accessIdx = WRITE_IDX; return Operand();       }
    virtual TypedReg AtomicOperand1()             { accessIdx = WRITE_IDX; return Operand1();      }

protected:
    virtual uint64_t InitialVal(unsigned idx) const { assert(false); return 0; }
    virtual TypedReg InitialVal()             const { assert(false); return 0; }

    virtual TypedReg Operand()                const { assert(false); return 0; }
    virtual TypedReg Operand1()               const {                return 0; }

    virtual TypedReg ExpectedVal()            const { assert(false); return 0; }
};

//=====================================================================================

class MModelTestPropAdd : public MModelTestProp // ******* BRIG_ATOMIC_ADD *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx; }
    virtual TypedReg InitialVal()                 const { return Idx(); }

    virtual TypedReg Operand()                    const { return Idx(); }

    virtual TypedReg ExpectedVal()                const { return Mul(Idx(), 2); }
};

class MModelTestPropSub : public MModelTestProp // ******* BRIG_ATOMIC_SUB *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx * 2; }
    virtual TypedReg InitialVal()                 const { return Mul(Idx(), 2); }

    virtual TypedReg Operand()                    const { return Idx(); }

    virtual TypedReg ExpectedVal()                const { return Idx(); }
};

class MModelTestPropOr : public MModelTestProp // ******* BRIG_ATOMIC_OR *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx * 2; }
    virtual TypedReg InitialVal()                 const { return Mul(Idx(), 2); }

    virtual TypedReg Operand()                    const { return Mov(1); }

    virtual TypedReg ExpectedVal()                const { return Add(Mul(Idx(), 2), 1); }
};

class MModelTestPropXor : public MModelTestProp // ******* BRIG_ATOMIC_XOR *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx * 2; }
    virtual TypedReg InitialVal()                 const { return Mul(Idx(), 2); }

    virtual TypedReg Operand()                    const { return Mov(1); }

    virtual TypedReg ExpectedVal()                const { return Add(Mul(Idx(), 2), 1); }
};

class MModelTestPropAnd : public MModelTestProp // ******* BRIG_ATOMIC_AND *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx + 0xFF000000; }
    virtual TypedReg InitialVal()                 const { return Add(Idx(), 0xFF000000); }

    virtual TypedReg Operand()                    const { return Idx(); }

    virtual TypedReg ExpectedVal()                const { return Idx(); }
};

class MModelTestPropWrapinc : public MModelTestProp // ******* BRIG_ATOMIC_WRAPINC *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx; }
    virtual TypedReg InitialVal()                 const { return Idx(); }

    virtual TypedReg Operand()                    const { return Mov(-1); }     // max value

    virtual TypedReg ExpectedVal()                const { return Add(Idx(), 1); }
};

class MModelTestPropWrapdec : public MModelTestProp // ******* BRIG_ATOMIC_WRAPDEC *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx + 1; }
    virtual TypedReg InitialVal()                 const { return Add(Idx(), 1); }

    virtual TypedReg Operand()                    const { return Mov(-1); }     // max value

    virtual TypedReg ExpectedVal()                const { return Idx(); }
};

class MModelTestPropMax : public MModelTestProp // ******* BRIG_ATOMIC_MAX *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx; }
    virtual TypedReg InitialVal()                 const { return Idx(); }

    virtual TypedReg Operand()                    const { return Add(Idx(), 1); }

    virtual TypedReg ExpectedVal()                const { return Add(Idx(), 1); }
};

class MModelTestPropMin : public MModelTestProp // ******* BRIG_ATOMIC_MIN *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx + 1; }
    virtual TypedReg InitialVal()                 const { return Add(Idx(), 1); }

    virtual TypedReg Operand()                    const { return Idx(); }

    virtual TypedReg ExpectedVal()                const { return Idx(); }
};

class MModelTestPropExch : public MModelTestProp // ******* BRIG_ATOMIC_EXCH *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx; }
    virtual TypedReg InitialVal()                 const { return Idx(); }

    virtual TypedReg Operand()                    const { return Mul(Idx(), 2); }

    virtual TypedReg ExpectedVal()                const { return Mul(Idx(), 2); }
};

class MModelTestPropCas : public MModelTestProp // ******* BRIG_ATOMIC_CAS *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx; }
    virtual TypedReg InitialVal()                 const { return Idx(); }

    virtual TypedReg Operand()                    const { return InitialVal(); }   // value which is being compared
    virtual TypedReg Operand1()                   const { return Mul(Idx(), 2); }  // value to swap

    virtual TypedReg ExpectedVal()                const { return Mul(Idx(), 2); }
};

class MModelTestPropSt : public MModelTestProp // ******* BRIG_ATOMIC_ST *******
{
protected:
    virtual uint64_t InitialVal(unsigned idx)     const { return idx; }
    virtual TypedReg InitialVal()                 const { return Idx(); }

    virtual TypedReg Operand()                    const { return Mul(Idx(), 2); }

    virtual TypedReg ExpectedVal()                const { return Mul(Idx(), 2); }
};

class MModelTestPropLd : public MModelTestProp // ******* BRIG_ATOMIC_LD *******
{
};

//=====================================================================================

class MModelTestPropFactory : public TestPropFactory<MModelTestProp, 2>
{
public:
    MModelTestPropFactory(unsigned dim) : TestPropFactory<MModelTestProp, 2>(dim) {}

public:
    virtual MModelTestProp* CreateProp(BrigAtomicOperation op)
    {
        switch (op)
        {
        case BRIG_ATOMIC_ADD:      return new MModelTestPropAdd();    
        case BRIG_ATOMIC_AND:      return new MModelTestPropAnd();    
        case BRIG_ATOMIC_CAS:      return new MModelTestPropCas();    
        case BRIG_ATOMIC_EXCH:     return new MModelTestPropExch();   
        case BRIG_ATOMIC_MAX:      return new MModelTestPropMax();    
        case BRIG_ATOMIC_MIN:      return new MModelTestPropMin();    
        case BRIG_ATOMIC_OR:       return new MModelTestPropOr();     
        case BRIG_ATOMIC_ST:       return new MModelTestPropSt();     
        case BRIG_ATOMIC_SUB:      return new MModelTestPropSub();    
        case BRIG_ATOMIC_WRAPDEC:  return new MModelTestPropWrapdec();
        case BRIG_ATOMIC_WRAPINC:  return new MModelTestPropWrapinc();
        case BRIG_ATOMIC_XOR:      return new MModelTestPropXor();    
        case BRIG_ATOMIC_LD:       return new MModelTestPropLd();     

        default:
            assert(false);
            return 0;
        }
    }
};

//=====================================================================================

class MModelTest : public AtomicTestHelper
{
private:
    static const BrigType LOOP_IDX_TYPE  = BRIG_TYPE_U32; // Type of loop index
    static const BrigType RES_TYPE       = BRIG_TYPE_U32; // Type of elements in output array
    static const uint64_t RES_VAL_FAILED = 0;
    static const uint64_t RES_VAL_PASSED = 1;
    static const unsigned EQUIV          = 0;

private:
    enum // Indices of arrays used by this test
    {
        HB_ARRAY_ID     = 0,    // Array accessed by "happens-before" operations
        SYNC_ARRAY_ID   = 1,    // Array accessed by "synchronized-with" operations
        ARRAYS_NUM,

        MIN_ARRAY_ID    = 0,
        MAX_ARRAY_ID    = ARRAYS_NUM - 1
    };

private:
    static const unsigned MAX_LOOP = 1000;

private:                                // Properties of "happens-before" operations
    MModelTestProp*     writeHbOp;      // Write operation
    MemOpProp           readHbOp;       // Read operation

private:                                // Properties of "synchronized-with" operations
    MModelTestProp*     writeSyncOp;    // Write operation
    MemOpProp           readSyncOp;     // Read operation

private:                                // Properties of fences
    FenceOpProp         writeFence;     // Fence before "synchronized-with" write operation
    FenceOpProp         readFence;      // Fence after "synchronized-with" read operation

private:
    DirectiveVariable   testArray[ARRAYS_NUM];                      // Arrays
    PointerReg          testArrayAddr[ARRAYS_NUM];                  // Array addresses
    TypedReg            indexInTestArray[ARRAYS_NUM][ACCESS_NUM];   // Read and write indices in each array

private:
    PointerReg          resArrayAddr;       // Result array indicating test pass/fail status for each workitem
    TypedReg            indexInResArray;    // Index of current workitem in result array

private:
    bool                mapFlat2Group;      // if true, map flat to group, if false, map flat to global
    TypedReg            resultFlag;
    TypedReg            loopIdx;

public:

//=====================================================================================

#ifdef QUICK_TEST

    MModelTest(Grid                 geometry,
               BrigSegment          sync_seg,
               BrigMemoryOrder      sync_order,
               BrigMemoryScope      sync_scope,
               BrigType             sync_type,
               BrigAtomicOperation  hb_op,
               BrigSegment          hb_seg,
               BrigMemoryOrder      hb_order,
               BrigMemoryScope      hb_scope,
               bool                 hb_plain
               )
    : AtomicTestHelper(KERNEL, geometry),
        resArrayAddr(0),
        indexInResArray(0),
        resultFlag(0),
        loopIdx(0)
    {
        SetTestKind();

        //---------------------------------------------------------------------------------------
        // Set properties of synchronozes-with and happens-before r/w operations

        BrigAtomicOperation sync_op = ShuffleOp(hb_op);

        MemOpProp syncOp(sync_op, sync_seg, sync_order, sync_scope, sync_type, EQUIV, false, false,    SYNC_ARRAY_ID);
        MemOpProp hbOp  (hb_op,   hb_seg,   hb_order,   hb_scope,   sync_type, EQUIV, true,  hb_plain, HB_ARRAY_ID);

        EnsureValid(hbOp); // HB op and type are derived from sync and may require corrections

        writeHbOp = MModelTestPropFactory::Get(HB_ARRAY_ID)->GetProp(this, hbOp);

        readHbOp         = hbOp;
        readHbOp.op      = BRIG_ATOMIC_LD;
        readHbOp.order   = BRIG_MEMORY_ORDER_RELAXED;
        readHbOp.scope   = ShuffleLdScope(syncOp.scope, readHbOp.seg, readHbOp.isPlainOp);
        readHbOp.isNoRet = false;

        writeSyncOp = MModelTestPropFactory::Get(SYNC_ARRAY_ID)->GetProp(this, syncOp);

        readSyncOp         = syncOp;
        readSyncOp.op      = BRIG_ATOMIC_LD;
        readSyncOp.order   = ShuffleLdOrder(hbOp.order);
        readSyncOp.isNoRet = false;

        //---------------------------------------------------------------------------------------
        // Set properties of synchronozes-with fences

        if (!writeSyncOp->IsRelease()) writeFence.Release(writeSyncOp->scope);
        if (!readSyncOp.IsAcquire())   readFence.Acquire(readSyncOp.scope);

        //---------------------------------------------------------------------------------------

        mapFlat2Group = isBitType(sync_type);       // This is to minimize total number of tests

        for (unsigned id = MIN_ARRAY_ID; id <= MAX_ARRAY_ID; ++id)
        {
            indexInTestArray[id][WRITE_IDX] = 0;
            indexInTestArray[id][READ_IDX]  = 0;
            testArrayAddr[id] = 0;
        }
    }

    BrigAtomicOperation ShuffleOp(BrigAtomicOperation op)
    {
        switch (op)
        {
        case BRIG_ATOMIC_ADD:         return BRIG_ATOMIC_SUB;
        case BRIG_ATOMIC_AND:         return BRIG_ATOMIC_XOR;
        case BRIG_ATOMIC_CAS:         return BRIG_ATOMIC_OR; 
        case BRIG_ATOMIC_EXCH:        return BRIG_ATOMIC_CAS;
        case BRIG_ATOMIC_MAX:         return BRIG_ATOMIC_MIN;
        case BRIG_ATOMIC_ST:          return BRIG_ATOMIC_ST; 
        case BRIG_ATOMIC_WRAPINC:     return BRIG_ATOMIC_WRAPDEC;
        default:
            assert(false);
            return op;
        }
    }

    //NB: 'ar' is not supported for 'ld'
    BrigMemoryOrder ShuffleLdOrder(BrigMemoryOrder order, bool isPlain = false)
    {
        if (isPlain) return BRIG_MEMORY_ORDER_RELAXED;

        switch (order)
        {
        case BRIG_MEMORY_ORDER_RELAXED:               return BRIG_MEMORY_ORDER_SC_ACQUIRE;
        case BRIG_MEMORY_ORDER_SC_ACQUIRE:            return BRIG_MEMORY_ORDER_RELAXED;
        case BRIG_MEMORY_ORDER_SC_RELEASE:            return BRIG_MEMORY_ORDER_SC_ACQUIRE;
        case BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE:    return BRIG_MEMORY_ORDER_RELAXED;
        default:
            assert(false);
            return order;
        }
    }

    BrigMemoryScope ShuffleLdScope(BrigMemoryScope scope, BrigSegment seg, bool isPlain = false)
    {
        if (isPlain) return BRIG_MEMORY_SCOPE_NONE;

        switch (scope)
        {
        case BRIG_MEMORY_SCOPE_WAVEFRONT:   scope = BRIG_MEMORY_SCOPE_SYSTEM;
        case BRIG_MEMORY_SCOPE_WORKGROUP:   scope = BRIG_MEMORY_SCOPE_AGENT;
        case BRIG_MEMORY_SCOPE_AGENT:       scope = BRIG_MEMORY_SCOPE_WORKGROUP;
        case BRIG_MEMORY_SCOPE_SYSTEM:      scope = BRIG_MEMORY_SCOPE_WAVEFRONT;
        default:                            scope = BRIG_MEMORY_SCOPE_WAVEFRONT;
        }

        //NB: 'system' and 'agent' scopes are not supported for group segment
        if (seg == BRIG_SEGMENT_GROUP && 
            (scope == BRIG_MEMORY_SCOPE_AGENT || scope == BRIG_MEMORY_SCOPE_SYSTEM))
        {
            scope = BRIG_MEMORY_SCOPE_WAVEFRONT;
        }
        
        return scope;
    }

    // As quick test does not enumerate all possible combinations for first write, 
    // the following code ensures that attributes of this operation are valid
    void EnsureValid(MemOpProp& op)
    {
        unsigned typeSz = getBrigTypeNumBits(op.type);

        if (op.isPlainOp)
        {
            op.type    = (typeSz == 32)? BRIG_TYPE_S32 : BRIG_TYPE_S64;
            op.op      = BRIG_ATOMIC_ST;
            op.isNoRet = true;
        }
        else
        {
            if (!IsValidAtomicOp(op.op, op.isNoRet)) op.op   = BRIG_ATOMIC_ST;
            if (!IsValidAtomicType(op.op, op.type))  op.type = (typeSz == 32)? BRIG_TYPE_B32 : BRIG_TYPE_B64;
        }
    }

#else

    MModelTest(Grid                 geometry_,
               BrigAtomicOperation  hb_op,
               BrigAtomicOperation  sync_op,
               BrigSegment          hb_seg,
               BrigSegment          sync_seg,
               BrigMemoryOrder      hb_order,
               BrigMemoryOrder      sync_order,
               BrigMemoryScope      hb_scope,
               BrigMemoryScope      sync_scope,
               BrigType             hb_type,
               BrigType             sync_type,
               bool                 hb_plain
               )
    : AtomicTestHelper(KERNEL, geometry_),
        resArrayAddr(0),
        indexInResArray(0),
        resultFlag(0),
        loopIdx(0)
    {
        SetTestKind();

        mapFlat2Group = isBitType(sync_type);              // This is to minimize total number of tests
        bool sync_plain = false;                       // Second write must be an atomic to make synchromization possible
        bool hb_noret = true;
        bool sync_noret = false;

        for (unsigned id = MIN_ARRAY_ID; id <= MAX_ARRAY_ID; ++id)
        {
            indexInTestArray[id][WRITE_IDX] = 0;
            indexInTestArray[id][READ_IDX]  = 0;
            testArrayAddr[id] = 0;
        }

        // To avoid enumeration of all possible combinations of atomicNoRet for
        // both writes, deduce required value based on other attributes.
        // This is quite sufficient for test purposes.

        if (hb_plain)
        {
            hb_noret = true;
        }
        else
        {
            if (!IsValidAtomicOp(hb_op, hb_noret)) hb_noret = !hb_noret;
        }

        if (!IsValidAtomicOp(sync_op, sync_noret))
        {
            sync_noret = !sync_noret;
        }

        writeHbOp = MModelTestPropFactory::Get(0)->GetProp(this, 
                                                           hb_op, 
                                                           hb_seg, 
                                                           hb_order, 
                                                           hb_scope, 
                                                           hb_type, 
                                                           EQUIV, 
                                                           hb_noret,
                                                           hb_plain,
                                                           HB_ARRAY_ID);
        readHbOp = *writeHbOp;
        readHbOp.op      = BRIG_ATOMIC_LD;
        readHbOp.order   = BRIG_MEMORY_ORDER_RELAXED;
        readHbOp.isNoRet = false;

        writeSyncOp = MModelTestPropFactory::Get(1)->GetProp(this, 
                                                             sync_op, 
                                                             sync_seg, 
                                                             sync_order, 
                                                             sync_scope, 
                                                             sync_type, 
                                                             EQUIV, 
                                                             sync_noret,
                                                             sync_plain,
                                                             SYNC_ARRAY_ID);
        readSyncOp = *writeSyncOp;
        readSyncOp.op      = BRIG_ATOMIC_LD;
        readSyncOp.order   = BRIG_MEMORY_ORDER_SC_ACQUIRE;
        readSyncOp.isNoRet = false;
    }

#endif

    // ========================================================================
    // Test Name

    void Name(std::ostream& out) const 
    { 
        if (writeHbOp->isPlainOp) StName(out, writeHbOp); else AtomicName(out, writeHbOp);
        if (writeFence.IsRequired()) FenceName(out);
        out << "__";
        if (writeSyncOp->isPlainOp) StName(out, writeSyncOp); else AtomicName(out, writeSyncOp);
        out << "/" << geometry; 
    }

    void AtomicName(std::ostream& out, MemOpProp* p) const 
    { 
        assert(p);
        out << (p->isNoRet? "atomicnoret" : "atomic")
            << "_" << atomicOperation2str(p->op)
                   << SegName(p->seg)
            << "_" << memoryOrder2str(p->order)
            << "_" << memoryScope2str(p->scope)
            << "_" << type2str(p->type);
    }

    void StName(std::ostream& out, MemOpProp* p) const 
    { 
        assert(p);
        out << "st" << SegName(p->seg) << "_" << type2str(p->type);
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
            << "_" << memoryOrder2str(writeFence.order)
            << "_" << memoryScope2str(writeFence.scope);
    }

    // ========================================================================
    // Definition of test variables and arrays

    BrigType ResultType() const { return RES_TYPE; }
    
    Value ExpectedResult() const
    {
        return Value(MV_UINT32, U32(RES_VAL_PASSED));
    }

    void Init()
    {
        Test::Init();
    }

    void ModuleVariables()
    {
        Comment("Testing memory operations within " + TestName());

        DefineArray(writeHbOp);
        DefineArray(writeSyncOp);

        DefineWgCompletedArray();
    }

    // ========================================================================
    // Array properties

    MemOpProp* ArrayId2WriteOp(unsigned arrayId)
    {
        assert(MIN_ARRAY_ID <= arrayId && arrayId <= MAX_ARRAY_ID);
        switch(arrayId)
        {
        case HB_ARRAY_ID:   return writeHbOp;
        case SYNC_ARRAY_ID: return writeSyncOp;
        default:
            assert(false);
            return 0;
        }
    }

    string GetArrayName(MemOpProp* p)
    {
        assert(p);

        ostringstream s;

        switch (p->seg) 
        {
        case BRIG_SEGMENT_GLOBAL: s << "global"; break;
        case BRIG_SEGMENT_GROUP:  s << "group";  break;
        case BRIG_SEGMENT_FLAT:   s << "flat";   break;
        default: 
            assert(false);
            break;
        }

        s << "_array";

        switch(p->arrayId)
        {
        case HB_ARRAY_ID:     s << "_hb";     break;
        case SYNC_ARRAY_ID:   s << "_sync";   break;
        default: 
            assert(false);
            break;
        }

        return s.str();
    }

    void DefineArray(MModelTestProp* p)
    {
        assert(p);

        unsigned arrayId = p->arrayId;
        string arrayName = GetArrayName(p);

        testArray[arrayId] = be.EmitVariableDefinition(arrayName, ArraySegment(p), ArrayElemType(p), BRIG_ALIGNMENT_NONE, ArraySize(p));
        if (ArraySegment(p) != BRIG_SEGMENT_GROUP) testArray[arrayId].init() = ArrayInitializer(p);
    }

    Operand ArrayInitializer(MModelTestProp* p)
    {
        assert(p);
        assert(isIntType(p->type));

        ArbitraryData values;
        unsigned typeSize = getBrigTypeNumBytes(p->type);
        for (unsigned pos = 0; pos < geometry->GridSize(); ++pos)
        {
            uint64_t value = InitialValue(p, pos);
            values.write(&value, typeSize, pos * typeSize);
        }
        return be.Brigantine().createOperandConstantBytes(values.toSRef(), ArrayElemType(p), true);
    }

    BrigType ArrayElemType(MemOpProp* p)
    {
        assert(p);
        return isBitType(p->type)? (BrigType)getUnsignedType(getBrigTypeNumBits(p->type)) : p->type;
    }

    BrigSegment ArraySegment(MemOpProp* p) const
    {
        assert(p);
        if (p->seg == BRIG_SEGMENT_FLAT) return mapFlat2Group? BRIG_SEGMENT_GROUP : BRIG_SEGMENT_GLOBAL;
        return p->seg;
    }

    uint64_t ArraySize(MemOpProp* p) const 
    { 
        assert(p);
        return (ArraySegment(p) == BRIG_SEGMENT_GROUP)? (uint64_t)geometry->WorkgroupSize() : geometry->GridSize();
    }
    
    TypedReg Initializer(MModelTestProp* p)
    {
        assert(p);
        assert(isIntType(p->type));

        return p->InitialValue();
    }

    uint64_t InitialValue(MModelTestProp* p, unsigned pos)
    {
        assert(p);

        return p->InitialValue(pos);
    }

    TypedReg ExpectedValue(MModelTestProp* p, unsigned accessIdx)
    {
        assert(p);
        assert(accessIdx == WRITE_IDX || accessIdx == READ_IDX);

        return p->ExpectedValue(accessIdx);
    }

    // ========================================================================
    // Test properties

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
    // Encoding of atomic read and write operations

    ItemList AtomicOperands(MModelTestProp* p)
    {
        assert(p);

        TypedReg src0 = p->AtomicOperand();
        TypedReg src1 = p->AtomicOperand1();

        assert(src0);

        ItemList operands;

        if (!p->isNoRet)
        { 
            TypedReg atomicDst = be.AddTReg(getUnsignedType(getBrigTypeNumBits(p->type)));
            operands.push_back(atomicDst->Reg());
        }

        OperandAddress target = TargetAddr(LoadArrayAddr(p), ArrayIndex(p, WRITE_IDX), p->type);
        operands.push_back(target);

        if (src0) operands.push_back(src0->Reg());
        if (src1) operands.push_back(src1->Reg());

        return operands;
    }

    void AtomicSt(MModelTestProp* p)
    {
        assert(p);
        assert(!p->isPlainOp);

        ItemList operands = AtomicOperands(p);
        InstAtomic inst = Atomic(p->type, p->op, p->order, p->scope, p->seg, p->eqClass, !p->isNoRet);
        inst.operands() = operands;
    }

    TypedReg AtomicLd(MemOpProp* p)
    {
        assert(p);
        assert(!p->isPlainOp);

        ItemList operands;
        TypedReg atomicDst = be.AddTReg(getUnsignedType(getBrigTypeNumBits(p->type)));
        OperandAddress target = TargetAddr(LoadArrayAddr(p), ArrayIndex(p, READ_IDX), p->type);

        operands.push_back(atomicDst->Reg());
        operands.push_back(target);

        InstAtomic inst = Atomic(p->type, p->op, p->order, p->scope, p->seg, p->eqClass, !p->isNoRet);
        inst.operands() = operands;

        return atomicDst;
    }

    // ========================================================================
    // Encoding of plain read and write operations

    void PlainSt(MModelTestProp* p)
    {
        assert(p);
        assert(p->op == BRIG_ATOMIC_ST);
        assert(p->order == BRIG_MEMORY_ORDER_RELAXED);
        assert(p->isPlainOp);
        assert(p->isNoRet);

        TypedReg val = ExpectedValue(p, WRITE_IDX);
        OperandAddress target = TargetAddr(LoadArrayAddr(p), ArrayIndex(p, WRITE_IDX), p->type);
        St(p->type, p->seg, target, val);
    }

    TypedReg PlainLd(MemOpProp* p)
    {
        assert(p);
        assert(p->op == BRIG_ATOMIC_LD);
        assert(p->order == BRIG_MEMORY_ORDER_RELAXED);
        assert(p->isPlainOp);
        assert(!p->isNoRet);

        TypedReg dst = be.AddTReg(getUnsignedType(getBrigTypeNumBits(p->type)));
        OperandAddress target = TargetAddr(LoadArrayAddr(p), ArrayIndex(p, READ_IDX), p->type);
        Ld(p->type, p->seg, target, dst);

        return dst;
    }

    // ========================================================================
    // Kernel code

    void KernelCode()
    {
        assert(codeLocation == emitter::KERNEL);
        
        LoadArrayAddr(writeHbOp);
        LoadArrayAddr(writeSyncOp);
        LoadResAddr();
        LoadWgCompleteAddr();

        ArrayIndex(writeHbOp, WRITE_IDX);
        ArrayIndex(writeHbOp, READ_IDX);
        ArrayIndex(writeSyncOp, WRITE_IDX);
        ArrayIndex(writeSyncOp, READ_IDX);
        ResIndex();

        InitArray(writeHbOp);
        InitArray(writeSyncOp);
        InitResFlag();

        InitLoop();

        Comment("Clear 'testComplete' flag");
        TypedReg testComplete = be.AddTReg(BRIG_TYPE_B1);   // testComplete flag is used to record the result
        be.EmitMov(testComplete, (uint64_t)0);              // at first successful 'synchronized-with' attempt

        if (ArraySegment(writeHbOp) == BRIG_SEGMENT_GROUP ||
            ArraySegment(writeSyncOp) == BRIG_SEGMENT_GROUP) 
        {
            assert(testKind == TEST_KIND_WAVE || testKind == TEST_KIND_WGROUP);
            Comment("Make sure all workitems have completed initialization before starting test code", 
                    "This is important because otherwise some workitems may see uninitialized values");
            MemFence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_WORKGROUP);
            Barrier(testKind == TEST_KIND_WAVE);
            MemFence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_WORKGROUP);
        }

        Comment("This instruction is a part of 'happens-before' pair");
        if (writeHbOp->isPlainOp) PlainSt(writeHbOp); else AtomicSt(writeHbOp);         // HB-W

        Comment("This is the instruction another thread will 'synchronize-with'");
        if (writeFence.IsRequired()) Fence(writeFence);                                 // F-W
        if (writeSyncOp->isPlainOp) PlainSt(writeSyncOp); else AtomicSt(writeSyncOp);   // SYNC-W

        //NB: Execution model for workitems within a wave or workgroup is not defined.
        //    So when testing synchronization between workitems in a wave or workgroup
        //    the code below attempts to synchronize with SYNC-W in another workitem
        //    executing a limited number of iterations. If this attempt fails, the code
        //    waits at barrier and does one more synchrinization attempt which should
        //    succeed regardless of used execution model.
        //
        //NB: When testing synchronization between workgroups, the code attempts to 
        //    synchronize with the second write (instruction B) and also waits for 
        //    previous workgroups to complete. In this scenario the number of 
        //    iterations is not limited.

        StartLoop();

            TypedReg synchronizedWith = CheckResult(testComplete);  // SYNC-R, F-R, HB-R

            Comment("Update 'testComplete' flag");
            Or(testComplete, testComplete, synchronizedWith);

        EndLoop();

        if (testKind == TEST_KIND_WAVE || testKind == TEST_KIND_WGROUP)
        {
            Comment("This is the last attempt to synchronize with another workitem",
                    "Make sure all workitems within a workgroup have completed writing test data");
            
            // NB: fences are required to avoid reordering of test operations with barrier
            if (!writeSyncOp->IsRelease()) MemFence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_WORKGROUP);
            Barrier(testKind == TEST_KIND_WAVE);
            if (!readSyncOp.IsAcquire()) MemFence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_WORKGROUP);
        }
        else
        {
            Comment("This is the last attempt to synchronize with another workitem");
        }

        // Last attempt to synchronize (should always succeed)
        CheckResult(testComplete);

        // Save result flag in output array
        SaveResFlag();
    }

    TypedReg CheckResult(TypedReg testComplete)
    {
        Comment("Attempt to 'synchronize-with' another workitem");
        TypedReg sync = AtomicLd(&readSyncOp);                          // SYNC-R
        if (readFence.IsRequired()) Fence(readFence);                   // F-R

        Comment("Compare test value with expected value");
        TypedReg synchronizedWith = COND(sync, EQ, ExpectedValue(writeSyncOp, READ_IDX)->Reg());

        string lab = IfCond(synchronizedWith);                          // Skip HB code if synchronization failed

            Comment("This instruction is a part of 'happens-before' pair");
            TypedReg res  = writeHbOp->isPlainOp? PlainLd(&readHbOp) : AtomicLd(&readHbOp); // HB-R
            TypedReg isResSet = COND(res,  EQ, ExpectedValue(writeHbOp, READ_IDX)->Reg());

        EndIfCond(lab);

        Comment("Set test result");
        TypedReg ok = And(Not(testComplete), And(synchronizedWith, isResSet));

        Comment("Update result flag");
        CondAssign(resultFlag, RES_VAL_PASSED, resultFlag, ok);

        return synchronizedWith;
    }

    void Fence(FenceOpProp fence)
    {
        be.EmitMemfence(fence.order, fence.scope, fence.scope, BRIG_MEMORY_SCOPE_NONE);
    }

    // ========================================================================
    // Helper code for array access

    PointerReg LoadArrayAddr(MemOpProp* p)
    {
        assert(p);

        unsigned arrayId = p->arrayId;

        if (!testArrayAddr[arrayId])
        {
            Comment("Load array address");
            testArrayAddr[arrayId] = be.AddAReg(testArray[arrayId].segment());
            be.EmitLda(testArrayAddr[arrayId], testArray[arrayId]);
            if (p->seg == BRIG_SEGMENT_FLAT && ArraySegment(p) == BRIG_SEGMENT_GROUP) // NB: conversion is not required for global segment
            {
                PointerReg flat = be.AddAReg(BRIG_SEGMENT_FLAT);
                be.EmitStof(flat, testArrayAddr[arrayId]);
                testArrayAddr[arrayId] = flat;
            }
        }

        return testArrayAddr[arrayId];
    }

    void InitArray(MModelTestProp* p)
    {
        assert(p);

        if (ArraySegment(p) == BRIG_SEGMENT_GROUP)
        {
            Comment("Init array element");
        
            TypedReg val = Initializer(p);
            OperandAddress target = TargetAddr(LoadArrayAddr(p), ArrayIndex(p, WRITE_IDX), p->type);
            InstAtomic inst = Atomic(p->type, BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_SC_RELEASE, p->scope, p->seg, p->eqClass, false);
            inst.operands() = be.Operands(target, val->Reg());
        }
    }

    TypedReg ArrayIndex(MemOpProp* p, unsigned access)
    {
        assert(p);
        assert(access == WRITE_IDX || access == READ_IDX);

        unsigned arrayId = p->arrayId;

        if (!indexInTestArray[arrayId][access])
        {
            if (access == WRITE_IDX)
            {
                Comment("Init write array index for " + GetArrayName(p));

                indexInTestArray[arrayId][access] = TestIndex(p);
            }
            else // READ_IDX
            {
                Comment("Init read array index for " + GetArrayName(p));

                TypedReg id = TestIndex(p);

                if (testKind == TEST_KIND_AGENT)
                {
                    assert(ArraySegment(p) == BRIG_SEGMENT_GLOBAL);

                    // index == (id < wg.size)? id : id - wg.size;
                    TypedReg testId = Sub(id, Delta());
                    indexInTestArray[arrayId][access] = CondAssign(id, testId, COND(id, LT, Delta()));
                }
                else if (testKind == TEST_KIND_WGROUP || testKind == TEST_KIND_WAVE)
                {
                    // index == ((id % test.size) < delta)? id + test.size - delta : id - delta;
                    TypedReg localId = Rem(id, TestSize());
                    TypedReg testId1 = Add(id, TestSize() - Delta());
                    TypedReg testId2 = Sub(id, Delta());
                    indexInTestArray[arrayId][access] = CondAssign(testId1, testId2, COND(localId, LT, Delta()));
                }
                else
                {
                    assert(false);
                }
            }
        }
        return indexInTestArray[arrayId][access];
    }

    TypedReg TestIndex(MemOpProp* p)
    {
        assert(p);

        if (ArraySegment(p) == BRIG_SEGMENT_GLOBAL)
            return TestAbsId(LoadArrayAddr(p)->IsLarge());
        else
            return TestId(LoadArrayAddr(p)->IsLarge());
    }

    // ========================================================================
    // Helper code for working with result flag and output array

    void InitResFlag()
    {
        Comment("Init result flag");
        resultFlag = be.AddTReg(RES_TYPE);
        be.EmitMov(resultFlag, RES_VAL_FAILED);
    }

    void SaveResFlag()
    {
        Comment("Save result in output array");

        OperandAddress target = TargetAddr(LoadResAddr(), ResIndex(), ResultType());
        St(ResultType(), (BrigSegment)LoadResAddr()->Segment(), target, resultFlag);
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

    PointerReg LoadResAddr()
    {
        if (!resArrayAddr)
        {
            Comment("Load result address");
            resArrayAddr = output->Address();
        }
        return resArrayAddr;
    }

    // ========================================================================
    // Interface with MModelTestProp

    virtual TypedReg Index(unsigned arrayId, unsigned accessIdx)
    {
        assert(accessIdx == WRITE_IDX || accessIdx == READ_IDX);
        assert(MIN_ARRAY_ID <= arrayId && arrayId <= MAX_ARRAY_ID);

        MemOpProp* p = ArrayId2WriteOp(arrayId);
        TypedReg index = ArrayIndex(p, accessIdx);
        if (index->RegSizeBits() != getBrigTypeNumBits(p->type)) index = Cvt(index);
        return index;
    }

    // ========================================================================
    // Helper loop code

    void InitLoop()
    {
        if (testKind == TEST_KIND_WAVE || testKind == TEST_KIND_WGROUP)
        {
            Comment("Init loop index");

            loopIdx = be.AddTReg(LOOP_IDX_TYPE);
            be.EmitMov(loopIdx, MAX_LOOP);
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
            CheckPrevWg();
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
    // Validation of test attributes

    bool IsValid() const
    {
#ifdef TINY_TEST
        if (writeHbOp->op != BRIG_ATOMIC_ADD) return false;
        if (writeSyncOp->type != BRIG_TYPE_U32) return false;
#endif
        if (writeHbOp->isPlainOp)
        {
            if (!IsValidPlainStTest(writeHbOp)) return false;
        }
        else
        {
            if (!IsValidAtomicTest(writeHbOp)) return false;
        }

        if (writeSyncOp->isPlainOp)
        {
            if (!IsValidPlainStTest(writeSyncOp)) return false;
        }
        else
        {
            if (!IsValidAtomicTest(writeSyncOp)) return false;
        }

        if(!isValidTestSegment(writeHbOp)) return false;
        if(!isValidTestSegment(writeSyncOp)) return false;

        //NB: Any scope is valid for HB-W and HB-R
        //if(!isValidTestScope(writeHbOp)) return false;
        if(!isValidTestScope(writeSyncOp)) return false;

        return true;
    }

    bool isValidTestSegment(MemOpProp* p) const
    {
        assert(p);
        if (testKind == TEST_KIND_AGENT) return ArraySegment(p) != BRIG_SEGMENT_GROUP;
        return true;
    }

    bool isValidTestScope(MemOpProp* p)  const
    {
        assert(p);

        if (testKind == TEST_KIND_WGROUP) return p->scope != BRIG_MEMORY_SCOPE_WAVEFRONT;
        if (testKind == TEST_KIND_AGENT)  return p->scope != BRIG_MEMORY_SCOPE_WAVEFRONT &&
                                                 p->scope != BRIG_MEMORY_SCOPE_WORKGROUP;
        return true;
    }

    bool IsValidPlainStTest(MemOpProp* p) const
    {
        assert(p);
        BrigMemoryScope scope = (ArraySegment(p) == BRIG_SEGMENT_GROUP)? BRIG_MEMORY_SCOPE_WORKGROUP : BRIG_MEMORY_SCOPE_AGENT;
        return isValidStType(p->type) &&
               p->op    == BRIG_ATOMIC_ST &&
               p->order == BRIG_MEMORY_ORDER_RELAXED &&
               p->scope == scope &&
               p->isNoRet;
    }

    bool isValidStType(BrigType t) const
    {
        return isSignedType(t) || 
               isUnsignedType(t) ||
               isFloatType(t) ||
               t == BRIG_TYPE_B128;
    }

    bool IsValidAtomicTest(MemOpProp* p) const
    {
        assert(p);

        if (!IsValidAtomic(p->op, p->seg, p->order, p->scope, p->type, p->isNoRet)) return false;
        if (!IsValidGrid(p)) return false;

        if (p->op == BRIG_ATOMIC_LD) return false;
        //if (p->scope == BRIG_MEMORY_SCOPE_SYSTEM) return false; //F

        return true;
    }

    bool IsValidGrid(MemOpProp* p) const
    {
        assert(p);

        switch (p->op)
        {
        case BRIG_ATOMIC_AND:
        case BRIG_ATOMIC_OR:
        case BRIG_ATOMIC_XOR:
            return getBrigTypeNumBits(p->type) == TestSize();
            
        default:
            return true;
        }
    }

}; // class AtomicTest

//=====================================================================================

#ifdef QUICK_TEST

void MModelTests::Iterate(hexl::TestSpecIterator& it)
{
    MModelTestPropFactory first(0);
    MModelTestPropFactory second(1);

    CoreConfig* cc = CoreConfig::Get(context);
    MModelTest::wavesize = cc->Wavesize(); //F: how to get the value from inside of AtomicTest?
    Arena* ap = cc->Ap();

    TestForEach<MModelTest>(ap, it, "mmodel", cc->Grids().MModelSet(),
                                                                // "synchronized-with" properties:
                            cc->Segments().Atomic(),            //  - segment
                            cc->Memory().AllMemoryOrders(),     //  - order
                            cc->Memory().AllMemoryScopes(),     //  - scope
                            cc->Types().MemModel(),             //  - type
                                                                // "happens-before" properties:
                            cc->Memory().LimitedAtomics(),      //  - op
                            cc->Segments().Atomic(),            //  - segment
                            cc->Memory().AllMemoryOrders(),     //  - order
                            cc->Memory().AllMemoryScopes(),     //  - scope
                            Bools::All()                        //  - isPlain
                            );
}

#else 

void MModelTests::Iterate(hexl::TestSpecIterator& it)
{
    MModelTestPropFactory first(0);
    MModelTestPropFactory second(1);

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
                            Bools::All()                                                        // isPlain
                            );
}

#endif
//=====================================================================================

} // namespace hsail_conformance

// TODO
// - base initial values on id
