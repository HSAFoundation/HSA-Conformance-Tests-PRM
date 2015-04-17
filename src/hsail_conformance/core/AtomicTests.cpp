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

//=====================================================================================
//=====================================================================================
//=====================================================================================
// OVERVIEW
//
// This is a set of tests for atomic instructions.
//
// The purpose of this code is to test the result of parallel execution of the same 
// atomic write or read-write instruction by multple workitems in the grid.
// All of these atomic instructions access the same memory location at address M.
// Each workitem checks the following values:
//  - value in the destination register (except for atomicnoret instructions);
//  - final value in memory at address M.
//
//=====================================================================================
// GENERIC TEST STRUCTURE
//
// The following code shows generic structure of atomic tests:
//
//    <seg> <type> var = InitialValue();
//    Kernel(unsigned res[test.size])
//    {
//        res[wi.id] = TEST_FAILED;
//        Synchronize(1);
//        dst = Atomic(op, type, seg, var, Operand());
//        Synchronize(2);
//        ValidateDst(res, dst);
//        ValidateVar(res, var);
//    }
//
// Functions InitialValue(), and Operand() are different for each atomic operation;
// they are implemented so as to ensure different results for different workitems.
// Also note that ValidateDst() and ValidateVar() functions are not trivial.
// A straightforward approach would be to check dst and var values locally but these
// checks would have to be very conservative. Let us analyze how a test for ADD 
// operation could be implemented:
//
//    global unsigned var = 0;        // Initial value
//    Kernel(unsigned res[test.size]) // Expected (DST_PASSED | MEM_PASSED) for each wi
//    {
//        res[wi.id] = FAILED;
//        Synchronize(1);
//        dst = Atomic(ADD, UNSIGNED, GLOBAL, var, 1); // Each workitem adds 1
//        Synchronize(2);
//        if (dst     < test.size) res[wi.id] |= DST_PASSED;
//        if (var - 1 < test.size) res[wi.id] |= MEM_PASSED;
//    }
//
// However a simple analysis shows that each workitem has a unique dst value and
// that these values are in the [0, grid.size[ interval. This observation suggests
// a more intelligent test:
//
//    <seg> <type> var = 0;           // Initial value
//    Kernel(unsigned res[test.size]) // Expected (DST_PASSED | MEM_PASSED) for each wi
//    {
//        res[wi.id] = FAILED;
//        Synchronize(1);
//        dst = Atomic(ADD, UNSIGNED, GLOBAL, var, 1); // Each workitem adds 1
//        Synchronize(2);
//        if (dst     < test.size) res[dst]   |= DST_PASSED;
//        if (var - 1 < test.size) res[wi.id] |= MEM_PASSED;
//    }
//
// Test values being written to memory by initializer and atomic instruction depend on 
// atomic operation. Some operations such as OR and XOR utilize all bits of test memory.
// Other operations use only a few bits of test memory; these include additional encoding 
// of test values to fill in most bits of test memory. Tests for these operations 
// additionally decode dst amd memory values and validate that these values were decoded 
// successfully.
//
// Let us continue with our previous example. If our test includes, say 1024 workitems,
// test values will utilize only 10 bits out of 32 available bits. Additional encoding 
// and decoding functions may be implemented as follows:
//
// unsigned Encode(unsigned val) { return val * 0x10001; }
// unsigned Decode(unsigned val) { return val / 0x10001; }
//
// Using these functions, test code may be rewritten as shown below:
//
//    <seg> <type> var = Encode(0);           // Initial value
//    Kernel(unsigned res[test.size]) // Expected (DST_PASSED | MEM_PASSED) for each wi
//    {
//        res[wi.id] = FAILED;
//        Synchronize(1);
//        dst = Atomic(ADD, UNSIGNED, GLOBAL, var, Encode(1)); // Each workitem adds 1
//        Synchronize(2);
//        dst = Decode(dst);
//        mem = Decode(var);
//        if (dst     < test.size) res[dst]   |= DST_PASSED;
//        if (mem - 1 < test.size) res[wi.id] |= MEM_PASSED;
//    }
//
//=====================================================================================
// TEST KINDS
//
// There are three kinds of tests depending of test scope:
//  1. WAVE kind.
//     The grid may include many tests.
//     Each test consists of workitems within the same wave.
//     Each wave has a separate test memory location M.
//  2. WGROUP kind.
//     The grid may include many tests.
//     Each test consists of workitems within the same workgroup.
//     Each workgroup has a separate test memory location M.
//  3. AGENT kind.
//     The grid may include only one test.
//     The test consists of all workitems within the grid.
//     All workitems within grid access the same test memory location M.
//
// Note that first two kind of tests may ensure (pseudo) parallel execution
// using barriers:
//
//     void Synchronize(int i) { if (testKind == WAVE) wavebarrier(); else barrier(); }
//
// This will ensure that all workitems in the test execute atomic operation before
// reading value in memory. However this is not the case for AGENT kind tests.
// The order in which workitems in separate workgroups are executed is not defined;
// and there is no device like barrier to synchronize execution between workgroups.
// However a workitem in a workgroup may wait for completion of workitems in
// previous worgroups; this is an allowed behavior. 
//
// Consequently, tests for AGENT kind are more complicated. First, these test must 
// include synchronization so that workitems in the last workgroup see final value 
// in memory:
//
//     void Synchronize(int i) { barrier(); if (i == 1) WaitForPrevWgToComplete(); }
//
// Also note that other workgroups are not guaranteed to see final value in memory so
// checks for these workgroups have to be more conservative.
//
//=====================================================================================
// DETAILED DESCRIPTION OF TESTS
//
// Legend:
//
//  - wi.id:        workitemflatabsid
//  - wg.id:        workgroupid(0)
//  - wg.size:      workgroup size in X dimension 
//  - grid.size:    grid size in X dimension 
//  - test.size:    number of workitems which participate in the test:
//                      - wavesize for WAVE kind
//                      - wg.size for WGROUP kind
//                      - grid.size for AGENT kind
//
// Interface functions:
//
//  <type>   InitialValue()    initial value
//  <type>   Encode(val)       encode test value (compile-time)
//  TypedReg EncodeRt(val)     encode test value (run-time)
//  TypedReg VerifyRt(val)     verify encoding of test value (run-time)
//  TypedReg DecodeRt(val)     decode test value (run-time)
//  TypedReg AtomicOperand()   generate code for the first source operand of atomic instruction
//  TypedReg AtomicOperand1()  generate code for the second source operand of atomic instruction 
//  TypedReg MemIndex()        index in result array indicating where to set pass/fail flag for mem testing
//  TypedReg DstIndex(dst)     index in result array indicating where to set pass/fail flag for dst testing.  NB: dst is unsigned
//  TypedReg MemCond(mem)      condition indicating if mem result is valid (version for WAVE and WGROUP).     NB: mem is unsigned
//  TypedReg MemCondAgent(mem) condition indicating if mem result is valid (version for AGENT).               NB: mem is unsigned
//  TypedReg DstCond(dst, mem) condition indicating if dst result is valid.                                   NB: dst and mem are unsigned
//
//=====================================================================================
// TEST STRUCTURE FOR WAVE AND WGROUP TEST KINDS
//
//    // Define a test array. Each test must have a separate element in this array.
//    // NB: that array size depends on test kind and segment <seg>.
//    // NB: Initialization shown below is only possible for global array.
//    <seg> <type> var[TestArraySize()] = {Encode(InitialValue()), Encode(InitialValue()), ...; 
//
//    kernel(unsigned global res[grid.size]) // output array
//    {
//        private unsigned loc = MemLoc(); // Compute location of this test in test array
//
//        var[loc] = Encode(InitialValue());
//        res[wi.id] = 0;                            // clear result flag
//        
//        // Make sure all workitems have completed initialization
//
//        memfence_screl_wg;
//        (wave)barrier;
//        memfence_scacq_wg;
//
//        // This is the instruction being tested
//
//        private dst = AtomicOp(var[loc], EncodeRt(AtomicOperand()), EncodeRt(AtomicOperand1()));
//
//        // Make sure all workitems have completed atomic operation
//
//        memfence_screl_wg;
//        (wave)barrier;
//        memfence_scacq_wg;
//
//        // Validate that test values may be decoded and set result flags
//
//        if (VerifyRt(dst))      res[wi.id] |= FLAG_VLD_DST;
//        if (VerifyRt(var[loc])) res[wi.id] |= FLAG_VLD_MEM;
//
//        // Decode test values for subsequent checks 
//
//        private <type> d_dst = DecodeRt(dst);
//        private <type> d_mem = DecodeRt(var[loc]);
//        
//        // Validate decoded values
//
//        res[DstIndex(d_dst)] = DstCond(d_dst, d_mem)? FLAG_DST : 0;
//        res[MemIndex()]      = MemCond(d_mem)?        FLAG_MEM : 0;
//    }
//
//=====================================================================================
// TEST STRUCTURE FOR AGENT TEST KIND
//
//    // Define a test variable
//    <type> global var = Encode(InitialValue());
//
//    // Array used to check if all workitems in the previous workgroup have finished.
//    // When workitem i finishes, it increments the value at finished[i+1].
//    // First element of this array is initialized to ensure completion of first group
//    global unsigned finished[grid.size / wg.size + 1] = {wg.size, 0, 0, ...};
//
//    kernel(unsigned global res[grid.size]) // output array
//    {
//        res[wi.id] = 0;                            // clear result flag
//        
//        // This is the instruction being tested
//
//        private <type> dst = AtomicOp(var, EncodeRt(AtomicOperand()), EncodeRt(AtomicOperand1()));
//
//        // Make sure all workitems within workgroup have completed atomic operation
//
//        memfence_screl_wg;
//        barrier;
//        memfence_scacq_wg;
//
//        // Wait for previous workgroup to complete
//
//        do {} while (finished[wg.id] < wg.size);
//
//        finished[wg.id + 1]++; // Label this workitem as completed
//
//        // Validate that test values may be decoded and set result flags
//
//        if (VerifyRt(dst)) res[wi.id] |= FLAG_VLD_DST;
//        if (VerifyRt(var)) res[wi.id] |= FLAG_VLD_MEM;
//
//        // Decode test values for subsequent checks 
//
//        private <type> d_dst = DecodeRt(dst);
//        private <type> d_mem = DecodeRt(var);
//        
//        // Validate decoded values
//        // NB: only workitems in the last workgroup are guaranteed to see final value in memory
//
//        res[DstIndex(d_dst)] = DstCond(d_dst, d_mem)? FLAG_DST : 0;
//        res[MemIndex()]      = MemCond(d_mem)?        FLAG_MEM : 0;
//    }
//
//=====================================================================================

