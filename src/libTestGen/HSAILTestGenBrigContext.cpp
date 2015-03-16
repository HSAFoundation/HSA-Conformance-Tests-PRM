#include "HSAILTestGenBrigContext.h"
#include "HSAILTestGenUtilities.h"

#include "HSAILBrigContainer.h"
#include "HSAILItems.h"
#include "Brig.h"

#include <string>
#include <sstream>
#include <iomanip>

using std::string;
using std::ostringstream;
using std::setw;
using std::setfill;

using HSAIL_ASM::Code;

using HSAIL_ASM::DirectiveModule;
using HSAIL_ASM::DirectiveKernel;
using HSAIL_ASM::DirectiveFunction;
using HSAIL_ASM::DirectiveExecutable;
using HSAIL_ASM::DirectiveVariable;
using HSAIL_ASM::DirectiveLabel;
using HSAIL_ASM::DirectiveComment;
using HSAIL_ASM::DirectiveExtension;
using HSAIL_ASM::DirectiveArgBlockStart;
using HSAIL_ASM::DirectiveArgBlockEnd;
using HSAIL_ASM::DirectiveFbarrier;

using HSAIL_ASM::InstBasic;
using HSAIL_ASM::InstAtomic;
using HSAIL_ASM::InstCmp;
using HSAIL_ASM::InstCvt;
using HSAIL_ASM::InstImage;
using HSAIL_ASM::InstMem;
using HSAIL_ASM::InstMod;
using HSAIL_ASM::InstBr;
using HSAIL_ASM::InstAddr;

using HSAIL_ASM::OperandRegister;
using HSAIL_ASM::OperandOperandList;
using HSAIL_ASM::OperandConstantBytes;
using HSAIL_ASM::OperandWavesize;
using HSAIL_ASM::OperandAddress;
using HSAIL_ASM::OperandCodeRef;
using HSAIL_ASM::OperandCodeList;

using HSAIL_ASM::ItemList;

using HSAIL_ASM::isSignedType;
using HSAIL_ASM::isFloatType;
using HSAIL_ASM::isOpaqueType;
using HSAIL_ASM::ArbitraryData;
using HSAIL_ASM::getBrigTypeNumBits;
using HSAIL_ASM::type2immType;
using HSAIL_ASM::type2str;
using HSAIL_ASM::isSignalType;

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================
// Default settings

unsigned BrigSettings::brigModel    = BRIG_MACHINE_UNDEF;
unsigned BrigSettings::brigProfile  = BRIG_PROFILE_UNDEF;
bool     BrigSettings::brigComments = true;
bool     BrigSettings::stdSubset    = true;
bool     BrigSettings::imgSubset    = false;
bool     BrigSettings::gcnSubset    = false;

//=============================================================================
//=============================================================================
//=============================================================================

void BrigContext::emitModule()
{
    //F1.0: name + rounding + pass by option
    brigantine.module("&module", BRIG_VERSION_HSAIL_MAJOR, BRIG_VERSION_HSAIL_MINOR, getModel(), getProfile(), BRIG_ROUND_FLOAT_DEFAULT);
}

void BrigContext::emitExtension(const char* name)
{
    DirectiveExtension ext = brigantine.addExtension(name);
}

string BrigContext::getLabName(const char* name, unsigned idx, unsigned width /*=0*/)
{
    ostringstream labName;

    labName << name;
    if (width > 0) labName << setfill('0') << setw(width);
    labName << idx;
    return labName.str();
}

Operand BrigContext::emitLabelRef(const char* name, unsigned idx, unsigned width /*=0*/)
{
    return emitLabelRef(getLabName(name, idx, width).c_str());
}

Operand BrigContext::emitLabelRef(const char* name)
{
    return brigantine.createLabelRef(SRef(name));
}

Operand BrigContext::emitLabelAndRef(const char* name)
{
    DirectiveLabel lbl = emitLabel(name);
    return emitLabelRef(name);
}

DirectiveLabel BrigContext::emitAuxLabel() 
{ 
    return emitLabel("@aux_label_", labCount++); 
}

DirectiveLabel BrigContext::emitLabel(const char* name, unsigned idx, unsigned width /*=0*/)
{
    return emitLabel(getLabName(name, idx, width).c_str());
}

