//===-- HSAILTestGenSample.h - HSAIL Test Generator Backend ---------------===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef INCLUDED_HSAIL_TESTGEN_BACKEND_H
#define INCLUDED_HSAIL_TESTGEN_BACKEND_H

#include "HSAILTestGenBrigContext.h"
#include "HSAILTestGenTestDesc.h"
#include "HSAILItems.h"
#include "Brig.h"

#include <string>

namespace TESTGEN {

// ============================================================================
// TestGen Backend
//
// TestGen Backend is a component which extends TestGen functionality.
//
// The primary purpose of TestGen is to generate valid (or invalid) HSAIL
// instructions  with all possible combinations of modifiers and argument types.
// Each generated instruction may be regarded as a "template" for
// further customization.
//
// TestGen Backend may generate any number of tests for each instruction
// "template" provided by TestGen, for example:
//   - Disable tests generation for some opcodes or for instructions with
//     specific operands;
//   - generate several tests for each template replacing template arguments
//     with specific operands.
//
// All generated tests may be divided into sets, groups and individual tests.
//
// A test set is a bundle of tests generated for one instruction "template".
// One test set may include one or more test groups.
//
// A test group is a 2-dimensional bundle of tests. One dimension
// specifies individual tests executed sequentally by each workitem; 
// the second dimension enumerates groups of tests executed by workitems:
//
// workitem 0: test 0.0; test 0.1; ... test 0.N;
// workitem 1: test 1.0; test 1.1; ... test 1.N;
// ...
// workitem K: test K.0; test K.1; ... test K.N;
//
// Each test group has its own BRIG and test data. 
// 
// ============================================================================
//
class TestGenBackend
{
private:
    static TestGenBackend *backend;

private:
    // Index of current test within current test set
    // Used only by this (default) backend to generate exactly one test per test set
    unsigned testIdx;

protected:
    TestGenBackend() {}
    virtual ~TestGenBackend() {}

public:
    // Called to check if tests shall be generated for the specified template.
    // If returned value is true, there is at least one test in this set.
    //
    // inst: instruction template (opcode, attributes and operands) used for test generation.
    //       Backend may inspect it but MUST NOT modify it.
    virtual bool beginTestSet(Inst inst) { testIdx = 0; return true; }

    // Called to initialize test data.
    // Return true on success and false if this test set shall be skipped
    virtual bool initTestData() { return true; }

    // Called to check if all tests for the current test set have been generated.
    // Return true if there is at least one more test; false if there are no more tests
    virtual bool genNextTestGroup() { return ++testIdx == 0; }

    // Called after brig container for the current test is created but before generation of test kernel.
    // This is a convenient place for backend to generate any required top-level directives and
    // definitions of auxiliary variables, fbarriers and functions used by test kernel.
    // (Note that test kernel is generated automatically by framework on next steps.)
    // If something went wrong, backend may skip generation of the current test by returning false.
    // Note that such a failure does not cancel further test generation for the current test set.
    //
    // context:  BRIG context (including brig container) used for test generation.
    //           Backend may save this context internally, however, it cannot be used after
    //           finishKernelBody is called.
    // testName: test name used for identification purposes, e.g. "abs_000"
    //
    // Return true on success and false if this test shall not be generated.
    //
    virtual bool beginTestGroup(BrigContext* context, string testName) { return true; }

    // Called to allow backend define test kernel arguments.
    // By default, no arguments are generated
    virtual void defKernelArgs() {}

    // Called after test kernel is defined but before generation of the instruction being tested.
    // This is a convenient place for backend to generate test prologue code
    // (e.g. load registers with test data)
    //
    // Note that this function is called once for each test in test group.
    // The resultant BRIG will include not one but several tests executed sequentally.
    //
    // tstIdx: index of test in the current test group
    //
    virtual void beginTestCode(unsigned tstIdx) {}

    // Called after generation of the instruction being tested.
    // This is a convenient place for backend to generate test epilogue code
    // (e.g. save test results)
    //
    // Note that this function is called once for each test in test group.
    // The resultant BRIG will include not one but several tests executed sequentally.
    //
    // tstIdx: index of test in the current test group
    //
    virtual void endTestCode(unsigned tstIdx) {}

    // Called after the framework has generated a new test instruction.
    // This instruction is a copy of original template instruction specified when
    // 'beginTestSet' was called. This copy is created in a separate context
    // and may be modified by the backend as required.
    //
    // This is the place for backend to create a new test by modifying original
    // (template) instruction. For example, backend may replace an original immediate
    // operand with a specific test value.
    //
    // Note that this function is called once for each test in test group.
    // The resultant BRIG will include not one but several tests executed sequentally.
    //
    // inst: copy of the original template instruction.
    //       Backend is allowed to modify this instruction (with some limitations).
    //       Generally speaking, backend is expected to change template instruction by
    //       mutating its operands (but retaining kind of each operand). All other
    //       modifications should be avoided because they lead to duplication of tests.
    //       (This is because TestGenManager generates all possible valid combinations
    //       of instruction modifiers and operands.)
    //
    //       Safe modifications include:
    //       - replace register with another register of the same type;
    //       - replace immediate constant with another constant of the same type;
    //       - replace vector registers and immediate values according to rules specified above;
    //       - replace offset of OperandAddress with another offset;
    //       - replace reference to a symbol from OperandAddress with a reference to another symbol.
    //
    // tstIdx: index of test in the current test group
    //
    virtual void makeTestInst(Inst inst, unsigned tstIdx) { }

    // Return the number of tests in the current test group
    virtual unsigned getTestGroupSize() { return 1; }

    // Update test description with backend-specific data.
    // Called by the framework when the current test is ready.
    virtual void registerTest(TestDesc& desc)  {}

    // Called just before context destruction
    // This is a good place for backend cleanup
    virtual void endTestGroup() {}

    // Called when all test for the current template (i.e. test set) have been generated.
    // This is a good place for backend cleanup
    virtual void endTestSet() {}

    static TestGenBackend* get(string name);
    static void dispose();
};

// ============================================================================

} // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_BACKEND_H
