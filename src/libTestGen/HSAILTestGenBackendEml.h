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

#ifndef INCLUDED_HSAIL_TESTGEN_EML_BACKEND_H
#define INCLUDED_HSAIL_TESTGEN_EML_BACKEND_H

#include "HSAILTestGenDataProvider.h"
#include "HSAILTestGenEmulator.h"
#include "HSAILTestGenBackend.h"
#include "HSAILTestGenUtilities.h"

#include <algorithm>

using std::string;
using std::ostringstream;
using std::setw;

using HSAIL_ASM::DirectiveKernel;
using HSAIL_ASM::DirectiveFunction;
using HSAIL_ASM::DirectiveIndirectFunction;
using HSAIL_ASM::DirectiveExecutable;
using HSAIL_ASM::DirectiveVariable;
using HSAIL_ASM::DirectiveLabel;
using HSAIL_ASM::DirectiveFbarrier;

using HSAIL_ASM::InstBasic;
using HSAIL_ASM::InstSourceType;
using HSAIL_ASM::InstAtomic;
using HSAIL_ASM::InstCmp;
using HSAIL_ASM::InstCvt;
using HSAIL_ASM::InstImage;
using HSAIL_ASM::InstMem;
using HSAIL_ASM::InstMod;
using HSAIL_ASM::InstBr;

using HSAIL_ASM::OperandOperandList;
using HSAIL_ASM::OperandCodeList;

using HSAIL_ASM::OperandRegister;
using HSAIL_ASM::OperandConstantBytes;
using HSAIL_ASM::OperandWavesize;
using HSAIL_ASM::OperandAddress;
using HSAIL_ASM::OperandCodeRef;

using HSAIL_ASM::isFloatType;
using HSAIL_ASM::isSignedType;
using HSAIL_ASM::isOpaqueType;
using HSAIL_ASM::isBitType;
using HSAIL_ASM::getOperandType;
using HSAIL_ASM::getPackedDstDim;
using HSAIL_ASM::getUnsignedType;
using HSAIL_ASM::getBitType;
using HSAIL_ASM::ItemList;

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================
// KERNEL STRUCTURE
//
// This backend generates kernels comprising a group of tests.
//
// Kernel arguments are addresses of src, dst and mem arrays.
//
// Src arrays shall be initialized with test data.
// The number of src arrays is the same as the number
// of src arguments of the instruction being tested.
//
// Dst array is used by kernel to save the value in dst register
// after execution of test instruction. This array is created only
// if the instruction being tested has destination.
//
// Mem array is used by kernel to save the value in memory
// after execution of test instruction. This array is created only
// if the instruction being tested affects memory.
//
// Results in dst and/or mem arrays shall be compared
// with expected values.
//
// Register map for generated code is as follows:
//
//   --------------------------------------------------------------
//   Registers              Usage
//   --------------------------------------------------------------
//   $c0  $s0  $d0  $q0     0-th argument of test instruction
//   $c1  $s1  $d1  $q1     1-th argument of test instruction
//   $c2  $s2  $d2  $q2     2-th argument of test instruction
//   $c3  $s3  $d3  $q3     3-th argument of test instruction
//   $c4  $s4  $d4  $q4     4-th argument of test instruction
//   --------------------------------------------------------------
//        $s5               Temporary               (REG_IDX_TMP)
//        $s6  $d6          Temporary array address (REG_IDX_ADDR)
//        $s7  $d7          Test index              (REG_IDX_ID)
//        $s8  $d8          First index  = id * X1  (REG_IDX_IDX1)
//        $s9  $d9          Second index = id * X2  (REG_IDX_IDX2)
//   --------------------------------------------------------------
//        $s10 $d10         first  vector register  (REG_IDX_VEC)
//        $s11 $d11         second vector register  (REG_IDX_VEC + 1)
//        $s12 $d12         third  vector register  (REG_IDX_VEC + 2)
//        $s13 $d13         fourth vector register  (REG_IDX_VEC + 3)
//   --------------------------------------------------------------
//
//   Other registers are not used
//
//==============================================================================
//==============================================================================
//==============================================================================
//
class EmlBackend : public TestGenBackend
{
private:
    // See register map info above
    static const unsigned REG_IDX_TMP   = 5;
    static const unsigned REG_IDX_ADDR  = 6;
    static const unsigned REG_IDX_ID    = 7;
    static const unsigned REG_IDX_IDX1  = 8;
    static const unsigned REG_IDX_IDX2  = 9;
    static const unsigned REG_IDX_VEC   = 10;

private:
    // HSAIL-specific segment size limitations
    static const uint64_t MIN_GROUP_SEGMENT_SIZE   = 32 * 1024ULL;
    static const uint64_t MIN_PRIVATE_SEGMENT_SIZE = 64 * 1024ULL;
    static const uint64_t MAX_SEGMENT_SIZE         = 0xFFFFFFFFFFFFFFFFULL;

private:
    // Worst case memory overhead for tests with memory 
    // (size of autogenerated array + worst case alignment size)
    static const uint64_t MAX_SEGMENT_OVERHEAD = TEST_ARRAY_SIZE + 256;

private:
    static const unsigned MAX_TESTS  = 0xFFFFFFFF;
    static const unsigned MAX_GROUPS = 0xFFFFFFFF;

    //==========================================================================
private:
    static const unsigned OPERAND_IDX_DST = 0;  // Index of destination operand (if any)

    //==========================================================================
private:
    BrigContext *context;                       // Brig context for code generation
    TestDataProvider *provider;                 // Provider of test data
    DirectiveVariable memTestArray;             // Array allocated for testing memory access

protected:
    Inst testSample;                            // Instruction template for this test. It MUST NOT be modified!
    TestDataFactory factory;                    // Test data factory which bundle test data into groups
    TestGroupArray* testGroup;                  // Test data for the current test group
    TestDataMap     testDataMap;                // Mapping of test data to arguments of test instruction
    string testName;                            // Test name (without extension)

    //==========================================================================
public:
    EmlBackend()
    {
        context = 0;
        provider = 0;
        testGroup = 0;
    }

    //==========================================================================
    // Backend Interface Implementation
public:

    // Called to check if tests shall be generated for the specified template.
    // If returned value is true, there is at least one test in this set.
    //
    // inst: instruction template (opcode, attributes and operands) used for test generation.
    //       Backend may inspect it but MUST NOT modify it.
    //
    bool beginTestSet(Inst readOnlyInst)
    {
        provider = 0;
        testGroup = 0;
        factory.reset();
        testSample = readOnlyInst;

        if (testableInst(readOnlyInst) && testableOperands(readOnlyInst) && testableTypes(readOnlyInst))
        {
            // Create a provider of test data for the current instruction.
            // Providers are selected based on data type of each operand.
            // Supported operand types and their test values must be defined
            // for each instruction in HSAILTestGenTestData.h
            provider = getProvider(readOnlyInst);
        }

        return provider != 0;
    }

    // Called when all test for the current template (i.e. test set) have been generated.
    // This is a good place for backend cleanup
    void endTestSet()
    {
        // Cleanup: important for brig proxies
        testSample = Inst();
        testGroup = 0;
        factory.reset();
        delete provider;
        provider = 0;
    }

    // Called to initialize test data.
    // Return true on success and false if this test set shall be skipped
    bool initTestData()
    { 
        assert(provider);

        setupDataMap();
        setupFactory();
        testGroup = factory.getNextGroup();

        return testGroup != 0;
    }

    // Called to check if all tests for the current test set have been generated.
    // Return true if there is at least one more test; false if there are no more tests
    bool genNextTestGroup()
    {
        assert(provider);
        testGroup = factory.getNextGroup();
        return testGroup != 0;
    }

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
    bool beginTestGroup(BrigContext* ctx, string name)
    {
        assert(testGroup);

        context  = ctx;
        testName = name;

        createMemTestArray(); // Create an array for testing memory access (if required)

        return true;
    }

    // Called just before context destruction
    // This is a good place for backend cleanup
    void endTestGroup()
    {
        // Cleanup: important for proxies
        memTestArray = DirectiveVariable();
        testGroup = 0;
        context = 0;
    }

    // This function declares kernel arguments.
    // These arguments are addresses of src, dst and mem arrays.
    // See detailed description of test kernel structure and interface
    // at the beginning of this file.
    void defKernelArgs()
    {
        for (int i = provider->getFirstSrcOperandIdx(); i <= provider->getLastOperandIdx(); i++)
        {
            context->emitSbrArg(getModelType(), getSrcArrayName(i, "%"));
        }
        if (hasDstOperand())    context->emitSbrArg(getModelType(), getDstArrayName("%"));
        if (hasMemoryOperand()) context->emitSbrArg(getModelType(), getMemArrayName("%"));
    }

    // Called after test kernel is defined but before generation of the instruction being tested.
    // This is a convenient place for backend to generate test prologue code
    // (e.g. load registers with test data)
    //
    // tstIdx: index of test in the current test group
    //
    void beginTestCode(unsigned tstIdx)
    {
        assert(provider);
        assert(testGroup);

        emitCommentSeparator();
        CommentBrig commenter(context);

        if (tstIdx == 0)
        {
            emitTestDescriptionHeader(commenter, testName, testSample, testGroup->getGroupSize());
            if (testGroup->getGroupSize() > 1) emitCommentSeparator();
        }

        emitTestDescriptionBody(commenter, testSample, *testGroup, testDataMap, tstIdx);

        emitLoadId(tstIdx);   // Generate code to load workitem id (used as an index to arrays with test data)
        emitInitCode(tstIdx); // Generate code to init all input registers and test variables

        emitCommentHeader("This is the instruction being tested:");
    }

    // Called after generation of the instruction being tested.
    // This is a convenient place for backend to generate test epilogue code
    // (e.g. save test results)
    //
    // tstIdx: index of test in the current test group
    //
    void endTestCode(unsigned tstIdx)
    {
        assert(provider);

        saveTestResults(tstIdx);
        emitCommentSeparator();
    }

    // Called after the framework has generated a new test instruction.
    // This instruction is a copy of original template instruction specified when
    // 'beginTestSet' was called. This copy is created in a separate context
    // and may be modified by the backend as required.
    //
    // This is the place for backend to create a new test by modifying original
    // (template) instruction. For example, backend may replace an original immediate
    // operand with a specific test value.
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
    void makeTestInst(Inst inst, unsigned tstIdx)
    {
        assert(inst);
        assert(provider);
        assert(testGroup);

        // Generate operands for test instruction based on test values from provider
        for (int i = provider->getFirstOperandIdx(); i <= provider->getLastOperandIdx(); i++)
        {
            assert(0 <= i && i < inst.operands().size());

            Operand operand = inst.operand(i);
            assert(operand);

            if (OperandRegister reg = operand)
            {
                assign(inst, i, getOperandReg(i));
            }
            else if (OperandOperandList reg = operand)
            {
                assign(inst, i, getOperandVector(tstIdx, i));
            }
            else if (OperandConstantBytes immed = operand)
            {
                assign(inst, i, getOperandImmed(tstIdx, i));
            }
            else if (OperandAddress addr = operand)
            {
                assert(!addr.reg() && addr.offset() == 0);
                assign(inst, i, getMemTestArrayAddr());
            }
            else if (OperandWavesize ws = operand)
            {
                // nothing to do
            }
            else
            {
                assert(false); // currently not supported
            }
        }
    }

    unsigned getTestGroupSize() 
    { 
        assert(testGroup);
        return testGroup->getGroupSize();
    }

    // Update test description with backend-specific data
    void registerTest(TestDesc& desc)
    {
        desc.setMap(&testDataMap);
        desc.setData(testGroup);
    }

    //==========================================================================
    // Kernel code generation
private:

    void emitLoadId(unsigned tstIdx)
    {
        emitCommentHeader("Set test index");
        initIdReg(tstIdx);
    }