#include "AtomicTests.hpp"
#include "TestHelper.hpp"

namespace hsail_conformance {

//=====================================================================================
// Class AtomicTestProp declares an interface with classes that define properties of 
// atomic operations.

class AtomicTest;

class AtomicTestProp
{
protected:
    static const uint64_t ZERO = 0;

protected:
    BrigType type;
    uint64_t testSize;

private:
    AtomicTest* test;

public:
    virtual ~AtomicTestProp() {}

public:
    void setup(AtomicTest* test_, BrigType type_, uint64_t testSize_)
    {
        assert(test_);

        test = test_;
        type = type_;
        testSize = testSize_;
    }

public:
    virtual bool     Encryptable()                  const { return false; }
    virtual bool     CheckDst()                     const { return true;  }
    virtual bool     CheckMem()                     const { return true;  }
    virtual bool     CheckExch()                    const { return false; }

    virtual uint64_t InitialValue()                 const { assert(false); return 0; }
    virtual TypedReg AtomicOperand()                const { assert(false); return 0; }
    virtual TypedReg AtomicOperand1()               const {                return 0; }

    virtual TypedReg DstIndex(TypedReg dst)         const { assert(false); return 0; }
    virtual TypedReg DstCond(TypedReg dst)          const { assert(false); return 0; }
    virtual TypedReg DstCond(TypedReg dst, 
                             TypedReg mem)          const { return DstCond(dst); }     // By default, mem is not used

