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

#ifndef INCLUDED_HSAIL_TESTGEN_SAMPLE_H
#define INCLUDED_HSAIL_TESTGEN_SAMPLE_H

#include "HSAILBrigContainer.h"
#include "HSAILItems.h"
#include "Brig.h"

#include "HSAILInstProps.h"

#include <cassert>

using HSAIL_ASM::Inst;

using namespace HSAIL_PROPS;

namespace TESTGEN {

class Context;

//==============================================================================
//==============================================================================
//==============================================================================
// Test sample which comprise of test instruction and a context
// (required operands and symbols)

class Sample
{
private:
    mutable Inst inst; // mutable because Inst provides no const interface
    Context* ctx;

    //==========================================================================
public:
    Sample() : ctx(0)  { }
    Sample(Context* c, Inst buf, unsigned opcode) : inst(buf), ctx(c) { setOpcode(opcode); }

    Sample(const Sample& s)
    {
        if (!s.isEmpty())
        {
            ctx  = s.ctx;
            inst = s.inst;
        }
        else
        {
            ctx = 0;
        }
    }

    const Sample &operator=(const Sample &s)
    {
        if (this == &s)
        {
            return *this;
        }
        else if (!s.isEmpty())
        {
            ctx  = s.ctx;
            inst = s.inst;
        }
        else
        {
            ctx = 0;
            inst.reset();
        }
        return *this;
    }

    //==========================================================================

public:

    unsigned get(unsigned propId) const;
    void     set(unsigned propId, unsigned val);

    bool     isEmpty()         const { return !inst; }
    unsigned getFormat()       const { assert(!isEmpty()); return inst.kind(); }
    unsigned getOpcode()       const { assert(!isEmpty()); return get(PROP_OPCODE); }
    Inst     getInst()         const { assert(!isEmpty()); return inst; }
    Context* getContext()      const { assert(!isEmpty()); return ctx; }
    void     setOpcode(unsigned opc) { assert(!isEmpty()); set(PROP_OPCODE, opc); }

    void     copyFrom(const Sample s);
    bool     isPlayground() const;

    //==========================================================================
};

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_SAMPLE_H