    // Generate initialization code for all input registers and test variables
    void emitInitCode(unsigned tstIdx)
    {
        for (int i = provider->getFirstSrcOperandIdx(); i <= provider->getLastOperandIdx(); ++i)
        {
            assert(0 <= i && i < testSample.operands().size());

            Operand operand = testSample.operand(i);
            assert(operand);

            if (OperandRegister reg = operand)
            {
                emitCommentHeader("Initialization of input register " + getName(getOperandReg(i)));
                initSrcVal(getOperandReg(i), getSrcArrayIdx(i));
            }
            else if (OperandOperandList vec = operand)
            {
                if (getVectorRegSize(vec) != 0) // Vector has register elements
                {
                    emitCommentHeader("Initialization of input vector " + getName(getOperandVector(tstIdx, i)));
                    initSrcVal(getOperandVector(tstIdx, i), getSrcArrayIdx(i));
                }
            }
            else if (OperandAddress(operand))
            {
                emitCommentHeader("Initialization of memory");
                initMemTestArray(tstIdx, getSrcArrayIdx(i));
            }
        }

        if (hasMemoryOperand())
        {
            emitCommentHeader("Initialization of index register for memory access");
            initMemTestArrayIndexReg(tstIdx);
        }

        // This instruction generates packed value, but affects only one packed element
        if (getPacking(testSample) != BRIG_PACK_NONE &&
            getPackedDstDim(getType(testSample), getPacking(testSample)) == 1)
        {
            // There are packing controls (such as 'ss' and 'ss_sat') which result in partial dst modification.
            // To validate dst bits which should not be modified, init dst with some value before emulation.

            emitCommentHeader("Initialize dst register because test instruction modifies only part of dst value");
            initPackedDstVal(getOperandReg(provider->getDstOperandIdx()));
        }
    }

    void saveTestResults(unsigned tstIdx)
    {
        if (hasDstOperand())
        {
            assert(OPERAND_IDX_DST < getOperandsNum(testSample));

            Operand sampleDst = testSample.operand(OPERAND_IDX_DST);
            if (OperandRegister(sampleDst))
            {
                OperandRegister dst = getOperandReg(OPERAND_IDX_DST);
                emitCommentHeader("Saving dst register " + getName(dst));
                saveDstVal(dst, getDstArrayIdx());
            }
            else if (OperandOperandList(sampleDst))
            {
                OperandOperandList dst = getOperandVector(tstIdx, OPERAND_IDX_DST);
                emitCommentHeader("Saving dst vector " + getName(dst));
                saveDstVal(dst, getDstArrayIdx());
            }
            else
            {
                assert(false);
            }
        }

        if (hasMemoryOperand())
        {
            emitCommentHeader("Saving mem result");
            saveMemTestArray(tstIdx, getMemArrayIdx());
        }
    }

    //==========================================================================
    // Helpers for bundling tests together
private:

    //F NB: WS is unsigned
    bool isValidWsData(Val v) { return v.getAsB64(0) == TestDataProvider::getWavesize() && v.getAsB64(1) == 0; }

    // Reject unsuitable test values
    bool validateSrcData(unsigned operandIdx, Val v)
    {
        assert(operandIdx < getOperandsNum(testSample));

        Operand opr = testSample.operand(operandIdx);

        if (v.empty()) // destination operand, ignore
        {
            return true;
        }
        else if (OperandWavesize(opr))
        {
            return isValidWsData(v);
        }
        else if (OperandOperandList vec = opr)
        {
            unsigned dim = vec.elementCount();

            assert(v.getDim() == dim);

            for (unsigned i = 0; i < dim; ++i)
            {
                if (OperandWavesize(vec.elements(i)) && !isValidWsData(v[i])) return false;
            }
        }

        return true;
    }

    void setupDataMap()
    {
        unsigned firstSrcArgIdx = static_cast<unsigned>(provider->getFirstSrcOperandIdx());
        unsigned srcArgsNum = static_cast<unsigned>(provider->getLastOperandIdx() - provider->getFirstSrcOperandIdx() + 1);
        testDataMap.setupTestArgs(firstSrcArgIdx, srcArgsNum, hasDstOperand()? 1 : 0, hasMemoryOperand()? 1 : 0, getPrecision(testSample));
    }

    void setupFactory()
    {
        TestData td;

        unsigned maxTestsNum  = getMaxTotalTestNum();
        unsigned maxGroupSize = std::max(1U, provider->getMaxConstGroupSize());
        unsigned maxGroupsNum = TestDataProvider::getMaxGridSize();

        assert(maxTestsNum != 0);
        assert(maxGroupSize <= maxTestsNum);
        assert(maxGroupsNum != 0);

        factory.reset(maxGroupSize, maxGroupsNum, maxTestsNum);

        for(;;)
        {
            bool valid = true;
            for (unsigned i = 0; i < MAX_OPERANDS_NUM && valid; ++i)                  // Read current set of test data
            {
                td.src[i] = provider->getSrcValue(i);
                valid = validateSrcData(i, td.src[i]);
            }

            if (valid)
            {
                td.dst = emulateDstVal(testSample, td.src[0], td.src[1], td.src[2], td.src[3], td.src[4]);    // Return an empty value if emulation failed or there is no dst value
                td.mem = emulateMemVal(testSample, td.src[0], td.src[1], td.src[2], td.src[3], td.src[4]);    // Return an empty value if emulation failed or there is no mem value

                valid = (td.dst.empty() != hasDstOperand() &&                         // Check that all expected results have been provided by emulator
                         td.mem.empty() != hasMemoryOperand());
            }

            if (!valid) td.clear();
            
            // Register all test data with factory (even those combinations of data which shall not be used in testing).
            // This is important because factory expects test data in groups of maxGroupSize size.
            factory.append(td);

            // Request next set of test data, if any
            if (!provider->next())
            {
                factory.finishGroup(); // start next test group
                if (!provider->nextGroup()) break;
            }
        }

        factory.seal();
    }

    //==========================================================================
    // Access to registers
private:

    OperandRegister getTmpReg(unsigned size)  { return context->emitReg(size, REG_IDX_TMP); }
    OperandRegister getAddrReg()              { return context->emitReg(getModelSize(), REG_IDX_ADDR); }
    OperandRegister getIdReg(unsigned size)   { return context->emitReg(size, REG_IDX_ID); }

    OperandRegister getIdxReg(unsigned size, unsigned idx) { return context->emitReg(size == 0? getModelSize() : size, idx); }
    OperandRegister getIdxReg1(unsigned size = 0)          { return getIdxReg(size, REG_IDX_IDX1); }
    OperandRegister getIdxReg2(unsigned size = 0)          { return getIdxReg(size, REG_IDX_IDX2); }

    OperandConstantBytes getOperandImmed(unsigned tstIdx, unsigned idx) // Create i-th operand of test instruction
    {
        assert(idx < getOperandsNum(testSample));

        OperandConstantBytes immed = testSample.operand(idx);
        assert(immed);

        Val val = testGroup->getData(0, tstIdx).src[idx];
        return context->emitImm(immed.type(), val.getAsB64(0), val.getAsB64(1));
    }