    virtual TypedReg MemIndex()                     const { assert(false); return 0; }
    virtual TypedReg MemCond(TypedReg mem)          const { assert(false); return 0; }
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return MemCond(mem);     }
    virtual TypedReg MemCond(TypedReg mem, 
                             bool isAgent)          const { if (isAgent) { // Note that only the last wg is guaranteed
                                                                           // to see final value in memory. Other wgs
                                                                           // must use more conservative condition
                                                                return Or(And(COND(WgId(), NE, MaxWgId()), MemCondAgent(mem)),
                                                                          And(COND(WgId(), EQ, MaxWgId()), MemCond(mem)));
                                                            } else {
                                                                return MemCond(mem);
                                                            }
                                                          }

    virtual TypedReg ExchIndex(TypedReg mem)        const { assert(false); return 0; }
    virtual TypedReg ExchCond(TypedReg dst)         const { assert(false); return 0; }
    virtual TypedReg ExchCondAgent(TypedReg dst)    const { assert(false); return 0; }
    virtual TypedReg ExchCond(TypedReg dst, 
                              bool isAgent)         const { return isAgent? ExchCondAgent(dst) : ExchCond(dst); }

protected:
    TypedReg Mov(uint64_t val) const;
    TypedReg Min(TypedReg val, uint64_t max) const;
    TypedReg Cond(unsigned cond, TypedReg val1, uint64_t val2) const;
    TypedReg Cond(unsigned cond, TypedReg val1, TypedReg val2) const;
    TypedReg And(TypedReg x, TypedReg y) const;
    TypedReg Or(TypedReg x, TypedReg y) const;
    TypedReg Sub(TypedReg x, uint64_t y) const;
    TypedReg Shl(uint64_t x, TypedReg y) const;
    TypedReg Not(TypedReg x) const;
    TypedReg PopCount(TypedReg x) const;
    TypedReg WgId() const;                          // workgroup id (32 bit)
    uint64_t MaxWgId() const;                       // max workgroup id
    TypedReg Id() const;                            // local test id (32/64 bit depending on type)
    TypedReg Id32() const;                          // local test id (32 bit)
    TypedReg Idx() const;                           // global test id (32/64 bit depending address size)
};

//=====================================================================================

class AtomicTestPropAdd : public AtomicTestProp // ******* BRIG_ATOMIC_ADD *******
{
public:
    virtual bool     Encryptable()                  const { return true; }

    virtual uint64_t InitialValue()                 const { return 0; }
    virtual TypedReg AtomicOperand()                const { return Mov(1); }

    virtual TypedReg DstIndex(TypedReg dst)         const { return Min(dst, testSize - 1); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(dst, LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, testSize); }
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return And(COND(mem, GT, ZERO), COND(mem, LE, testSize)); } // mem is unsigned
};

class AtomicTestPropSub : public AtomicTestProp // ******* BRIG_ATOMIC_SUB *******
{
public:
    virtual bool     Encryptable()                  const { return true; }

    virtual uint64_t InitialValue()                 const { return testSize; }
    virtual TypedReg AtomicOperand()                const { return Mov(1); }

    virtual TypedReg DstIndex(TypedReg dst)         const { return Min(Sub(dst, 1), testSize - 1); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(Sub(dst, 1), LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, ZERO); }
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return COND(mem, LT, testSize); } // mem is unsigned
};

class AtomicTestPropOr : public AtomicTestProp // ******* BRIG_ATOMIC_OR *******
{
public:
    virtual bool     Encryptable()                  const { return false; }

    virtual uint64_t InitialValue()                 const { return 0; }
    virtual TypedReg AtomicOperand()                const { return Shl(1, Id32()); }