DirectiveLabel BrigContext::emitLabel(const char* name)
{
    return brigantine.addLabel(SRef(name));
}

void BrigContext::emitComment(string s /*=""*/)
{
    if (commentsEnabled())
    {
        s = "// " + s;
        brigantine.addComment(s.c_str());
    }
}

//=============================================================================
//=============================================================================
//=============================================================================

void BrigContext::emitRet()
{
    Inst inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_RET, BRIG_TYPE_NONE);
    append(inst, Operand());
}

void BrigContext::emitSt(unsigned type, unsigned segment, Operand from, Operand to, unsigned align /*=BRIG_ALIGNMENT_1*/)
{
    InstMem inst = brigantine.addInst<InstMem>(BRIG_OPCODE_ST, conv2LdStType(type));

    inst.segment()    = segment;
    inst.align()      = align;
    inst.width()      = BRIG_WIDTH_NONE;
    inst.equivClass() = 0;
    inst.modifier().isConst() = false;

    append(inst, from, to);
}

void BrigContext::emitLd(unsigned type, unsigned segment, Operand to, Operand from, unsigned align /*=BRIG_ALIGNMENT_1*/)
{
    InstMem inst = brigantine.addInst<InstMem>(BRIG_OPCODE_LD, conv2LdStType(type));

    inst.segment()    = segment;
    inst.width()      = BRIG_WIDTH_1;
    inst.equivClass() = 0;
    inst.align()      = align;
    inst.modifier().isConst() = false;

    append(inst, to, from);
}

void BrigContext::emitShl(unsigned type, Operand res, Operand src, unsigned shift)
{
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_SHL, type);

    append(inst, res, src, emitImm(BRIG_TYPE_U32, shift));
}

void BrigContext::emitShr(unsigned type, Operand res, Operand src, unsigned shift)
{
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_SHR, type);

    append(inst, res, src, emitImm(BRIG_TYPE_U32, shift));
}

void BrigContext::emitMul(unsigned type, Operand res, Operand src, unsigned multiplier)
{
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_MUL, type);

    append(inst, res, src, emitImm(type, multiplier));
}

void BrigContext::emitMov(unsigned type, Operand to, Operand from)
{
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_MOV, type);

    append(inst, to, from);
}

void BrigContext::emitAdd(unsigned type, Operand res, Operand op1, Operand op2)
{
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_ADD, type);

    append(inst, res, op1, op2);
}

void BrigContext::emitAdd(unsigned type, Operand res, Operand op1, unsigned n)
{
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_ADD, type);

    append(inst, res, op1, emitImm(type, n));
}

void BrigContext::emitSub(unsigned type, Operand res, Operand op1, Operand op2)
{
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_SUB, type);

    append(inst, res, op1, op2);
}

void BrigContext::emitGetWorkItemId(Operand res, unsigned dim)
{
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_WORKITEMABSID, BRIG_TYPE_U32);

    append(inst, res, emitImm(BRIG_TYPE_U32, dim));
}

void BrigContext::emitCvt(unsigned dstType, unsigned srcType, OperandRegister to, OperandRegister from)
{
    InstCvt cvt = brigantine.addInst<InstCvt>(BRIG_OPCODE_CVT, dstType);
    cvt.sourceType() = srcType;

    append(cvt, to, from);
}

void BrigContext::emitLda(OperandRegister dst, DirectiveVariable var)
{
    assert(dst);
    assert(var);

    InstAddr lda = brigantine.addInst<InstAddr>(BRIG_OPCODE_LDA, getSegAddrType(var.segment()));

    lda.segment()   = var.segment();

    append(lda, dst, emitAddrRef(var));
}

void BrigContext::emitCmpEq(unsigned cRegIdx, unsigned sRegIdx, unsigned immVal)
{
    InstCmp cmp = brigantine.addInst<InstCmp>(BRIG_OPCODE_CMP, BRIG_TYPE_B1);

    cmp.sourceType()        = BRIG_TYPE_U32;
    cmp.compare()           = BRIG_COMPARE_EQ;
    cmp.modifier().ftz()    = 0;
    cmp.pack()              = BRIG_PACK_NONE;

    append(cmp, emitReg(1, cRegIdx), emitReg(32, sRegIdx), emitImm(BRIG_TYPE_U32, immVal));
}

