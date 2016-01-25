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

#ifndef INCLUDED_HSAIL_TESTGEN_MANAGER_H
#define INCLUDED_HSAIL_TESTGEN_MANAGER_H

#include "HSAILTestGenContext.h"
#include "HSAILTestGenInstSetManager.h"
#include "HSAILTestGenInstDesc.h"
#include "HSAILTestGenTestDesc.h"
#include "HSAILTestGenProvider.h"
#include "HSAILTestGenBackend.h"

#include <cassert>

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================
// This abstract class manages test generation and interacts with backend.
// Subclasses must implement required functionality to transfer generated tests to
// consumers. This functionality may include test saving, test running, logging
// and so on.

class TestGenManager
{
private:
    const bool      isPositive;      // test type: positive or negative
    const bool      isSinglePackage; // test package: single or separate
    const bool      genMod;          //
    const bool      genBasic;        //
    TestGenBackend* backend;         // optional backend
    Context*        context;         // test context which includes brig container, symbols, etc
    unsigned        testIdx;         // total number of generated tests

    //==========================================================================

public:
    TestGenManager(string backendName, bool positive, bool single, bool mod, bool basic) :
        isPositive(positive), isSinglePackage(single), genMod(mod), genBasic(basic), testIdx(0)
    {
        backend = TestGenBackend::get(backendName);
    }

    virtual ~TestGenManager()
    {
        TestGenBackend::dispose();
    }

    bool     isPositiveTest()   const { return isPositive; }
    unsigned getGlobalTestIdx() const { return testIdx; }

    //==========================================================================

public:
    virtual bool generate()
    {
        start();

        unsigned num = InstSetManager::getOpcodesNum();
        for (unsigned i = 0; i < num; ++i)
        {
            unsigned opcode = InstSetManager::getOpcode(i);
            // filter out opcodes which should not be tested
            if (!isOpcodeEnabled(opcode)) continue;

            // skip generation of tests for special opcodes
            if (HSAIL_ASM::isCallOpcode(opcode)) continue;  //F generalize
            if (opcode == BRIG_OPCODE_SBR) continue;        //F

            // Regular tests generation. For instructions which may be encoded
            // using InstBasic and InstMod formats, only InstMod version is generated.
            if (genMod)
            {
                std::unique_ptr<TestGen> desc(TestGen::create(opcode));
                generateTests(*desc);
            }

            // Optional generation of InstBasic version for instructions encoded in InstMod format
            if (InstDesc::getFormat(opcode) == BRIG_KIND_INST_MOD && genBasic)
            {
                std::unique_ptr<TestGen> basicDesc(TestGen::create(opcode, true));
                generateTests(*basicDesc); // for InstBasic format
            }
        }

        finish();
        return true;
    }

    //==========================================================================
    // Functions to be redifined in subclasses
protected:

    // Return true if tests shall be generated for this opcode
    virtual bool isOpcodeEnabled(unsigned opcode) = 0;

    // Return true if tests shall be generated for this instruction
    virtual bool startTest(Inst inst) = 0;

    // Return test name used for reference purposes (e.g. in comments)
    virtual string getTestName() = 0;

    // Called to notify about test case which has just been generated
    virtual void testComplete(TestDesc& testDesc) = 0;

    //==========================================================================
private:
    void start()
    {
        if (isSinglePackage)
        {
            context = new Context();
            context->defineTestKernel();
            context->startKernelBody();
        }
    }

    void finish()
    {
        if (isSinglePackage)
        {
            context->finishKernelBody();
            registerTest(context);
            delete context;
        }
    }

    //==========================================================================
private:

    void generateTests(TestGen& desc)
    {
        if (isPositive)
        {
            genPositiveTests(desc);
        }
        else
        {
            genNegativeTests(desc);
        }
    }

    void genPositiveTests(TestGen& test)
    {
        for (bool start = true; test.nextPrimarySet(start); start = false)
        {
            finalizePositiveSample(test, true);

            for (; test.nextSecondarySet(); )
            {
                finalizePositiveSample(test, false);
            }
        }
    }

