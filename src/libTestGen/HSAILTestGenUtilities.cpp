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

#include "HSAILTestGenUtilities.h"

#include <string>
#include <sstream>
#include <iomanip>

using std::string;
using std::ostringstream;
using std::setfill;
using std::setw;

using HSAIL_ASM::ItemList;

namespace TESTGEN {

// ============================================================================
// ============================================================================
//============================================================================

void assign(Inst inst, int idx, Operand opr)
{
    assert(0 <= idx && idx < MAX_OPERANDS_NUM);
    assert(idx < inst.operands().size());
    inst.operands().writeAccess(idx) = opr;
}

void append(Inst inst, Operand opr0, Operand opr1 /*=Operand()*/, Operand opr2 /*=Operand()*/)
{
    assert(inst);
    assert(!inst.operands() || inst.operands().size() == 0);

    ItemList list;

    if (opr0) list.push_back(opr0);
    if (opr1) list.push_back(opr1);
    if (opr2) list.push_back(opr2);

    inst.operands() = list;
}

string index2str(unsigned idx, unsigned width /*=0*/)
{
    ostringstream s;
    if (width > 0) {
        s << setfill('0');
        s << setw(width);
    }
    s << idx;
    return s.str();
}

//=============================================================================
//=============================================================================
//=============================================================================

} // namespace TESTGEN