    virtual TypedReg DstIndex(TypedReg dst)         const { return Min(PopCount(dst), testSize - 1); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(PopCount(dst), LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, -1); }
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return COND(mem, NE, ZERO); }
};

class AtomicTestPropXor : public AtomicTestProp // ******* BRIG_ATOMIC_XOR *******
{
public:
    virtual bool     Encryptable()                  const { return false; }

    virtual uint64_t InitialValue()                 const { return 0; }
    virtual TypedReg AtomicOperand()                const { return Shl(1, Id32()); }

    virtual TypedReg DstIndex(TypedReg dst)         const { return Min(PopCount(dst), testSize - 1); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(PopCount(dst), LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, -1); }
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return COND(mem, NE, ZERO); }
};

class AtomicTestPropAnd : public AtomicTestProp // ******* BRIG_ATOMIC_AND *******
{
public:
    virtual bool     Encryptable()                  const { return false; }

    virtual uint64_t InitialValue()                 const { return -1; } //F
    virtual TypedReg AtomicOperand()                const { return Not(Shl(1, Id32())); }

    virtual TypedReg DstIndex(TypedReg dst)         const { return Min(Sub(PopCount(dst), 1), testSize - 1); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(Sub(PopCount(dst), 1), LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, ZERO); }
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return COND(mem, NE, -1); }
};

class AtomicTestPropWrapinc : public AtomicTestProp // ******* BRIG_ATOMIC_WRAPINC *******
{
public:
    virtual bool     Encryptable()                  const { return false; }

    virtual uint64_t InitialValue()                 const { return 0; }
    virtual TypedReg AtomicOperand()                const { return Mov(-1); }     // max value

    virtual TypedReg DstIndex(TypedReg dst)         const { return Min(dst, testSize - 1); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(dst, LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, testSize); } // mem is unsigned
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return And(COND(mem, GT, ZERO), COND(mem, LE, testSize)); } // mem is unsigned
};

class AtomicTestPropWrapdec : public AtomicTestProp // ******* BRIG_ATOMIC_WRAPDEC *******
{
public:
    virtual bool     Encryptable()                  const { return false; }

    virtual uint64_t InitialValue()                 const { return testSize - 1; }
    virtual TypedReg AtomicOperand()                const { return Mov(-1); }     // max value

    virtual TypedReg DstIndex(TypedReg dst)         const { return Min(dst, testSize - 1); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(dst, LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, -1); } // mem is unsigned
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return Or(COND(mem, EQ, -1), COND(mem, LT, testSize - 1)); } // mem is unsigned
};

class AtomicTestPropMax : public AtomicTestProp // ******* BRIG_ATOMIC_MAX *******
{
public:
    virtual bool     Encryptable()                  const { return true; }

    virtual uint64_t InitialValue()                 const { return 0; }
    virtual TypedReg AtomicOperand()                const { return Id(); }

    virtual TypedReg DstIndex(TypedReg dst)         const { return Idx(); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(dst, LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, testSize - 1); } // mem is unsigned
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return COND(mem, LE, testSize - 1); } // mem is unsigned
};

class AtomicTestPropMin : public AtomicTestProp // ******* BRIG_ATOMIC_MIN *******
{
public:
    virtual bool     Encryptable()                  const { return true; }

    virtual uint64_t InitialValue()                 const { return testSize - 1; }
    virtual TypedReg AtomicOperand()                const { return Id(); }

    virtual TypedReg DstIndex(TypedReg dst)         const { return Idx(); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(dst, LT, testSize); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, EQ, ZERO); }     // mem is unsigned
    virtual TypedReg MemCondAgent(TypedReg mem)     const { return COND(mem, LT, testSize); } // mem is unsigned
};

class AtomicTestPropExch : public AtomicTestProp // ******* BRIG_ATOMIC_EXCH *******
{
public:
    virtual bool     Encryptable()                  const { return true; }

    virtual uint64_t InitialValue()                 const { return testSize; }
    virtual TypedReg AtomicOperand()                const { return Id(); }

    virtual TypedReg DstIndex(TypedReg dst)         const { return Min(dst, testSize - 1); }
    virtual TypedReg DstCond(TypedReg dst)          const { return COND(dst, LT, testSize); }

    virtual bool     CheckExch()                    const { return true; }
    virtual TypedReg ExchIndex(TypedReg mem)        const { return Min(mem, testSize - 1); }  // mem is unsigned
    virtual TypedReg ExchCond(TypedReg dst)         const { return COND(dst, EQ, testSize); }
    virtual TypedReg ExchCondAgent(TypedReg dst)    const { return COND(Id32(), EQ, testSize - 1); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, LT, testSize); } // mem is unsigned
};

class AtomicTestPropCas : public AtomicTestProp // ******* BRIG_ATOMIC_CAS *******
{
public:
    virtual bool     Encryptable()                  const { return true; }

    virtual uint64_t InitialValue()                 const { return testSize; }
    virtual TypedReg AtomicOperand()                const { return Mov(InitialValue()); } // value which is being compared
    virtual TypedReg AtomicOperand1()               const { return Id(); }                // value to swap

    virtual TypedReg DstIndex(TypedReg dst)         const { return Idx(); }
    virtual TypedReg DstCond(TypedReg dst,          // NB: this is a valid code even for AGENT test kind because mem is assigned only once.
                             TypedReg mem)          const { return Or(And(COND(dst, EQ, InitialValue()), COND(mem, EQ, Id())),
                                                                      And(COND(dst, EQ, mem),            COND(mem, NE, Id()))); } // mem is unsigned

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, LT, testSize); } // mem is unsigned
};