    OperandRegister getOperandReg(unsigned idx) // Create register for i-th operand of test instruction.
    {
        assert(idx < MAX_OPERANDS_NUM);
        assert(idx < getOperandsNum(testSample));

        OperandRegister reg = testSample.operand(idx); // NB: this register is read-only!

        assert(reg);
        assert(getRegSize(reg) == 1  ||
               getRegSize(reg) == 32 ||
               getRegSize(reg) == 64 ||
               getRegSize(reg) == 128);

        return context->emitReg(getRegSize(reg), idx); // NB: create register in CURRENT context
    }

    OperandOperandList getOperandVector(unsigned tstIdx, unsigned idx)  // Create register vector for i-th operand of test instruction.
    {
        assert(idx < MAX_OPERANDS_NUM);
        assert(idx < getOperandsNum(testSample));
        assert(OperandOperandList(testSample.operand(idx)));

        OperandOperandList vec = testSample.operand(idx); // NB: this vector is read-only!
        unsigned regSize = getVectorRegSize(vec);
        assert(regSize == 0 || regSize == 32 || regSize == 64);

        unsigned cnt = vec.elementCount();
        assert(2 <= cnt && cnt <= 4);

        Val v = testGroup->getData(0, tstIdx).src[idx];
        assert(idx < testDataMap.getFirstSrcArgIdx() || v.getDim() == cnt);

        ItemList opnds;
        for (unsigned i = 0; i < cnt; ++i)
        {
            Operand opr;

            if (OperandRegister x = vec.elements(i))
            {
                opr = context->emitReg(regSize, REG_IDX_VEC + i);
            }
            else if (OperandConstantBytes x = vec.elements(i))
            {
                opr = context->emitImm(x.type(), v[i].getAsB64(0), v[i].getAsB64(1));
            }
            else
            {
                assert(OperandWavesize(vec.elements(i)));
                opr = context->emitWavesize();
            }

            opnds.push_back(opr);
        }

        OperandOperandList res = context->getContainer().append<OperandOperandList>();
        res.elements() = opnds;

        return res;
    }

    static unsigned getVectorRegSize(OperandOperandList vec)
    {
        unsigned dim = vec.elementCount();

        for (unsigned i = 0; i < dim; ++i)
        {
            if (OperandRegister x = vec.elements(i)) return getRegSize(x);
        }

        return 0; // vector has no register elements
    }

    static bool isVectorWithImm(Operand opr)
    {
        if (OperandOperandList vec = opr)
        {
            unsigned dim = vec.elementCount();

            for (unsigned i = 0; i < dim; ++i) if (!OperandRegister(vec.elements(i))) return true;
        }

        return false;
    }

    static bool isVectorWithWaveSize(Operand opr)
    {
        if (OperandOperandList vec = opr)
        {
            unsigned dim = vec.elementCount();

            for (unsigned i = 0; i < dim; ++i) if (OperandWavesize(vec.elements(i))) return true;
        }

        return false;
    }

    //==========================================================================
    // Operations with index registers (used to access array elements)
private:

    void initIdReg(unsigned tstIdx) // Load test index
    {
        // Load workitem id
        context->emitGetWorkItemId(getIdReg(32), 0); // Id for 0th dimension

        if (testGroup->getGroupSize() > 1)
        {
            context->emitMul(BRIG_TYPE_U32, getIdReg(32), getIdReg(32), testGroup->getGroupSize());
        }

        if (tstIdx != 0)
        {
            context->emitAdd(BRIG_TYPE_U32, getIdReg(32), getIdReg(32), tstIdx);
        }

        if (BrigSettings::isLargeModel()) // Convert to U64 if necessary
        {
            context->emitCvt(BRIG_TYPE_U64, BRIG_TYPE_U32, getIdReg(64), getIdReg(32));
        }
    }

    OperandRegister loadIndexReg(OperandRegister idxReg, unsigned dim, unsigned elemSize)
    {
        unsigned addrSize = getRegSize(idxReg);
        if (elemSize == 1) elemSize = 32; // b1 is a special case, always stored as b32
        context->emitMul(getUnsignedType(addrSize), idxReg, getIdReg(addrSize), dim * elemSize / 8);
        return idxReg;
    }

    OperandRegister loadIndexReg(OperandRegister idxReg, unsigned slotSize)
    {
        assert(slotSize > 0);
        assert(slotSize % 8 == 0);

        unsigned addrSize = getRegSize(idxReg);
        context->emitMul(getUnsignedType(addrSize), idxReg, getIdReg(addrSize), slotSize / 8);
        return idxReg;
    }

    // For each test, test data are available at &var0[indexReg + alignOffset].
    // indexReg is initialized as follows:
    // - for tests working with private segment, indexReg = tstIdx * bundleSize
    // - for tests working with other segments,  indexReg = flatTstIdx * bundleSize
    //
    OperandRegister loadMemIndexReg(unsigned tstIdx, OperandRegister idxReg, unsigned memBundleSize)
    {
        assert(memBundleSize > 0);
        assert(memBundleSize % 8 == 0);

        if (isPrivateMemSeg())
        {
            unsigned addrSize = getRegSize(idxReg);
            unsigned type = getUnsignedType(addrSize);
            context->emitMov(type, idxReg, context->emitImm(type, tstIdx * memBundleSize / 8));
            return idxReg;
        }
        else
        {
            return loadIndexReg(idxReg, memBundleSize);
        }
    }

    //==========================================================================
    // Low-level operations with arrays
private:

    OperandRegister loadGlobalArrayAddress(OperandRegister addrReg, OperandRegister indexReg, unsigned arrayIdx)
    {
        assert(addrReg);
        assert(indexReg);
        assert(getRegSize(addrReg) == getRegSize(indexReg));

        context->emitLd(getModelType(), BRIG_SEGMENT_KERNARG, addrReg, context->emitAddrRef(getArray(arrayIdx)));
        context->emitAdd(getModelType(), addrReg, addrReg, indexReg);
        return addrReg;
    }

    unsigned getSrcArrayIdx(unsigned idx)
    {
        assert(static_cast<unsigned>(provider->getFirstSrcOperandIdx()) <= idx);
        assert(idx <= static_cast<unsigned>(provider->getLastOperandIdx()));

        return idx - provider->getFirstSrcOperandIdx();
    }

    unsigned getDstArrayIdx()
    {
        assert(hasDstOperand());
        return provider->getLastOperandIdx() - provider->getFirstSrcOperandIdx() + 1;
    }

    unsigned getMemArrayIdx()
    {
        assert(hasMemoryOperand());
        return provider->getLastOperandIdx() - provider->getFirstSrcOperandIdx() + (hasDstOperand()? 2 : 1);
    }

