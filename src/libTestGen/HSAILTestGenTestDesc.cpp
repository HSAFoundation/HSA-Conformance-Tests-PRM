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

#include "HSAILTestGenTestDesc.h"
#include "HSAILTestGenBrigContext.h"
#include "HSAILDisassembler.h"

#include <string>
#include <sstream>
#include <iomanip>

using std::string;
using std::ostringstream;
using std::setfill;
using std::setw;

namespace TESTGEN {

// ============================================================================
// ============================================================================
// ============================================================================

string dumpInst(Inst inst) 
{ 
    return HSAIL_ASM::Disassembler::getInstMnemonic(inst, BrigSettings::getModel(), BrigSettings::getProfile()); 
}

string getOperandKind(Inst inst, unsigned operandIdx)
{
    assert(operandIdx < getOperandsNum(inst));

    Operand operand = inst.operand(operandIdx);
    return HSAIL_ASM::OperandConstantBytes(operand) ? "imm" :
           HSAIL_ASM::OperandRegister(operand)      ? "reg" :
           HSAIL_ASM::OperandOperandList(operand)   ? "vec" :
           HSAIL_ASM::OperandAddress(operand)       ? "mem" :
           HSAIL_ASM::OperandWavesize(operand)      ? "wsz" :
                                                      "???" ;
}


//=============================================================================
//=============================================================================
//=============================================================================

} // namespace TESTGEN