class AtomicTestPropSt : public AtomicTestProp // ******* BRIG_ATOMIC_ST *******
{
public:
    virtual bool     Encryptable()                  const { return true; }
    virtual bool     CheckDst()                     const { return false; } // no dst: only atomicnoret for ST

    virtual uint64_t InitialValue()                 const { return testSize; }
    virtual TypedReg AtomicOperand()                const { return Id(); }

    virtual TypedReg MemIndex()                     const { return Idx(); }
    virtual TypedReg MemCond(TypedReg mem)          const { return COND(mem, LT, testSize); } // mem is unsigned
};

class AtomicTestPropLd : public AtomicTestProp // ******* BRIG_ATOMIC_LD *******
{
};

//=====================================================================================

class AtomicTestPropFactory
{
private:
    static const unsigned ATOMIC_OPS = BRIG_ATOMIC_XOR + 1; //F
    static AtomicTestProp* prop[ATOMIC_OPS];

public:
    static void Create();
    static void Destroy();
    static AtomicTestProp* Get(AtomicTest* test, BrigAtomicOperation op, BrigType type, uint64_t testSize);
};

//=====================================================================================

class AtomicTest : public TestHelper
{
protected: // Flags indicating passed/failed conditions
    static const unsigned FLAG_NONE    = 0;     // check failed
    static const unsigned FLAG_MEM     = 1;     // passed check of memory value
    static const unsigned FLAG_DST     = 2;     // passed check of destination value
    static const unsigned FLAG_VLD_MEM = 4;     // passed decryption of memory value
    static const unsigned FLAG_VLD_DST = 8;     // passed decryption of destination value

protected:
    BrigAtomicOperation atomicOp;
    BrigSegment         segment;
    BrigMemoryOrder     memoryOrder;
    BrigMemoryScope     memoryScope;
    BrigType            type;
    bool                atomicNoRet;
    uint8_t             equivClass;
    DirectiveVariable   testVar;                // memory which is being accessed by atomic ops
    bool                mapFlat2Group;          // if true, map flat to group, if false, map flat to global

private:
    AtomicTestProp*     prop;                   // properties of the current atomic operation
    PointerReg          atomicVarAddr;          // address of variable which is modified by atomic ops
    PointerReg          resArrayAddr;           // output array of test flags (passed/failed)
    TypedReg            indexInResArray;        // index of current workitem in result array
    TypedReg            atomicDst;              //NB: always unsigned type
    TypedReg            atomicMem;              //NB: always unsigned type


    // ========================================================================

public:
    AtomicTest(Grid geometry_,
                BrigAtomicOperation atomicOp_,
                BrigSegment segment_,
                BrigMemoryOrder memoryOrder_,
                BrigMemoryScope memoryScope_,
                BrigType type_,
                bool mapFlat2Group_,
                bool noret)
    : TestHelper(KERNEL, geometry_),
        atomicOp(atomicOp_),
        segment(segment_),
        memoryOrder(memoryOrder_),
        memoryScope(memoryScope_),
        type(type_),
        atomicNoRet(noret),
        equivClass(0),
        mapFlat2Group(mapFlat2Group_),
        atomicVarAddr(0),
        resArrayAddr(0),
        indexInResArray(0),
        atomicDst(0),
        atomicMem(0)
    {
        SetTestKind();
        prop = AtomicTestPropFactory::Get(this, atomicOp_, type_, geometry->GridSize());
    }

    // ========================================================================

    void SetTestKind()
    {
        assert(geometry->GridSize() % geometry->WorkgroupSize() == 0);
        assert(geometry->GridSize() >= geometry->WorkgroupSize());

        if      (Groups() > 1) testKind = TEST_KIND_AGENT;
        else if (Waves()  > 1) testKind = TEST_KIND_WGROUP;
        else                   testKind = TEST_KIND_WAVE;
    }

    void Name(std::ostream& out) const 
    { 
        out << (atomicNoRet? "atomicnoret" : "atomic")
            << "_" << atomicOperation2str(atomicOp)
                   << SegName()
            << "_" << memoryOrder2str(memoryOrder)
            << "_" << memoryScope2str(memoryScope)
            << "_" << type2str(type)
            << "/" << geometry; 
    }

    std::string SegName() const
    {
        string pref = (segment == BRIG_SEGMENT_FLAT)? "_f" : "_";
        return pref + segment2str(VarSegment());
    }

    BrigType ResultType() const { return BRIG_TYPE_U32; }
    
    Value ExpectedResult() const
    {
        unsigned expected = FLAG_MEM | (Encryptable()? FLAG_VLD_MEM : FLAG_NONE);
        if (!atomicNoRet) expected |= FLAG_DST | (Encryptable()? FLAG_VLD_DST : FLAG_NONE);
        return Value(MV_UINT32, U32(expected));
    }

    void Init()
    {
        Test::Init();
    }