void BrigContext::emitCbr(unsigned cRegIdx, Operand label)
{
    InstBr cbr = brigantine.addInst<InstBr>(BRIG_OPCODE_CBR, BRIG_TYPE_B1);

    cbr.width() = BRIG_WIDTH_1;

    append(cbr, emitReg(1, cRegIdx), label);
}

void BrigContext::emitBr(Operand label)
{
    InstBr br = brigantine.addInst<InstBr>(BRIG_OPCODE_BR, BRIG_TYPE_NONE);

    br.width() = BRIG_WIDTH_ALL;

    append(br, label);
}

//=============================================================================
//=============================================================================
//=============================================================================

string BrigContext::getRegName(unsigned size, unsigned idx)
{
    ostringstream name;

    switch(size)
    {
    case 1:      name << "$c";  break;
    case 32:     name << "$s";  break;
    case 64:     name << "$d";  break;
    case 128:    name << "$q";  break;
    default:
        assert(false);
        name << "ERR";
        break;
    }
    name << idx;

    return name.str();
}

Operand BrigContext::emitReg(OperandRegister reg)
{
    return brigantine.createOperandReg(HSAIL_ASM::getRegName(reg));
}

Operand BrigContext::emitReg(unsigned size, unsigned idx)
{
    return brigantine.createOperandReg(getRegName(size, idx));
}

Operand BrigContext::emitVector(unsigned cnt, unsigned type, unsigned idx0)
{
    assert(2 <= cnt && cnt <= 4);
    assert(type2str(type));

    ItemList opnds;
    unsigned size = getBrigTypeNumBits(type);
    for(unsigned i = 0; i < cnt; ++i) opnds.push_back(emitReg(size, idx0 + i));

    return brigantine.createOperandList(opnds);
}

Operand BrigContext::emitVector(unsigned cnt, unsigned type, bool isDst /*=true*/, unsigned immCnt /*=0*/)
{
    assert(2 <= cnt && cnt <= 4);
    assert(immCnt == 0 || !isDst);
    assert(immCnt <= cnt);
    assert(type2str(type));

    unsigned size = getBrigTypeNumBits(type);
    bool isSignal = isSignalType(type);

    assert(size == 8 || size == 16 || size == 32 || size == 64 || size == 128);

    unsigned rsize = (size <= 32)? 32 : size;
    unsigned wsCnt = (immCnt == cnt && rsize != 128 && !isSignal)? 1 : 0;

    unsigned i = 0;
    ItemList opnds;
    for(; i <  wsCnt; ++i) opnds.push_back(emitWavesize());
    for(; i < immCnt; ++i) opnds.push_back(emitImm(type, (i == 0 || isSignal)? 0 : -1));
    for(; i < cnt;    ++i) opnds.push_back(emitReg(rsize, isDst? i : 0));

    return brigantine.createOperandList(opnds);
}

Operand BrigContext::emitWavesize()
{
    return brigantine.createWaveSz();
}

Operand BrigContext::emitImm(unsigned type, uint64_t lVal /*=0*/, uint64_t hVal /*=0*/)
{
    assert(type2str(type));

    ArbitraryData data;

    switch(getBrigTypeNumBits(type))
    {
    case 1:      data.write((uint8_t)(lVal? 1 : 0), 0);        break;
    case 8:      data.write((uint8_t)lVal,          0);        break;
    case 16:     data.write((uint16_t)lVal,         0);        break;
    case 32:     data.write((uint32_t)lVal,         0);        break;
    case 64:     data.write((uint64_t)lVal,         0);        break;
    case 128:    data.write((uint64_t)lVal,         0);
                 data.write((uint64_t)hVal, sizeof(uint64_t)); break;
    default:
        assert(false);
    }

    unsigned constType = type2immType(type, false);
    assert(constType != BRIG_TYPE_NONE);

    return brigantine.createImmed(data.toSRef(), constType);
}

Operand BrigContext::emitOperandCodeRef(Code c)
{
    return brigantine.createCodeRef(c);
}

