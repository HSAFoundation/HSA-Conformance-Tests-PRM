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

#include "HSAILTestGenSample.h"
#include "HSAILTestGenContext.h"
#include "HSAILTestGenUtilities.h"

using HSAIL_ASM::ItemList;
using HSAIL_PROPS::getOperandIdx;

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================

unsigned Sample::get(unsigned propId) const
{
    assert(PROP_MINID < propId && propId < PROP_MAXID);
    assert(!isEmpty());

    if (isOperandProp(propId))
    {
        int idx = getOperandIdx(propId);
        assert(0 <= idx && idx < MAX_OPERANDS_NUM);
        assert(idx < inst.operands().size());
        return getContext()->operand2id(inst.operand(idx));
    }
    else
    {
        unsigned val = getBrigProp(inst, propId);
        return (propId == PROP_EQUIVCLASS)? getContext()->eqclass2id(val) : val;
    }
}

void Sample::set(unsigned propId, unsigned val)
{
    assert(PROP_MINID < propId && propId < PROP_MAXID);
    assert(!isEmpty());

    if (isOperandProp(propId))
    {
        int idx = getOperandIdx(propId);
        assert(0 <= idx && idx < MAX_OPERANDS_NUM);
        assert(idx < inst.operands().size());

        assign(inst, idx, getContext()->id2operand(val));
    }
    else
    {
        if (propId == PROP_EQUIVCLASS) val = getContext()->id2eqclass(val);
        setBrigProp(inst, propId, val);
    }
}

void Sample::copyFrom(const Sample s)
{
    assert(!s.isEmpty());
    assert(inst.kind() == s.inst.kind());
    assert(s.isPlayground());

    memcpy(inst.brig(), s.inst.brig(), s.inst.byteCount());

    ItemList list;
    for (int i = 0; i < s.inst.operands().size(); ++i)
    {
        // When creating final test instructions, get rid of unused operands
        // Playground test instructions must have exactly MAX_OPERANDS_NUM operands
        if (!s.inst.operand(i) && !isPlayground()) break;
        list.push_back(Operand());
    }
    inst.operands() = list;
}

bool Sample::isPlayground() const
{
    assert(ctx);
    return ctx->isPlayground();
}

//==============================================================================
//==============================================================================
//==============================================================================

} // namespace TESTGEN