    DirectiveVariable getArray(unsigned idx)
    {
        return HSAIL_ASM::getInputArg(context->getCurrentSbr(), idx);
    }

    //==========================================================================
    // Operations with src/dst arrays
private:

    void initSrcVal(OperandRegister reg, unsigned arrayIdx)
    {
        assert(reg);

        OperandRegister indexReg = loadIndexReg(getIdxReg1(), 1, getRegSize(reg));
        OperandRegister addrReg  = loadGlobalArrayAddress(getAddrReg(), indexReg, arrayIdx);
        OperandAddress addr      = context->emitAddrRef(addrReg);
        ldReg(getRegSize(reg), reg, addr);
    }

    void initSrcVal(OperandOperandList vector, unsigned arrayIdx)
    {
        assert(vector);

        unsigned dim     = vector.elementCount();
        unsigned regSize = getVectorRegSize(vector);

        assert(regSize == 32 || regSize == 64);

        OperandRegister indexReg = loadIndexReg(getIdxReg1(), dim, regSize);
        OperandRegister addrReg  = loadGlobalArrayAddress(getAddrReg(), indexReg, arrayIdx);

        for (unsigned i = 0; i < dim; ++i)
        {
            if (OperandRegister reg = vector.elements(i))
            {
                OperandAddress addr = context->emitAddrRef(addrReg, getSlotSize(regSize) / 8 * i);
                ldReg(regSize, context->emitReg(getRegSize(reg), reg.regNum()), addr);
            }
        }
    }

    void initPackedDstVal(OperandRegister reg)
    {
        assert(reg);

        unsigned type = getBitType(getRegSize(reg));
        context->emitMov(type, reg, context->emitImm(type, initialPackedVal, initialPackedVal));
    }

    void saveDstVal(OperandRegister reg, unsigned arrayIdx)
    {
        assert(reg);

        OperandRegister indexReg = loadIndexReg(getIdxReg1(), 1, getRegSize(reg));
        OperandRegister addrReg  = loadGlobalArrayAddress(getAddrReg(), indexReg, arrayIdx);
        OperandAddress addr      = context->emitAddrRef(addrReg);
        stReg(getRegSize(reg), reg, addr);
    }

    void saveDstVal(OperandOperandList vector, unsigned arrayIdx)
    {
        assert(vector);

        unsigned dim     = vector.elementCount();
        unsigned regSize = getVectorRegSize(vector);

        assert(regSize == 32 || regSize == 64);

        OperandRegister indexReg = loadIndexReg(getIdxReg1(), dim, regSize);
        OperandRegister addrReg  = loadGlobalArrayAddress(getAddrReg(), indexReg, arrayIdx);

        for (unsigned i = 0; i < dim; ++i)
        {
            OperandAddress addr = context->emitAddrRef(addrReg, getSlotSize(regSize) / 8 * i);
            OperandRegister reg = vector.elements(i);
            assert(reg); // dst vectors cannot include imm elements

            stReg(regSize, context->emitReg(reg), addr);
        }
    }

    void ldReg(unsigned elemSize, OperandRegister reg, OperandAddress addr)
    {
        assert(reg);
        assert(addr);

        if (elemSize == 1)
        {
            OperandRegister tmpReg = getTmpReg(32);
            context->emitLd(BRIG_TYPE_B32, BRIG_SEGMENT_GLOBAL, tmpReg, addr);
            context->emitCvt(BRIG_TYPE_B1, BRIG_TYPE_U32, reg, tmpReg);
        }
        else
        {
            context->emitLd(getBitType(elemSize), BRIG_SEGMENT_GLOBAL, reg, addr);
        }
    }

    void stReg(unsigned elemSize, OperandRegister reg, OperandAddress addr)
    {
        assert(reg);
        assert(addr);

        if (elemSize == 1)
        {
            OperandRegister tmpReg = getTmpReg(32);
            context->emitCvt(BRIG_TYPE_U32, BRIG_TYPE_B1, tmpReg, reg);
            context->emitSt(BRIG_TYPE_B32, BRIG_SEGMENT_GLOBAL, tmpReg, addr);
        }
        else
        {
            context->emitSt(getBitType(elemSize), BRIG_SEGMENT_GLOBAL, reg, addr);
        }
    }

    //==========================================================================
    // Operations with memory test array (required for instructions which access memory)
private:

    bool hasMemoryOperand()
    {
        for (int i = 0; i < testSample.operands().size(); ++i)
        {
            if (OperandAddress(testSample.operand(i))) return true;
        }
        return false;
    }

    bool hasVectorOperand()
    {
        for (int i = 0; i < testSample.operands().size(); ++i)
        {
            if (OperandOperandList(testSample.operand(i))) return true;
        }
        return false;
    }

    void createMemTestArray()
    {
        if (hasMemoryOperand())
        {
            assert(getMemTestArraySegment() != BRIG_SEGMENT_NONE);
    
            unsigned elemType = getMemTestArrayDeclType();
            unsigned dim      = getMemTestArrayDeclDim();
            unsigned align    = getMemTestArrayDeclAlign();
            unsigned segment  = getMemTestArraySegment();
            const char* name  = getTestArrayName();
    
            emitComment();
            emitComment("Array for testing memory access");
            //dumpMemoryProperties();

            memTestArray = context->emitSymbol(elemType, name, segment, dim);
            memTestArray.align() = align;
        }
    }