    // NB: 'nextSecondarySet' is not called for negative tests to avoid
    // generation of large number of identical tests
    void genNegativeTests(TestGen& test)
    {
        for (bool start = true; test.nextPrimarySet(start); start = false)
        {
            // Provide a reference to original valid sample (for inspection purpose)
            if (isSinglePackage && BrigSettings::commentsEnabled()) createPositiveTest(test, true);

            unsigned id;
            unsigned val;
            for (test.resetNegativeSet(); test.nextNegativeSet(&id, &val); )
            {
                finalizeNegativeSample(test, id, val);
            }
        }
    }

    //==========================================================================
private:

    void finalizePositiveSample(TestGen& test, bool start)
    {
        const Sample positiveSample = test.getPositiveSample();
        Inst inst = positiveSample.getInst();

        assert(InstSetManager::isValidInst(inst));

        if (isSinglePackage)
        {
            if (startTest(inst))
            {
                createPositiveTest(test, start);
                ++testIdx;
            }
        }
        else
        {
            if (backend->beginTestSet(inst) && startTest(inst) && backend->initTestData())
            {
                for (;;)
                {
                    Context* ctx = new Context(positiveSample, true, backend->genDefaultSymbols());
                    if (backend->beginTestGroup(ctx, getTestName()))
                    {
                        ctx->defineTestKernel();
                        backend->defKernelArgs();

                        ctx->startKernelBody();
                        backend->beginTestCode();

                        Sample res;
                        for (unsigned tstIdx = 0; tstIdx < backend->getTestGroupSize(); ++tstIdx)
                        {
                            backend->beginTestCode(tstIdx);
                            res = ctx->cloneSample(positiveSample);
                            backend->makeTestInst(res.getInst(), tstIdx);
                            backend->endTestCode(tstIdx);
                        }

                        ctx->finishKernelBody();

                        registerTest(ctx, res.getInst());
                        ++testIdx;
                    }

                    delete ctx;
                    backend->endTestGroup();
                    if (!backend->genNextTestGroup()) break;
                }
            }

            backend->endTestSet();
        }
    }

    //==========================================================================
private:

    void finalizeNegativeSample(TestGen& test, unsigned id, unsigned val)
    {
        assert(InstSetManager::isValidInst(test.getPositiveSample().getInst()));
        assert(!InstSetManager::isValidInst(test.getNegativeSample().getInst()));

        Sample negativeSample = test.getNegativeSample();

        if (startTest(negativeSample.getInst()))
        {
            if (isSinglePackage)
            {
                createNegativeTest(test, id, val);
            }
            else
            {
                Context* ctx = new Context(negativeSample, false);
                ctx->defineTestKernel();
                ctx->startKernelBody();
                Sample res = ctx->cloneSample(negativeSample, id, val);
                assert(!InstSetManager::isValidInst(res.getInst()));
                ctx->finishKernelBody();
                registerTest(ctx, res.getInst());
                delete ctx;
            }

            ++testIdx;
        }
    }

    //==========================================================================

    void createPositiveTest(TestGen& test, bool start)
    {
        assert(isSinglePackage);

        if (start)
        {
            string note;
            if (genBasic) {
                if (test.getFormat() == BRIG_KIND_INST_MOD)
                    note = " (InstMod format)";
                if (test.getFormat() == BRIG_KIND_INST_BASIC && test.isBasicVariant())
                    note = " (InstBasic format)";
            }

            context->emitComment();
            context->emitComment((isPositive? "Next sample" : "Next valid sample") + note);
            context->emitComment();

            context->cloneSample(test.getPositiveSample());

            context->emitComment();
        }
        else
        {
            context->cloneSample(test.getPositiveSample());
        }
    }

    void createNegativeTest(TestGen& test, unsigned id, unsigned val)
    {
        assert(isSinglePackage);

        Sample negativeSample = test.getNegativeSample();
        Sample invalid = context->cloneSample(negativeSample, id, val);
        assert(!InstSetManager::isValidInst(invalid.getInst()));
    }

    //==========================================================================
private:

    void registerTest(Context* ctx, Inst inst = Inst())
    {
        assert(ctx); //FF add inst validation: it must not contain null operands

        TestDesc testDesc;

        backend->registerTest(testDesc);
        testDesc.setContainer(&ctx->getContainer());
        testDesc.setInst(inst);

        testComplete(testDesc);
    }

    //==========================================================================
};

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_MANAGER_H
