//===-- HSAILTestGenContext.h - HSAIL Test Generator Context --------------===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef INCLUDED_HSAIL_TESTGEN_CONTEXT_H
#define INCLUDED_HSAIL_TESTGEN_CONTEXT_H

#include "HSAILTestGenProp.h"
#include "HSAILTestGenSample.h"
#include "HSAILTestGenBrigContext.h"
#include "HSAILTestGenUtilities.h"

#include "HSAILValidator.h"

#include "HSAILBrigContainer.h"
#include "HSAILBrigObjectFile.h"
#include "HSAILItems.h"
#include "Brig.h"

#include <cassert>
#include <sstream>

using std::ostringstream;
using HSAIL_ASM::isTermInst;
using HSAIL_ASM::ItemList;

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================
// Brig context used for test generation.
//
// Context includes the following components:
// - BRIG Container (see BrigContext)
// - Test kernel which includes instruction(s) being tested
// - Predefined set of symbols used for testing
// - Predefined set of operands used for testing
//
// There are two kinds of context: standard and playground.
//
// Standard context is used to create final BRIG files with tests.
// Instructions created in standard context must not have null operands.
//
// Playground context is used by TestGen engine internally and is never saved as BRIG.
// Test instructions created in this context have exactly 5 operands; unused operands are null.
// This convention simplifies TestGen algorithms (because there are instructions
// with variable number of operands).
//
class Context : public BrigContext
{
    friend class Sample;

private:
    DirectiveKernel testKernel;     // kernel for which test code is generated
    Operand operandTab[O_MAXID];    // operands used for testing. Created at first access
    Directive symTab[SYM_MAXID];    // Symbols used for testing
    bool playground;                // true for playground context (see description above)

    //==========================================================================

private:
    Context(const Context&); // non-copyable
    const Context &operator=(const Context &); // not assignable

    //==========================================================================

public:

    // Used to create context for a _set_ of test instructions (added separately).
    // This constructor is useful in two cases:
    // 1. To create a single file with many tests cases (option PACKAGE_SINGLE);
    // 2. To create special 'playground' context for generation of temporary samples (see description above).
    Context(bool isPlayground = false) : BrigContext(), playground(isPlayground)
    {
        emitVersion();
        if (gcnInstEnabled()) emitExtension("amd:gcn");
        if (imgInstEnabled()) emitExtension("IMAGE");
        genSymbols(); // Generate all symbols which can potentially be used
    }

    // Used to create context for tests which include just one _test_ instruction specified by sample
    Context(const Sample s, bool isPositive) : BrigContext(), playground(false)
    {
        emitVersion();

        // Generate required extensions based on instruction being tested
        if (HSAIL_ASM::isGcnInst(s.getOpcode()))
        {
            assert(gcnInstEnabled());
            emitExtension("amd:gcn");
        }
        else if (HSAIL_ASM::hasImageExtProps(s.getInst()))
        {
            // positive tests must not include image-specific props unless "-image" option is specified
            // negative tests may include image-specific types even if "-image" option is not specified
            assert(imgInstEnabled() || !isPositive);
            emitExtension("IMAGE");
        }

        // Generate only those symbols which are referred to by test instruction
        for (int i = 0; i < s.getInst().operands().size(); ++i)
        {
            unsigned propId = getSrcOperandId(i);
            genSymbol(operandId2SymId(s.get(propId)));
        }
    }

    //==========================================================================

public:

    void defineTestKernel()  { testKernel = emitSbrStart(Brig::BRIG_KIND_DIRECTIVE_KERNEL, "&Test"); }
    void startKernelBody()   { startSbrBody(); }

    void finishKernelBody()
    {
        assert(testKernel && testKernel.container() == &getContainer());

        emitSbrEnd();
    }

    bool isPlayground() { return playground; }

    //==========================================================================

public:
    // Used to create positive tests
    Sample cloneSample(const Sample s)
    {
        assert(!s.isEmpty());
        assert(s.isPlayground());
        assert(!isPlayground());

        Sample copy = createSample(s.getFormat(), s.getOpcode());
        copy.copyFrom(s); // Copy instruction fields and allocate array for non-null operands

        // Create operands in current context.
        // Cannot copy from 's'operands as they live in playground
        for (int i = 0; i < copy.getInst().operands().size(); ++i)
        {
            unsigned propId = getSrcOperandId(i);
            copy.set(propId, s.get(propId));
        }

        finalizeSample(copy);

        return copy;
    }