    // This function is used to:
    //   - initialize memory test array with test values;
    //   - unload results from memory test array to an output array.
    void copyMemTestArray(unsigned tstIdx, unsigned arrayIdx, bool isDst)
    {
        assert(memTestArray);

        unsigned glbAddrSize = getModelSize();
        unsigned memAddrSize = context->getSegAddrSize(getMemTestArraySegment());

        // Initialize index register 'dataIndexReg' to access test values for arguments
        // of the instruction being tested. For each workitem id, test values for an argument X
        // are available at %argX[dataIndexReg * id].
        // Note that these test values are not accessed directly by instruction being
        // tested; they have to be copied to a separate 'memory test array'
        //
        unsigned dataElemSize        = getMemDataElemSize();
        unsigned dataSlotSize        = getSlotSize(dataElemSize);
        unsigned dataBundleSize      = dataSlotSize * getMaxDim();
        OperandRegister dataIndexReg = loadIndexReg(getIdxReg1(glbAddrSize), dataBundleSize);

        // Initialize index register 'memIndexReg' to acess memory test array.
        // Test data are available at &var0[memIndexReg + alignOffset]
        unsigned memBundleSize      = getMemTestArrayBundleSize();
        unsigned memBundleOffset    = getMemTestArrayBundleOffset() / 8;
        OperandRegister memIndexReg = dataIndexReg; // Reuse first index register if possible
        if (isPrivateMemSeg() || glbAddrSize != memAddrSize || dataBundleSize != memBundleSize)
        {
            memIndexReg = loadMemIndexReg(tstIdx, getIdxReg2(memAddrSize), memBundleSize);
        }

        // Load address of test data in arguments array 
        OperandRegister addrReg = loadGlobalArrayAddress(getAddrReg(), dataIndexReg, arrayIdx);

        unsigned atomType   = getMemTestArrayAtomType();
        unsigned atomSize   = getMemDataAtomSize();
        unsigned memDim     = std::max(1U, dataElemSize / atomSize);
        OperandRegister reg = getTmpReg(testLdSt()? 32 : dataSlotSize);

        unsigned vectorDim = getMaxDim();

        // Copy test values to/from memory test array
        //
        for (unsigned i = 0; i < vectorDim; ++i)
        {
            if (isDst) // Initialize memory test array
            {
                for (unsigned m = 0; m < memDim; ++m)
                {
                    OperandAddress dataAddr = context->emitAddrRef(addrReg, dataSlotSize / 8 * i + m);
                    OperandAddress memAddr  = getMemTestArrayAddr(memIndexReg, dataElemSize, i, m + memBundleOffset);

                    ldReg(atomSize, reg, dataAddr);
                    context->emitSt(atomType, getMemTestArraySegment(), reg, memAddr);
                }
            }
            else // Save results stored in memory test array
            {
                for (unsigned m = 0; m < memDim; ++m)
                {
                    OperandAddress dataAddr = context->emitAddrRef(addrReg, dataSlotSize / 8 * i + m);
                    OperandAddress memAddr  = getMemTestArrayAddr(memIndexReg, dataElemSize, i, m + memBundleOffset);

                    context->emitLd(atomType, getMemTestArraySegment(), reg, memAddr);
                    stReg(atomSize, reg, dataAddr);
                }

                // Subword values are saved as s32/u32.
                // The code below stores upper bits of 32-bit result.
                if (dataElemSize < 32)
                {
                    assert(testLdSt());
                    
                    if (getMemDataElemType() == BRIG_TYPE_F16) // For F16 upper bits should be 0
                    {
                        assert(getRegSize(reg) == 32);
                        context->emitMov(BRIG_TYPE_B32, reg, context->emitImm(BRIG_TYPE_B32, 0, 0));
                    }
                    else
                    {
                        unsigned slotType = isSignedType(atomType) ? BRIG_TYPE_S32 : BRIG_TYPE_U32;
                        context->emitShr(slotType, reg, reg, 8); // copy sign bits from uppermost loaded byte
                    }

                    for (unsigned m = memDim; m < 4; ++m)
                    {
                        OperandAddress dataAddr = context->emitAddrRef(addrReg, dataSlotSize / 8 * i + m);

                        stReg(atomSize, reg, dataAddr);
                    }
                }
            }
        }
    }

    void initMemTestArray(unsigned tstIdx, unsigned arrayIdx) { copyMemTestArray(tstIdx, arrayIdx, true); }
    void saveMemTestArray(unsigned tstIdx, unsigned arrayIdx) { copyMemTestArray(tstIdx, arrayIdx, false); }

    OperandAddress getMemTestArrayAddr(OperandRegister idxReg, unsigned elemSize = 0, unsigned elemIdx = 0, unsigned offset = 0)
    {
        assert(memTestArray);
        offset += ((elemSize + 7) / 8) * elemIdx; // account for B1 type (1 byte)
        return context->emitAddrRef(memTestArray, idxReg, offset);
    }

    //==========================================================================
    // COMPUTATION OF MEMORY ARRAY PROPERTIES
    //==========================================================================
    //
    // Memory in test array has the following structure:
    //
    //     Header, Bundle#0, Bundle#1, ... Bundle#N
    //
    // Header is an optional memory allocated to ensure alignment specified by ld/st.
    // Header is empty for other operations.
    //
    // Bundles are used by workitems for operations with memory. Each workitem has one bundle.
    // All bundles have the same size and alignment.
    //
    // A bundle has the following structure:
    //
    //     Element#0, Element#1, ... Element#M, Footer
    //
    // For scalar operations, M = 1.
    // For vector operations, M is the number of vector elements.
    // Element type is specified by operation.
    //
    // Footer is an optional memory required to ensure _minimum_ required alignment of next bundle
    // as requested by operation being tested.
    //
    // For ld/st, memory test array is defined as u8/s8 with alignment=256.
    // For other operations the type of array is specified by operation and alignment is natural.
    //
    // Helper instructions that load/unload to/from memory test array operate with 'atoms'
    // which are naturally aligned pieces of memory. Atoms are introduced only to simplify helper code.
    //
    // For tests with ld/st, atoms are bytes. 
    // For other instructions, atoms have the same type and size as elements.
    //

    bool testLdSt() { return testSample.opcode() == BRIG_OPCODE_LD || testSample.opcode() == BRIG_OPCODE_ST; }

    unsigned getRequiredMemAlignNum()    { InstMem inst = testSample; return inst? HSAIL_ASM::align2num(inst.align()) * 8: 0; }

    bool     isPrivateMemSeg()           { return getMemTestArraySegment() == BRIG_SEGMENT_PRIVATE; }
    unsigned getMemTestArraySegment()    { return HSAIL_ASM::getSegment(testSample); }
    unsigned getMemTestArraySizeInBytes(){ return (getMemTestArrayBundleOffset() - getFooterSize() + 
                                                   getMemTestArrayBundleSize() * getMemTestArrayBundlesNum()) / 8; } // size in bytes

    unsigned getMemTestArrayDeclAlign()  { return testLdSt()? BRIG_ALIGNMENT_256 : getNaturalAlignment(getMemDataElemType()); } // array declaration alignment
    unsigned getMemTestArrayDeclDim()    { return getMemTestArraySizeInBytes() / (getMemTestArrayAtomSize() / 8); } // array declaration size (in atoms)
    unsigned getMemTestArrayDeclType()   { return getMemTestArrayAtomType(); }                                      // array declaration type

