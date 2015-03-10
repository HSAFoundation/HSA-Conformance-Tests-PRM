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
    HSAIL_ASM::Disassembler disasm(*inst.container());
    string res = disasm.get(inst, BrigSettings::getModel(), BrigSettings::getProfile());
    string::size_type pos = res.find_first_of("\t");
    if (pos != string::npos) res = res.substr(0, pos);
    return res;
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

