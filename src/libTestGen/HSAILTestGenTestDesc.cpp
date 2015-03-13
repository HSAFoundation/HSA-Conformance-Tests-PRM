//===-- HSAILTestGenUtilities.cpp - HSAIL Test Generator Utilities ------------===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

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