    unsigned getMemTestArrayBundleOffset() { return testLdSt()? getRequiredMemAlignNum() : 0; }
    unsigned getMemTestArrayBundlesNum() { return isPrivateMemSeg()? testGroup->getGroupSize() : testGroup->getFlatSize(); } // size in bytes
    unsigned getMemTestArrayBundleSize() 
    { 
        unsigned dim = getMaxDim();

        // Ensure proper alignment of subsequent bundles
        if (testLdSt() && dim == 3) dim = 4; 
        return getMemDataElemSize() * dim + getFooterSize();
    }

    // Footer size is necessary to ensure MINIMAL required alignment of next bundle
    // Note that bundle size must be a power of two (for the same reason)
    unsigned getFooterSize() 
    { 
        unsigned dim = getMaxDim();

        if (testLdSt())
        {
            if (dim == 3) dim = 4; // must be a power of two
            unsigned maxDataSize = getMemDataElemSize() * dim;
            // Ensure proper alignment of subsequent bundles
            unsigned minAlignSize = getRequiredMemAlignNum() * 2;
            if (minAlignSize > maxDataSize) return minAlignSize - maxDataSize;
        }

        return 0;
    }

    unsigned getMemDataElemType() { return testSample.type(); }
    unsigned getMemDataElemSize() { return getBrigTypeNumBits(getMemDataElemType()); }
    unsigned getMemDataAtomSize() { return testLdSt()? 8 : getSlotSize(getMemDataElemSize()); }

    unsigned getMemTestArrayAtomSize() { return getBrigTypeNumBits(getMemTestArrayAtomType()); }
    unsigned getMemTestArrayAtomType() 
    { 
        unsigned elemType = getMemDataElemType(); 
        return testLdSt()? (isSignedType(elemType) ? BRIG_TYPE_S8 : BRIG_TYPE_U8) : elemType; 
    }

    uint64_t getMinSegmentSize() 
    { 
        unsigned segment = getMemTestArraySegment();
        if (segment == BRIG_SEGMENT_GROUP)   return MIN_GROUP_SEGMENT_SIZE;
        if (segment == BRIG_SEGMENT_PRIVATE) return MIN_PRIVATE_SEGMENT_SIZE;
        return MAX_SEGMENT_SIZE; // unlimited
    }

    // Total number of tests in test group may be limited 
    // by amount of available memory in the segment being tested
    unsigned getMaxTotalTestNum()
    {
        unsigned maxTestNum = MAX_TESTS; // unlimited

        if (testLdSt()) 
        {
            uint64_t available  = getMinSegmentSize();
            unsigned bundleSize = getMemTestArrayBundleSize() / 8;

            // Compute max number of tests taking into account memory overhead
            // (i.e. size of autogenerated helper definitions and alignment)
            // This overhead affects max size of memory array.
            // Note that private memory array is allocated per test group,
            // while arrays in other segments are allocated for all tests.
            if (isPrivateMemSeg())
            {
                // For private segment, memory overhead should be computed per test group (not per test),
                // but group size connot be estimated at this moment (note that test factory may not be created yet).
                // So the code below computes memory overhead for the worst case (i.e. group size = 1).
                bundleSize += MAX_SEGMENT_OVERHEAD;
            }
            else
            {
                // For other segments overhead is per work group (i.e. per test array)
                available -= MAX_SEGMENT_OVERHEAD;
            }

            maxTestNum = static_cast<unsigned>(available / bundleSize);
        }

        assert(maxTestNum > 0);
        return maxTestNum;
    }

    void dumpMemoryProperties()
    {
        if (testLdSt())
        { 
            ostringstream s1;    s1 << getMaxDim();
            ostringstream s2;    s2 << testGroup->getFlatSize();
            ostringstream s3;    s3 << getRequiredMemAlignNum() / 8 << " bytes";
            ostringstream s4;    s4 << getMemTestArrayBundleOffset() / 8 << " bytes";
            ostringstream s5;    s5 << getMemTestArrayBundleSize() / 8 << " bytes";
        
            emitComment();
            emitComment("    -- elem type:     " + std::string(HSAIL_ASM::type2name(getMemDataElemType())));
            emitComment("    -- vec dim:       " + s1.str());
            emitComment("    -- num of tests:  " + s2.str());
            emitComment("    -- ld/st align:   " + s3.str());
            emitComment("    --                ");
            emitComment("    -- bundle offset: " + s4.str());
            emitComment("    -- bundle size:   " + s5.str());
            emitComment();
        }
    }
    //==========================================================================

    void initMemTestArrayIndexReg(unsigned tstIdx)
    {
        unsigned memAddrSize = context->getSegAddrSize(getMemTestArraySegment());
        loadMemIndexReg(tstIdx, getIdxReg1(memAddrSize), getMemTestArrayBundleSize());
    }

    Operand getMemTestArrayAddr()
    {
        assert(memTestArray);
        unsigned memAddrSize = context->getSegAddrSize(getMemTestArraySegment());
        unsigned memBundleOffset = getMemTestArrayBundleOffset() / 8;

        return getMemTestArrayAddr(getIdxReg1(memAddrSize), 0, 0, memBundleOffset);
    }

    //==========================================================================
    // Comments generation
private:

    struct CommentBrig
    {
        BrigContext* context;
        CommentBrig(BrigContext* ctx) : context(ctx) {}
        void operator()(string s) { context->emitComment(s); }
    };

    void emitComment(string text = "")
    {
        context->emitComment(text);
    }

    void emitCommentHeader(string text)
    {
        emitCommentSeparator();
        emitComment(text);
        emitComment();
    }

    void emitCommentSeparator()
    {
        emitComment();
        emitComment("======================================================");
    }

    //==========================================================================
    // Symbol names
protected:

    string getSrcArrayName(unsigned idx, string prefix = "") { return prefix + "src" + index2str(idx); }
    string getDstArrayName(              string prefix = "") { return prefix + "dst"; }
    string getMemArrayName(              string prefix = "") { return prefix + "mem"; }

    const char* getTestArrayName() { return "&var0"; }

    //==========================================================================
    // Helpers
private:

    static unsigned getModelType() { return BrigSettings::getModelType(); }
    static unsigned getModelSize() { return BrigSettings::getModelSize(); }