    // Used to create negative tests
    Sample cloneSample(const Sample s, unsigned id, unsigned val)
    {
        assert(!s.isEmpty());
        assert(s.isPlayground());
        assert(!isPlayground());

        emitComment();
        emitComment();
        ostringstream text;
        text << "Invalid value of " << prop2str(id) << " = " << val2str(id, val);
        emitComment(text.str());
        emitComment();

        return cloneSample(s);
    }

    Sample createSample(unsigned format, unsigned opcode)
    {
        Inst inst = appendInst(getContainer(), format);

        if (isPlayground())
        {
            ItemList list;
            for (int i = 0; i < 5; ++i) list.push_back(Operand());
            inst.operands() = list;
        }

        return Sample(this, inst, opcode);
    }

    //==========================================================================

private:

    void finalizeSample(Sample sample)
    {
        assert(!isPlayground());

        using namespace Brig;
        unsigned opcode = sample.getOpcode();

        if (isTermInst(opcode))
        {
            emitAuxLabel(); // Generate aux label to avoid "unreachable code" error
        }
    }

    //==========================================================================
    // Mapping of operand ids to brig operands and back
private:

    unsigned operand2id(Operand opr) const
    {
        if (!opr) return O_NULL;

        // NB: This code is inefficient and should be optimized.
        // But it amounts to about 1% of execution time. This is because 'operand2id' is
        // used only for accepted samples; generation and validation of all samples
        // takes up most of execution time.
        for (unsigned i = O_MINID + 1; i < O_MAXID; ++i)
        {
            if (i == O_NULL) continue;
            // NB: only used operands are created
            if (readOperand(i).brigOffset() == opr.brigOffset()) return i;
        }

        assert(false);
        return static_cast<unsigned>(-1);
    }

    Operand id2operand(unsigned oprId)
    {
        assert(O_MINID < oprId && oprId < O_MAXID);

        return getOperand(oprId);
    }

    //==========================================================================
    // Mapping of eqclass ids to values and back
private:

    unsigned eqclass2id(unsigned equiv) const
    {
        if (equiv == 0)   return EQCLASS_0;
        if (equiv == 1)   return EQCLASS_1;
        if (equiv == 2)   return EQCLASS_2;
        if (equiv == 255) return EQCLASS_255;

        assert(false);
        return static_cast<unsigned>(-1);
    }

    unsigned id2eqclass(unsigned eqClassId)
    {
        assert(EQCLASS_MINID < eqClassId && eqClassId < EQCLASS_MAXID);

        if (eqClassId == EQCLASS_0)   return 0;
        if (eqClassId == EQCLASS_1)   return 1;
        if (eqClassId == EQCLASS_2)   return 2;
        if (eqClassId == EQCLASS_255) return 255;

        assert(false);
        return 0xFF;
    }

    //==========================================================================

private:

    bool    isOperandCreated(unsigned oprId) const { assert(O_MINID < oprId && oprId < O_MAXID); return oprId == O_NULL || operandTab[oprId]; }
    Operand readOperand(unsigned oprId)      const { assert(O_MINID < oprId && oprId < O_MAXID); return operandTab[oprId]; }

    Operand getOperand(unsigned oprId);                 // create if not created yet

    void genSymbols();                                  // Create all symbols
    void genSymbol(unsigned symId);                     // Create only symbol required for operandId
    Operand emitOperandRef(unsigned symId);

    Directive emitSymbol(unsigned symId);

    //==========================================================================

private:

    void dump() //F
    {
        for(Code c = getContainer().code().begin();
            c != getContainer().code().end();
            c = c.next())
        {
            //
        }
        for(Operand o = getContainer().operands().begin();
            o != getContainer().operands().end();
            o = o.next())
        {
            //o.dump(std::cout);
        }
    }

};

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_CONTEXT_H