Operand BrigContext::emitAddrRef(DirectiveVariable var, OperandRegister reg, unsigned offset /*=0*/)
{
    assert(var || reg);

    bool is32BitAddr = (var && getSegAddrSize(var.segment()) == 32) || (reg && reg.regKind() == BRIG_REGISTER_KIND_SINGLE);

    return brigantine.createRef(var? SRef(var.name()) : SRef(), reg, offset, is32BitAddr);
}

Operand BrigContext::emitAddrRef(DirectiveVariable var, uint64_t offset /*=0*/)
{
    assert(var);

    bool is32BitAddr = (var && getSegAddrSize(var.segment()) == 32);

    return brigantine.createRef(var? SRef(var.name()) : SRef(), OperandRegister(), offset, is32BitAddr);
}

Operand BrigContext::emitAddrRef(OperandRegister reg, uint64_t offset)
{
    assert(reg);

    bool is32BitAddr = (reg && reg.regKind() == BRIG_REGISTER_KIND_SINGLE);

    return brigantine.createRef(SRef(), reg, offset, is32BitAddr);
}

Operand BrigContext::emitAddrRef(uint64_t offset, bool is32BitAddr)
{
    return brigantine.createRef(SRef(), OperandRegister(), offset, is32BitAddr);
}

//=============================================================================
//=============================================================================
//=============================================================================

DirectiveVariable BrigContext::emitSbrArg(unsigned type, string name, bool isInputArg /*=true*/)
{
    assert(currentSbr);
    assert(!DirectiveKernel(currentSbr) || isInputArg);

    unsigned segment = DirectiveKernel(currentSbr)? BRIG_SEGMENT_KERNARG : BRIG_SEGMENT_ARG;
    DirectiveVariable arg = emitSymbol(type, name, segment);
    if (isInputArg) brigantine.addInputParameter(arg); else brigantine.addOutputParameter(arg);
    return arg;
}

DirectiveExecutable BrigContext::emitSbrStart(unsigned kind, const char* name)
{
    assert(!currentSbr);

    switch (kind)
    {
    case BRIG_KIND_DIRECTIVE_FUNCTION:            currentSbr = brigantine.declFunc(SRef(name));         break;
    case BRIG_KIND_DIRECTIVE_INDIRECT_FUNCTION:   currentSbr = brigantine.declIndirectFunc(SRef(name)); break;
    case BRIG_KIND_DIRECTIVE_KERNEL:              currentSbr = brigantine.declKernel(SRef(name));       break;
    case BRIG_KIND_DIRECTIVE_SIGNATURE:           currentSbr = brigantine.declSignature(SRef(name));    break;
    default: 
        assert(false); 
        return DirectiveExecutable();
    }

    currentSbr.linkage() = DirectiveSignature(currentSbr)? BRIG_LINKAGE_NONE : BRIG_LINKAGE_MODULE;

    return currentSbr;
}

void BrigContext::startSbrBody()
{
    assert(currentSbr);
    assert(!DirectiveSignature(currentSbr));

    brigantine.startBody();
}

void BrigContext::emitSbrEnd()
{
    assert(currentSbr);

    if (!DirectiveSignature(currentSbr))
    {
        // This footer is necessary to avoid hanging labels
        // which refer past the end of the code section
        emitAuxLabel();
        emitRet();

        bool ok = brigantine.endBody();
        assert(ok);
    }

    currentSbr = DirectiveExecutable(); // clear
}

unsigned BrigContext::conv2LdStType(unsigned type) // Convert to type supported by ld/st
{
    if (isSignedType(type) || isFloatType(type) || isOpaqueType(type)) return type;

    switch(getBrigTypeNumBits(type))
    {
    case 8:      return BRIG_TYPE_U8;
    case 16:     return BRIG_TYPE_U16;
    case 32:     return BRIG_TYPE_U32;
    case 64:     return BRIG_TYPE_U64;
    case 128:    return BRIG_TYPE_B128;

    default:
        assert(false);
        return BRIG_TYPE_NONE;
    }
}

//=============================================================================
//=============================================================================
//=============================================================================

} // namespace TESTGEN