    static string   getName(OperandRegister reg) { return getRegName(reg); }
    static string   getName(OperandOperandList vector)
    {
        string res;
        for (unsigned i = 0; i < vector.elementCount(); ++i)
        {
            res += (i > 0? ", " : "");
            if (OperandRegister reg = vector.elements(i))
            {
                res += getRegName(reg);
            }
            else if (OperandConstantBytes imm = vector.elements(i))
            {
                res += "imm";
            }
            else
            {
                assert(OperandWavesize(vector.elements(i)));
                res += "ws";
            }
        }
        return "(" + res + ")";
    }

    static unsigned getSlotSize(unsigned typeSize)
    {
        switch(typeSize)
        {
        case 1:
        case 8:
        case 16:
        case 32:    return 32;
        case 64:    return 64;
        case 128:   return 128;
        default:
            assert(false);
            return 0;
        }
    }

    static unsigned getAtomicSrcNum(InstAtomic inst)
    {
        assert(inst);

        unsigned atmOp = inst.atomicOperation();
        return (atmOp == BRIG_ATOMIC_CAS) ? 3 : (atmOp == BRIG_ATOMIC_LD) ? 1 : 2;
    }

    bool hasDstOperand()
    {
        assert(provider);
        return provider->getDstOperandIdx() >= 0;
    }

    unsigned getMaxDim() { return getMaxDim(testSample); }

    static unsigned getMaxDim(Inst inst)
    {
        assert(inst);

        for (int i = 0; i < inst.operands().size(); ++i)
        {
            OperandOperandList vec = inst.operand(i);
            if (vec) return vec.elementCount();
        }
        return 1;
    }

    //==========================================================================
    //==========================================================================
    //==========================================================================
    // Providers of test data

private:

    TestDataProvider* getProvider(Inst inst)
    {
        TestDataProvider* p = makeProvider(inst, false);

        if (p && getMaxTotalTestNum() < p->getMaxConstGroupSize())
        {
            delete p;
            p = makeProvider(inst, true);
            assert(p->getMaxConstGroupSize() == 0);
        }

        return p;
    }

    // Create a provider of test data for the current instruction.
    // Providers are selected based on data type of each operand.
    // Supported operand types for each instruction must be defined
    // in HSAILTestGenTestData.h
    // If the current instruction is not described or required type
    // is not found, this test will be rejected.
    static TestDataProvider* makeProvider(Inst inst, bool lockConst)
    {
        assert(inst);

        TestDataProvider* p;

        switch (inst.kind())
        {
        case BRIG_KIND_INST_BASIC:
        case BRIG_KIND_INST_MOD:         p = TestDataProvider::getProvider(inst.opcode(), inst.type(), inst.type());                        break;
        case BRIG_KIND_INST_CVT:         p = TestDataProvider::getProvider(inst.opcode(), inst.type(), InstCvt(inst).sourceType(), AluMod(InstCvt(inst).round())); break;
        case BRIG_KIND_INST_CMP:         p = TestDataProvider::getProvider(inst.opcode(), inst.type(), InstCmp(inst).sourceType());         break;
        case BRIG_KIND_INST_ATOMIC:      p = TestDataProvider::getProvider(inst.opcode(), inst.type(), inst.type(),                AluMod(), getAtomicSrcNum(inst)); break;
        case BRIG_KIND_INST_SOURCE_TYPE: p = TestDataProvider::getProvider(inst.opcode(), inst.type(), InstSourceType(inst).sourceType());  break;
        case BRIG_KIND_INST_MEM:         p = TestDataProvider::getProvider(inst.opcode(), inst.type(), InstMem(inst).type());               break;
        default:                         p = 0; /* other formats are not currently supported */                                             break;
        }

        if (p)
        {
            unsigned maxDim = getMaxDim(inst);

            // By default, tests for source non-immediate operands can be grouped together to speedup testing
            for (int i = p->getFirstSrcOperandIdx(); i <= p->getLastOperandIdx(); ++i)
            {
                assert(0 <= i && i < inst.operands().size());

                Operand opr = inst.operand(i);
                assert(opr);

                // NB: If there are vector operands, memory operands (if any) must be processed in similar way.
                unsigned dim     = (OperandOperandList(opr) || OperandAddress(opr))? maxDim : 1;
                bool     isConst = OperandConstantBytes(opr) || OperandWavesize(opr) || isVectorWithImm(opr);

                p->registerOperand(i, dim, isConst, lockConst);
            }
            p->reset();
        }

        return p;
    }

    //==========================================================================
    //==========================================================================
    //==========================================================================
    // This section of code describes limitations on instructions
    // which could be tested

private:
    // Check generic limitations on operands.
    static bool testableOperands(Inst inst)
    {
        assert(inst);

        for (int i = 0; i < inst.operands().size(); ++i)
        {
            Operand operand = inst.operand(i);
            if (!operand) return true;          // no gaps!

            if (OperandAddress addr = operand)
            {
                DirectiveVariable var = addr.symbol();
                return var && !isOpaqueType(addr.symbol().elementType()) && !addr.reg() && addr.offset() == 0;
            }
            else if (OperandWavesize(operand))
            {
                if (TestDataProvider::getWavesize() == 0) return false;
            }
            else if (OperandOperandList(operand))
            {
                if (TestDataProvider::getWavesize() == 0 && isVectorWithWaveSize(operand)) return false;
            }
            else if (!OperandOperandList(operand) && !OperandRegister(operand) && !OperandConstantBytes(operand))
            {
                return false;
            }
        }

        return true;
    }

    static bool testableTypes(Inst inst)
    {
        assert(inst);
        if (isF16(getType(inst)) || isF16(getSrcType(inst))) {
            if (!TestDataProvider::testF16())
                return false;
            if (isHavingFtz(inst))
                return TestDataProvider::testFtzF16();
            return true;
        }
        return true;
    }

    static bool isHavingFtz(const Inst& inst)
    {
      if (! instSupportsFtz(Inst(inst).opcode())) { return false; }
      if (InstCmp i = inst) { return i.modifier().ftz(); }
      if (InstCvt i = inst) { return i.modifier().ftz(); }
      if (InstMod i = inst) { return i.modifier().ftz(); }
      return false;
    }

    static bool isF16(unsigned type)
    {
        switch(type)
        {
        case BRIG_TYPE_F16:
        case BRIG_TYPE_F16X2:
        case BRIG_TYPE_F16X4:
        case BRIG_TYPE_F16X8:
            return true;
        default:
            return false;
        }
    }

}; // class EmlBackend

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_EML_BACKEND_H