    void ModuleVariables()
    {
        Comment("Testing atomic operations within " + TestName());

        std::string varName;

        switch (segment) 
        {
        case BRIG_SEGMENT_GLOBAL: varName = "global_var"; break;
        case BRIG_SEGMENT_GROUP:  varName = "group_var";  break;
        case BRIG_SEGMENT_FLAT:   varName = "flat_var";   break;
        default: 
            assert(false);
            break;
        }

        testVar = be.EmitVariableDefinition(varName, VarSegment(), type);

        if (VarSegment() != BRIG_SEGMENT_GROUP) testVar.init() = Initializer(type);

        DefineWgCompletedArray();
    }

    BrigSegment VarSegment() const
    {
        if (segment == BRIG_SEGMENT_FLAT) return mapFlat2Group? BRIG_SEGMENT_GROUP : BRIG_SEGMENT_GLOBAL;
        return segment;
    }

    // ========================================================================

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

    TypedReg EncodeRt(TypedReg val)
    {
        return (Key() == 1)? val : Mul(val, Key());
    }

    TypedReg DecodeRt(TypedReg val)
    {
        assert(val);
        assert(isUnsignedType(val->Type()));

        return (Key() == 1)? val : Div(val, Key());
    }

    TypedReg VerifyRt(TypedReg val)
    {
        assert(val);
        assert(isUnsignedType(val->Type()));

        return (Key() == 1)? Mov(type, 0) : Rem(val, Key());
    }

    Operand Initializer(BrigType t)
    {
        uint64_t init = InitialValue();
        if (Encryptable()) init = Encode(init);
        return be.Immed(t, init);
    }

    BrigType UnsignedType() { return (BrigType)getUnsignedType(getBrigTypeNumBits(type)); }

    // ========================================================================

    bool Encryptable() const
    {
        assert(prop);
        return prop->Encryptable();
    }

    uint64_t InitialValue()
    {
        assert(prop);
        return prop->InitialValue();
    }

    // ========================================================================

    void KernelCode()
    {
        assert(codeLocation == emitter::KERNEL);

        LoadVarAddr();
        LoadResAddr();
        LoadWgCompleteAddr();

        InitVar();
        InitRes();

        Synchronize();

        AtomicInst(AtomicOperands());

        Synchronize();

        WaitForPrevWg();

        DecodeDst();
        DecodeMem();

        CheckMem();
        CheckDst();
        CheckExch();
    }

    // ========================================================================

    void Synchronize()
    {
        Comment("Synchronize");

        MemFence(BRIG_MEMORY_ORDER_SC_RELEASE, BRIG_MEMORY_SCOPE_WORKGROUP);
        Barrier(testKind == TEST_KIND_WAVE);
        MemFence(BRIG_MEMORY_ORDER_SC_ACQUIRE, BRIG_MEMORY_SCOPE_WORKGROUP);
    }

    void DecodeDst()
    {
        if (!atomicNoRet && Encryptable())
        {
            assert(atomicDst);
            assert(isUnsignedType(atomicDst->Type()));

            TypedReg idx = Index();
            Comment("Validate atomic dst");
            SetFlag(idx, COND(VerifyRt(atomicDst), EQ, 0), FLAG_VLD_DST);

            Comment("Decode dst value");
            atomicDst = DecodeRt(atomicDst);
        }
    }

    void DecodeMem()
    {
        LdVar();

        if (Encryptable())
        {
            TypedReg idx = Index();
            Comment("Validate final value in memory");
            SetFlag(idx, COND(VerifyRt(LdVar()), EQ, 0), FLAG_VLD_MEM);

            assert(atomicMem);
            assert(isUnsignedType(atomicMem->Type()));

            Comment("Decode memory value");
            atomicMem = DecodeRt(atomicMem);
        }
    }

    void CheckDst()
    {
        assert(prop);
        
        if (!atomicNoRet && prop->CheckDst())
        {
            assert(atomicDst);
            assert(atomicMem);
            assert(isUnsignedType(atomicDst->Type()));
            assert(isUnsignedType(atomicMem->Type()));

            Comment("Compute and normalize dst index (if necessary)");
            TypedReg idx = prop->DstIndex(atomicDst);
        
            assert(idx);

            Comment("Check atomic dst");
            TypedReg cond = prop->DstCond(atomicDst, atomicMem);

            assert(cond);

            SetFlag(idx, cond, FLAG_DST);
        }
    }

    void CheckExch()
    {
        assert(prop);
        
        if (!atomicNoRet && prop->CheckExch())
        {
            assert(atomicDst);
            assert(isUnsignedType(atomicDst->Type()));

            Comment("Compute and normalize special dst index");
            TypedReg idx = prop->ExchIndex(LdVar());
        
            assert(idx);

            Comment("Check atomic dst (special)");
            TypedReg cond = prop->ExchCond(atomicDst, testKind == TEST_KIND_AGENT);

            assert(cond);

            SetFlag(idx, cond, FLAG_DST);
        }
    }

    void CheckMem()
    {
        assert(prop);
        
        if (prop->CheckMem())
        {
            TypedReg idx = prop->MemIndex();
        
            assert(idx);

            Comment("Check final value in memory");
            TypedReg cond = prop->MemCond(LdVar(), testKind == TEST_KIND_AGENT);

            assert(cond);

            SetFlag(idx, cond, FLAG_MEM);
        }
    }

    // ========================================================================

    void AtomicInst(ItemList operands)
    {
        Comment("This is the instruction being tested:");

        InstAtomic inst = Atomic(type, atomicOp, memoryOrder, memoryScope, segment, equivClass, !atomicNoRet);
        inst.operands() = operands;
    }

    ItemList AtomicOperands()
    {
        assert(prop);

        Comment("Load atomic operands");

        TypedReg src0 = prop->AtomicOperand();
        TypedReg src1 = prop->AtomicOperand1();

        assert(src0);

        if (Encryptable())
        {
            if (src0) src0 = EncodeRt(src0);
            if (src1) src1 = EncodeRt(src1);
        }

        ItemList operands;

        if (!atomicNoRet)
        { 
            assert(!atomicDst);
            atomicDst = be.AddTReg(UnsignedType()); // NB: atomicDst is interpreted as unsigned to simplify checks
            operands.push_back(atomicDst->Reg());
        }

        operands.push_back(be.Address(LoadVarAddr()));
        if (src0) operands.push_back(src0->Reg());
        if (src1) operands.push_back(src1->Reg());

        return operands;
    }

    // ========================================================================

    PointerReg LoadVarAddr()
    {
        if (!atomicVarAddr)
        {
            Comment("Load variable address");
            atomicVarAddr = be.AddAReg(VarSegment());
            be.EmitLda(atomicVarAddr, testVar);
            if (segment == BRIG_SEGMENT_FLAT && VarSegment() == BRIG_SEGMENT_GROUP) // NB: conversion is not required for global segment
            {
                PointerReg flat = be.AddAReg(BRIG_SEGMENT_FLAT);
                be.EmitStof(flat, atomicVarAddr);
                atomicVarAddr = flat;
            }
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
        if (VarSegment() == BRIG_SEGMENT_GROUP)
        {
            Comment("Init variable");

            TypedReg id = be.EmitWorkitemFlatAbsId(false);
            STARTIF(id, EQ, 0)

            InstAtomic inst = Atomic(type, BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_SC_RELEASE, memoryScope, segment, equivClass, false);
            inst.operands() = be.Operands(be.Address(LoadVarAddr()), Initializer((BrigType)type2bitType(type)));

            ENDIF
        }
    }

    void InitRes()
    {
        Comment("Clear result array");

        OperandAddress target = TargetAddr(LoadResAddr(), Index(), ResultType());
        InstAtomic inst = Atomic(ResultType(), BRIG_ATOMIC_ST, BRIG_MEMORY_ORDER_SC_RELEASE, memoryScope, (BrigSegment)LoadResAddr()->Segment(), 0, false);
        inst.operands() = be.Operands(target, be.Immed(ResultType(), FLAG_NONE));
    }

    TypedReg LdVar()
    {
        if (!atomicMem)
        {
            Comment("Load final value from memory");

            atomicMem = be.AddTReg(UnsignedType()); // NB: atomicMem is interpreted as unsigned to simplify checks
            InstAtomic inst = Atomic(type, BRIG_ATOMIC_LD, BRIG_MEMORY_ORDER_SC_ACQUIRE, memoryScope, segment, equivClass);
            inst.operands() = be.Operands(atomicMem->Reg(), be.Address(LoadVarAddr()));
        }

        assert(atomicMem);
        assert(isUnsignedType(atomicMem->Type()));

        return atomicMem;
    }

    void SetFlag(TypedReg index, TypedReg cond, unsigned flagVal)
    {
        TypedReg flagValue = CondAssign(ResultType(), flagVal, FLAG_NONE, cond);
        OperandAddress target = TargetAddr(LoadResAddr(), index, ResultType());
        InstAtomic inst = Atomic(ResultType(), BRIG_ATOMIC_ADD, BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE, memoryScope, (BrigSegment)LoadResAddr()->Segment(), 0, false);
        inst.operands() = be.Operands(target, flagValue->Reg());
    }
    
    // ========================================================================
    // Helper loop code

    void WaitForPrevWg()
    {
        if (testKind == TEST_KIND_AGENT)
        {
            be.EmitLabel(LAB_NAME);
            CheckPrevWg();
        }
    }

    // ========================================================================

    bool IsValid() const
    {
        if (!IsValidAtomic(atomicOp, segment, memoryOrder, memoryScope, type, atomicNoRet)) return false;
        if (!isValidTestSegment()) return false;
        if (!isValidTestScope()) return false;
        if (!IsValidGrid()) return false;

        // List of current limitations (features that require special testing setup)
        // Tests for the following features should be implemented separately
        if (atomicOp == BRIG_ATOMIC_LD) return false;

        //if (memoryScope == BRIG_MEMORY_SCOPE_SYSTEM) return false;
        //if (atomicOp == BRIG_ATOMIC_ST && memoryOrder != BRIG_MEMORY_ORDER_SC_RELEASE) return false;
        //if (atomicOp != BRIG_ATOMIC_ST && memoryOrder != BRIG_MEMORY_ORDER_SC_ACQUIRE_RELEASE) return false;

        return true;
    }

    bool isValidTestSegment() const
    {
        if (testKind == TEST_KIND_AGENT) return VarSegment() != BRIG_SEGMENT_GROUP;
        return true;
    }

    bool isValidTestScope()  const
    {
        if (testKind == TEST_KIND_WGROUP) return memoryScope != BRIG_MEMORY_SCOPE_WAVEFRONT;
        if (testKind == TEST_KIND_AGENT)  return memoryScope != BRIG_MEMORY_SCOPE_WAVEFRONT &&
                                                 memoryScope != BRIG_MEMORY_SCOPE_WORKGROUP;
        return true;
    }

    bool IsValidGrid() const
    {
        if (memoryScope == BRIG_MEMORY_SCOPE_WAVEFRONT && Waves()  != 1) return false;
        if (memoryScope == BRIG_MEMORY_SCOPE_WORKGROUP && Groups() != 1) return false;

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

unsigned TestHelper::wavesize;

//=====================================================================================

TypedReg AtomicTestProp::Mov(uint64_t val)                                 const { return test->Mov(type, val); }
TypedReg AtomicTestProp::Min(TypedReg val, uint64_t max)                   const { return test->Min(val, max); }
TypedReg AtomicTestProp::Cond(unsigned cond, TypedReg val1, uint64_t val2) const { return test->Cond(cond, val1, val2); }
TypedReg AtomicTestProp::Cond(unsigned cond, TypedReg val1, TypedReg val2) const { return test->Cond(cond, val1, val2->Reg()); }
TypedReg AtomicTestProp::And(TypedReg x, TypedReg y)                       const { return test->And(x, y); }
TypedReg AtomicTestProp::Or(TypedReg x, TypedReg y)                        const { return test->Or(x, y); }
TypedReg AtomicTestProp::Sub(TypedReg x, uint64_t y)                       const { return test->Sub(x, y); }
TypedReg AtomicTestProp::Shl(uint64_t x, TypedReg y)                       const { return test->Shl(type, x, y); }
TypedReg AtomicTestProp::Not(TypedReg x)                                   const { return test->Not(x); }
TypedReg AtomicTestProp::PopCount(TypedReg x)                              const { return test->Popcount(x); }
TypedReg AtomicTestProp::WgId()                                            const { return test->TestWgId(false); }
uint64_t AtomicTestProp::MaxWgId()                                         const { return test->Groups() - 1; }
TypedReg AtomicTestProp::Id()                                              const { return test->TestAbsId(getBrigTypeNumBits(type) == 64); }
TypedReg AtomicTestProp::Id32()                                            const { return test->TestAbsId(false); }
TypedReg AtomicTestProp::Idx()                                             const { return test->Index(); }

//=====================================================================================

AtomicTestProp* AtomicTestPropFactory::prop[ATOMIC_OPS];

void AtomicTestPropFactory::Create()  { for (unsigned i = 0; i < ATOMIC_OPS; ++i) prop[i] = 0; }
void AtomicTestPropFactory::Destroy() { for (unsigned i = 0; i < ATOMIC_OPS; ++i) delete prop[i]; }

AtomicTestProp* AtomicTestPropFactory::Get(AtomicTest* test, BrigAtomicOperation op, BrigType type, uint64_t testSize)
{
    assert(0 <= op && op < ATOMIC_OPS);

    if (prop[op] == 0)
    {
       switch (op)
       {
       case BRIG_ATOMIC_ADD:      prop[op] = new AtomicTestPropAdd();     break;
       case BRIG_ATOMIC_AND:      prop[op] = new AtomicTestPropAnd();     break;
       case BRIG_ATOMIC_CAS:      prop[op] = new AtomicTestPropCas();     break;
       case BRIG_ATOMIC_EXCH:     prop[op] = new AtomicTestPropExch();    break;
       case BRIG_ATOMIC_MAX:      prop[op] = new AtomicTestPropMax();     break;
       case BRIG_ATOMIC_MIN:      prop[op] = new AtomicTestPropMin();     break;
       case BRIG_ATOMIC_OR:       prop[op] = new AtomicTestPropOr();      break;
       case BRIG_ATOMIC_ST:       prop[op] = new AtomicTestPropSt();      break;
       case BRIG_ATOMIC_SUB:      prop[op] = new AtomicTestPropSub();     break;
       case BRIG_ATOMIC_WRAPDEC:  prop[op] = new AtomicTestPropWrapdec(); break;
       case BRIG_ATOMIC_WRAPINC:  prop[op] = new AtomicTestPropWrapinc(); break;
       case BRIG_ATOMIC_XOR:      prop[op] = new AtomicTestPropXor();     break;
       case BRIG_ATOMIC_LD:       prop[op] = new AtomicTestPropLd();      break;

       default:
           assert(false);
           break;
       }
    }

    prop[op]->setup(test, type, testSize);
    return prop[op];
}

//=====================================================================================

void AtomicTests::Iterate(hexl::TestSpecIterator& it)
{
    AtomicTestPropFactory::Create();
    CoreConfig* cc = CoreConfig::Get(context);
    AtomicTest::wavesize = cc->Wavesize(); //F: how to get the value from inside of AtomicTest?
    Arena* ap = cc->Ap();
    TestForEach<AtomicTest>(ap, it, "atomicity", 
                            cc->Grids().AtomicSet(),          // grid
                            cc->Memory().AllAtomics(),        // atomic op
                            cc->Segments().Atomic(),          // segment
                            cc->Memory().AllMemoryOrders(),   // order
                            cc->Memory().AllMemoryScopes(),   // scope
                            cc->Types().Atomic(),             // type
                            Bools::All(),                     // mapFlat2Group
                            Bools::All());                    // isNoRet
    AtomicTestPropFactory::Destroy();
}

//=====================================================================================

} // namespace hsail_conformance

// TODO
// - generalize WAVE and WGROUP tests for any grid
