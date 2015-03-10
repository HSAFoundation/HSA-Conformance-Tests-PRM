/*
   Copyright 2014 Heterogeneous System Architecture (HSA) Foundation

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

#include "BrigEmitter.hpp"
#include "Scenario.hpp"
#include "CoreConfig.hpp"
#include "Emitter.hpp"
#include <algorithm>
#include <sstream>

using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;

namespace hexl {

namespace emitter {

const Operand BrigEmitter::nullOperand;

brig_container_t BrigEmitter::Brig()
{
  brig_container_t res = brig;
  brig = 0;
  return res;
}

BrigContainer* BrigEmitter::BrigC() {
  // TODO: fix hsail_c API not to do cast.
  return (BrigContainer*) brig;
/*
  return brig_container_create_copy(Brig()->strings().data().begin,
    Brig()->code().data().begin,
    Brig()->operands().data().begin,
    0);
*/
}

BrigTypeX EPointerReg::GetSegmentPointerType(BrigSegment8_t segment, bool large)
{
  switch (getSegAddrSize(segment, large)) {
  case 32:
    return BRIG_TYPE_U32;
  case 64:
    return BRIG_TYPE_U64;
  default:
    assert(false); return BRIG_TYPE_NONE;
  }
}


BrigEmitter::BrigEmitter()
  : ap(new Arena()),
    brig(brig_container_create_empty()),
    coreConfig(0),
    brigantine(*BrigC()),
    currentScope(ES_MODULE)
{
  workitemabsid[0] = 0;
  workitemabsid[1] = 0;
  workitemflatabsid[0] = 0;
  workitemflatabsid[1] = 0;
}

BrigEmitter::~BrigEmitter()
{
  if (brig) { brig_container_destroy(brig); }
  delete ap;
}

std::string BrigEmitter::AddName(const std::string& name, bool addZero)
{
  auto i = nameIndexes.find(name);
  unsigned index = (i == nameIndexes.end()) ? 0 : i->second;
  std::ostringstream ss;
  ss << name;
  if (index || addZero) { ss << index; }
  nameIndexes[name] = index + 1;
  return ss.str();
}

OperandRegister BrigEmitter::Reg(const std::string& name)
{
  return brigantine.createOperandReg(name);
}

OperandRegister BrigEmitter::AddReg(const std::string& name)
{
  return Reg(AddName(name, true));  
}

OperandRegister BrigEmitter::AddReg(BrigType16_t type)
{
  switch (getBrigTypeNumBits(type)) {
  case 1: return AddCReg();
  case 8:
  case 16:
  case 32: return AddSReg();
  case 64: return AddDReg();
  case 128: return AddQReg();
  default: assert(false); return OperandRegister();
  }
}

OperandOperandList BrigEmitter::AddVec(BrigType16_t type, unsigned count)
{
  assert(count <= 4);
  ItemList list;
  for (unsigned i = 0; i < count; ++i) {
    list.push_back(AddReg(type));
  }
  return brigantine.createOperandList(list);
}

TypedReg BrigEmitter::AddCTReg()
{
  return new(ap) ETypedReg(AddCReg(), BRIG_TYPE_B1);
}

PointerReg BrigEmitter::AddAReg(BrigSegment8_t segment)
{
  BrigType16_t type;
  OperandRegister reg;
  switch (getSegAddrSize(segment, coreConfig->IsLarge())) {
  case 32: type = BRIG_TYPE_U32; reg = AddSReg(); break;
  case 64: type = BRIG_TYPE_U64; reg = AddDReg(); break;
  default: assert(false); type = BRIG_TYPE_NONE;
  }
  return new(ap) EPointerReg(reg, type, segment);
}

TypedReg BrigEmitter::AddTReg(BrigType16_t type, unsigned count)
{
  count = std::max(count, (unsigned) 1);
  assert(count <= 16);
  TypedReg regs = new(ap) ETypedReg(type);
  for (unsigned i = 0; i < count; ++i) {
    regs->Regs().push_back(AddReg(type));
  }
  return regs;
}

TypedRegList BrigEmitter::AddTRegList()
{
  return new(ap) ETypedRegList();
}

std::string BrigEmitter::GenVariableName(BrigSegment segment, bool output)
{
  switch (segment) {
  case BRIG_SEGMENT_ARG:
  case BRIG_SEGMENT_KERNARG:
    if (output) {
      return OName();
    } else {
      return IName();
    }
  default:
    return TName();
  }
}

BrigTypeX BrigEmitter::PointerType(BrigSegment8_t asegment) const
{
  switch (getSegAddrSize(asegment, coreConfig->IsLarge())) {
  case 32: return BRIG_TYPE_U32;
  case 64: return BRIG_TYPE_U64;
  default: assert(false); return BRIG_TYPE_INVALID;
  }
}

void BrigEmitter::Start()
{
  assert(coreConfig);
  brigantine.startProgram();
  brigantine.module("&sample", coreConfig->MajorVersion(), coreConfig->MinorVersion(), coreConfig->Model(), coreConfig->Profile(), Brig::BRIG_ROUND_FLOAT_NEAR_EVEN);
}

void BrigEmitter::End()
{
  //Brig()->optimizeOperands();
  brigantine.endProgram();
}

DirectiveKernel BrigEmitter::StartKernel(const std::string& name)
{
  std::string kernelName = AddName(name);
  currentExecutable = brigantine.declKernel(kernelName);
  currentExecutable.linkage() = BRIG_LINKAGE_PROGRAM;
  currentScope = ES_FUNCARG;
  return currentExecutable;
}

void BrigEmitter::EndKernel()
{
  brigantine.endBody();
  currentScope = ES_MODULE;
}

DirectiveFunction BrigEmitter::StartFunction(const std::string& id)
{
  std::string funcName = AddName(id);
  currentExecutable = brigantine.declFunc(funcName);
  currentExecutable.linkage() = BRIG_LINKAGE_PROGRAM;
  currentScope = ES_FUNCARG;
  return currentExecutable;
}

void BrigEmitter::EndFunction()
{
  brigantine.endBody();
  currentScope = ES_MODULE;
}

void BrigEmitter::StartBody()
{
  brigantine.startBody();
  currentScope = ES_LOCAL;
  ResetRegs();
}

void BrigEmitter::EndBody()
{
  brigantine.endBody();
}

void BrigEmitter::StartArgScope()
{
  currentScope = ES_ARG;
  brigantine.startArgScope();
}

void BrigEmitter::EndArgScope()
{
  brigantine.endArgScope();
  currentScope = ES_LOCAL;
}

ItemList BrigEmitter::Operands(Operand o1, Operand o2, Operand o3, Operand o4, Operand o5) {
  ItemList operands;
  if (o1) { operands.push_back(o1); }
  if (o2) { operands.push_back(o2); }
  if (o3) { operands.push_back(o3); }
  if (o4) { operands.push_back(o4); }
  if (o5) { operands.push_back(o5); }
  return operands;
}

hexl::Value BrigEmitter::GenerateTestValue(BrigTypeX type, uint64_t id) const
{
  return Value(Brig2ValueType(type), U64(42));
}

Operand BrigEmitter::Immed(BrigType16_t type, uint64_t imm)
{
  return brigantine.createImmed(imm, type);
}

Operand BrigEmitter::Immed(BrigType16_t type, SRef data) {
  return brigantine.createImmed(data, type);
}

Operand BrigEmitter::ImmedString(const std::string& str) 
{
  return brigantine.createOperandString(str);
}

Operand BrigEmitter::Wavesize()
{
  return brigantine.createWaveSz();
}

InstBasic BrigEmitter::EmitMov(Operand dst, Operand src, unsigned sizeBits)
{
  BrigType16_t movType;
  switch (sizeBits) {
  case 1: movType = BRIG_TYPE_B1; break;  
  case 8:
  case 16:
  case 32: movType = BRIG_TYPE_B32; break;
  case 64: movType = BRIG_TYPE_B64; break;
  case 128: movType = BRIG_TYPE_B128; break;
  default: assert(false); movType = BRIG_TYPE_NONE; break;
  }
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_MOV, movType);
  inst.operands() = Operands(dst, src);
  return inst;
}

void BrigEmitter::EmitMov(TypedReg dst, TypedReg src)
{
  assert(dst->Type() == src->Type());
  assert(dst->Count() == src->Count());
  for (unsigned i = 0; i < dst->Count(); ++i) {
    EmitMov(dst->Reg(i), src->Reg(i), dst->TypeSizeBits());
  }
}

void BrigEmitter::EmitMov(TypedReg dst, Operand src)
{
  for (unsigned i = 0; i < dst->Count(); ++i) {
    EmitMov(dst->Reg(i), src, dst->TypeSizeBits());
  }
}

TypedReg BrigEmitter::AddInitialTReg(BrigType16_t type, uint64_t initialValue, unsigned count) 
{
  TypedReg reg = AddTReg(type, count);
  for (uint32_t i = 0; i < count; ++i) {
    EmitMov(reg->Reg(i), Immed(type, initialValue), reg->TypeSizeBits());
  }
  return reg;
}

OperandAddress BrigEmitter::IncrementAddress(OperandAddress addr, int64_t offset)
{
  if (offset == 0) {
    return addr;
  } else {
    SRef name;
    if (addr.symbol()) { name = addr.symbol().name(); }
    return brigantine.createRef(name, addr.reg(), addr.offset() + offset);
  }
}

InstMem BrigEmitter::EmitLoad(BrigSegment8_t segment, BrigType16_t type, Operand dst, OperandAddress addr, uint8_t equiv)
{
  InstMem mem = brigantine.addInst<InstMem>(BRIG_OPCODE_LD, type);
  mem.segment() = segment;
  mem.align() = getNaturalAlignment(type);
  mem.width() = BRIG_WIDTH_1;
  mem.equivClass() = equiv;
  mem.operands() = Operands(dst, addr) ;
  return mem;
}

void BrigEmitter::EmitLoad(BrigSegment8_t segment, TypedReg dst, OperandAddress addr, bool useVectorInstructions, uint8_t equiv)
{
  BrigType16_t type = MemOpType(dst->Type());
  if (useVectorInstructions && dst->Count() > 1) {
    uint64_t dim = dst->Count();
    unsigned i = 0;
    uint64_t offset = 0;
    while (dim > 0) {
      uint64_t dim1 = std::min((uint64_t) 4, dim);
      InstMem mem = brigantine.addInst<InstMem>(BRIG_OPCODE_LD, type);
      mem.segment() = segment;
      mem.align() = getNaturalAlignment(type);
      mem.width() = BRIG_WIDTH_1;
      mem.equivClass() = equiv;
      ItemList dsts;
      for (size_t j = i; j < i + dim1; ++j) {
        dsts.push_back(dst->Reg(j));
      }
      OperandAddress addr1 = IncrementAddress(addr, i * getBrigTypeNumBits(type)/8);
      mem.operands() = Operands(brigantine.createOperandList(dsts), addr1) ;
      i += (unsigned) dim1;
      dim -= dim1;
      offset += dim1 * getBrigTypeNumBytes(type);
    }
  } else {
    for (size_t i = 0; i < dst->Count(); ++i) {
      EmitLoad(segment, type, dst->Reg(i), IncrementAddress(addr, i * getBrigTypeNumBits(type)/8), equiv);
    }
  }
}

BrigType16_t BrigEmitter::MemOpType(BrigType16_t type)
{
  if (getBrigTypeNumBits(type) == 128) {
    return BRIG_TYPE_B128;
  }
  switch (type) {
  case BRIG_TYPE_B16: return BRIG_TYPE_U16;
  case BRIG_TYPE_B32: return BRIG_TYPE_U32;
  case BRIG_TYPE_B64: return BRIG_TYPE_U64;
  case BRIG_TYPE_B128:
  case BRIG_TYPE_B1: assert(false);
  default:
    return type; 
  }
}


void BrigEmitter::EmitLoad(TypedReg dst, PointerReg addr, int64_t offset, bool useVectorInstructions, uint8_t equiv)
{
  EmitLoad(addr->Segment(), dst, Address(addr, offset), useVectorInstructions, equiv);
}


void BrigEmitter::EmitLoads(TypedRegList dsts, ItemList vars, bool useVectorInstructions)
{
  assert(dsts->Count() == vars.size());
  for (unsigned i = 0; i < dsts->Count(); ++i) {
    DirectiveVariable var = vars[i];
    EmitLoad(var.segment(), dsts->Get(i), Address(var), useVectorInstructions);
  }
}


InstMem BrigEmitter::EmitStore(BrigSegment8_t segment, BrigType16_t type, Operand src, OperandAddress addr, uint8_t equiv)
{
  InstMem mem = brigantine.addInst<InstMem>(BRIG_OPCODE_ST, type);
  mem.segment() = segment;
  mem.align() = getNaturalAlignment(type);
  mem.width() = BRIG_WIDTH_NONE;
  mem.equivClass() = equiv;
  mem.operands() = Operands(src, addr);
  return mem;
}

void BrigEmitter::EmitStore(BrigSegment8_t segment, TypedReg src, OperandAddress addr, bool useVectorInstructions, uint8_t equiv)
{
  BrigType16_t type = MemOpType(src->Type());
  if (useVectorInstructions && src->Count() > 1) {
    uint64_t dim = src->Count();
    unsigned i = 0;
    uint64_t offset = 0;
    while (dim > 0) {
      uint64_t dim1 = std::min((uint64_t) 4, dim);
      InstMem mem = brigantine.addInst<InstMem>(BRIG_OPCODE_ST, type);
      mem.segment() = segment;
      mem.align() = getNaturalAlignment(type);
      mem.width() = BRIG_WIDTH_NONE;
      mem.equivClass() = equiv;
      ItemList dsts;
      for (size_t j = i; j < i + dim1; ++j) {
        dsts.push_back(src->Reg(j));
      }
      OperandAddress addr1 = IncrementAddress(addr, i * getBrigTypeNumBits(type)/8);
      mem.operands() = Operands(brigantine.createOperandList(dsts), addr1) ;
      i += (unsigned) dim1;
      dim -= dim1;
      offset += dim1 * getBrigTypeNumBytes(type);
    }
  } else {
    for (size_t i = 0; i < src->Count(); ++i) {
      EmitStore(segment, type, src->Reg(i), IncrementAddress(addr, i * getBrigTypeNumBits(type)/8), equiv);
    }
  }
}

/*
void BrigEmitter::EmitStore(TypedReg src, DirectiveVariable v, int64_t offset, bool useVectorInstructions)
{
  EmitStore(v.segment(), src, Address(v, offset), useVectorInstructions);
}
*/

void BrigEmitter::EmitStore(TypedReg src, PointerReg addr, int64_t offset, bool useVectorInstructions, uint8_t equiv)
{
  EmitStore(addr->Segment(), src, Address(addr, offset), useVectorInstructions, equiv);
}

void BrigEmitter::EmitStore(BrigSegment8_t segment, BrigTypeX type, Operand src, OperandAddress addr, uint8_t equiv)
{
  InstMem mem = brigantine.addInst<InstMem>(BRIG_OPCODE_ST, type);
  mem.segment() = segment;
  mem.align() = getNaturalAlignment(type);
  mem.width() = BRIG_WIDTH_NONE;
  mem.equivClass() = equiv;
  mem.operands() = Operands(src, addr) ;
}

void BrigEmitter::EmitStore(BrigTypeX type, Operand src, PointerReg addr, uint8_t equiv) 
{
  EmitStore(addr->Segment(), type, src, Address(addr), equiv);
}

void BrigEmitter::EmitStores(TypedRegList srcs, ItemList vars, bool useVectorInstructions)
{
  assert(srcs->Count() == vars.size());
  for (unsigned i = 0; i < srcs->Count(); ++i) {
    DirectiveVariable var = vars[i];
    EmitStore(var.segment(), srcs->Get(i), Address(var), useVectorInstructions);
  }
}

OperandAddress BrigEmitter::Address(DirectiveVariable v, OperandRegister reg, int64_t offset)
{
  return brigantine.createRef(v.name(), reg, offset);
}

OperandAddress BrigEmitter::Address(PointerReg reg, int64_t offset)
{
  return brigantine.createRef("", reg->Reg(), offset);
}

OperandAddress BrigEmitter::Address(DirectiveVariable v, int64_t offset)
{
  assert(v != 0);
  return brigantine.createRef(v.name(), offset);
}

void BrigEmitter::EmitBufferIndex(PointerReg dst, BrigType16_t type, TypedReg index, size_t count)
{
  count = std::max((unsigned) count, (unsigned) 1);
  uint32_t factor = (uint32_t) count * (uint32_t) getBrigTypeNumBits(type) / 8;
  if (factor == 1) {
    EmitMov(dst, index);
  } else {
    EmitArith(BRIG_OPCODE_MUL, dst, index, brigantine.createImmed(factor, dst->Type()));
  }
}

void BrigEmitter::EmitBufferIndex(PointerReg dst, BrigType16_t type, size_t count)
{
  EmitBufferIndex(dst, type, EmitWorkitemFlatAbsId(dst->TypeSizeBits() == 64), count);
}

void BrigEmitter::EmitLoadFromBuffer(TypedReg dst, DirectiveVariable addr, BrigSegment8_t segment, bool useVectorInstructions)
{
  PointerReg addrReg = AddAReg(segment);
  EmitLoad(addr.segment(), addrReg, Address(addr));
  PointerReg indexReg = AddAReg(segment);
  EmitBufferIndex(indexReg, dst->Type(), dst->Count());
  EmitArith(BRIG_OPCODE_ADD, addrReg, addrReg, indexReg->Reg());
  EmitLoad(dst, addrReg, 0, useVectorInstructions);
}

void BrigEmitter::EmitStoreToBuffer(TypedReg src, DirectiveVariable addr, BrigSegment8_t segment, bool useVectorInstructions)
{
  PointerReg addrReg = AddAReg(segment);
  EmitLoad(addr.segment(), addrReg, Address(addr));
  PointerReg indexReg = AddAReg(segment);
  EmitBufferIndex(indexReg, src->Type(), src->Count());
  EmitArith(BRIG_OPCODE_ADD, addrReg, addrReg, indexReg->Reg());
  EmitStore(src, addrReg, 0, useVectorInstructions);
}

void BrigEmitter::EmitLoadsFromBuffers(TypedRegList dsts, ItemList buffers, BrigSegment8_t segment, bool useVectorInstructions)
{
  assert(dsts->Count() == buffers.size());
  for (unsigned i = 0; i < dsts->Count(); ++i) {
    EmitLoadFromBuffer(dsts->Get(i), buffers[i], segment, useVectorInstructions);
  }
}

void BrigEmitter::EmitStoresToBuffers(TypedRegList srcs, ItemList buffers, BrigSegment8_t segment, bool useVectorInstructions)
{
  assert(srcs->Count() == buffers.size());
  for (unsigned i = 0; i < srcs->Count(); ++i) {
    EmitStoreToBuffer(srcs->Get(i), buffers[i], segment, useVectorInstructions);
  }
}

BrigType16_t BrigEmitter::ArithType(Brig::BrigOpcode16_t opcode, BrigType16_t operandType) const
{
  switch (opcode) {
    case BRIG_OPCODE_SHL:
    case BRIG_OPCODE_SHR: {
      switch (operandType) {
        case BRIG_TYPE_B32: return BRIG_TYPE_U32;
        case BRIG_TYPE_B64: return BRIG_TYPE_U64;
        default: return operandType;
      }
    }
    case BRIG_OPCODE_AND: 
    case BRIG_OPCODE_OR: 
    case BRIG_OPCODE_XOR: 
    case BRIG_OPCODE_NOT:{
      switch (operandType) {
        case BRIG_TYPE_U32: case BRIG_TYPE_S32: return BRIG_TYPE_B32;
        case BRIG_TYPE_U64: case BRIG_TYPE_S64: return BRIG_TYPE_B64;
        default: return operandType;
      }                   
    } 
    default: return operandType;
  }
}

InstBasic BrigEmitter::EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, Operand o)
{
  assert(dst->Type() == src0->Type());
  InstBasic inst = brigantine.addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
  inst.operands() = Operands(dst->Reg(), src0->Reg(), o);
  return inst;
}

InstBasic BrigEmitter::EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, Operand src1, Operand src2) {
  assert(dst->Type() == src0->Type());
  InstBasic inst = brigantine.addInst<InstBasic>(opcode, ArithType(opcode, src0->Type()));
  inst.operands() = Operands(dst->Reg(), src0->Reg(), src1, src2);
  return inst;
}

InstBasic BrigEmitter::EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, const TypedReg& src1, Operand o)
{
  return EmitArith(opcode, dst, src0, src1->Reg(), o);
}

InstBasic BrigEmitter::EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, Operand src1, const TypedReg& src2)
{
  return EmitArith(opcode, dst, src0, src1, src2->Reg());
}

InstBasic BrigEmitter::EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, Operand o)
{
  InstBasic inst = brigantine.addInst<InstBasic>(opcode, ArithType(opcode, dst->Type()));
  inst.operands() = Operands(dst->Reg(), o);
  return inst;
}

InstBasic BrigEmitter::EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, Operand src0, Operand op)
{
  InstBasic inst = brigantine.addInst<InstBasic>(opcode, ArithType(opcode, dst->Type()));
  inst.operands() = Operands(dst->Reg(), src0, op);
  return inst;
}

InstCmp BrigEmitter::EmitCmp(OperandRegister b, const TypedReg& src0, Operand src1, BrigCompareOperation8_t cmp)
{
  InstCmp inst = brigantine.addInst<InstCmp>(BRIG_OPCODE_CMP, BRIG_TYPE_B1);
  BrigType16_t sourceType = src0->Type();
  switch (sourceType) {
    case BRIG_TYPE_B32: sourceType = BRIG_TYPE_U32; break;
    case BRIG_TYPE_B64: sourceType = BRIG_TYPE_U64; break;
    default: break;
  }
  inst.sourceType() = sourceType;
  inst.compare() = cmp;
  inst.operands() = Operands(b, src0->Reg(), src1);
  return inst;
}

InstCmp BrigEmitter::EmitCmp(OperandRegister b, const TypedReg& src0, const TypedReg& src1, BrigCompareOperation8_t cmp)
{
  assert(src0->Type() == src1->Type());
  return EmitCmp(b, src0, src1->Reg(), cmp);
}

void BrigEmitter::EmitCmpTo(TypedReg result, TypedReg src0, Operand src1, BrigCompareOperation8_t cmp)
{
  if (result->Type() == BRIG_TYPE_B1) {
    EmitCmp(result->Reg(), src0, src1, cmp);
  } else {
    TypedReg c = AddCTReg();
    EmitCmp(c->Reg(), src0, src1, cmp);
    EmitCvt(result, c);
  }
}


InstAddr BrigEmitter::EmitLda(PointerReg dst, OperandAddress addr)
{
  InstAddr inst = brigantine.addInst<InstAddr>(BRIG_OPCODE_LDA, dst->Type());
  inst.segment() = dst->Segment();
  inst.operands() = Operands(dst->Reg(), addr);
  return inst;
}

InstAddr BrigEmitter::EmitLda(PointerReg dst, DirectiveVariable v)
{
  return EmitLda(dst, brigantine.createRef(v.name()));
}

InstSegCvt BrigEmitter::EmitStof(PointerReg dst, PointerReg src, bool nonull)
{
  InstSegCvt inst = brigantine.addInst<InstSegCvt>(BRIG_OPCODE_STOF, dst->Type());
  inst.segment() = src->Segment();
  inst.sourceType() = src->Type();
  inst.modifier().isNoNull() = nonull;
  inst.operands() = Operands(dst->Reg(), src->Reg());
  return inst;
}

InstSegCvt BrigEmitter::EmitFtos(PointerReg dst, PointerReg src, bool nonull)
{
  InstSegCvt inst = brigantine.addInst<InstSegCvt>(BRIG_OPCODE_FTOS, dst->Type());
  inst.segment() = dst->Segment();
  inst.sourceType() = src->Type();
  inst.modifier().isNoNull() = nonull;
  inst.operands() = Operands(dst->Reg(), src->Reg());
  return inst;
}

InstSegCvt BrigEmitter::EmitSegmentp(const TypedReg& dst, PointerReg src, BrigSegment8_t segment, bool nonull)
{
  assert(src->Segment() == BRIG_SEGMENT_FLAT);
  InstSegCvt inst = brigantine.addInst<InstSegCvt>(BRIG_OPCODE_SEGMENTP, dst->Type());
  inst.segment() = segment;
  inst.sourceType() = src->Type();
  inst.modifier().isNoNull() = nonull;
  inst.operands() = Operands(dst->Reg(), src->Reg());
  return inst;
}

InstSeg BrigEmitter::EmitNullPtr(PointerReg dst)
{
  InstSeg inst = brigantine.addInst<InstSeg>(BRIG_OPCODE_NULLPTR, dst->Type());
  inst.segment() = dst->Segment();
  inst.operands() = Operands(dst->Reg());
  return inst;
}

DirectiveVariable BrigEmitter::EmitVariableDefinition(const std::string& name, BrigSegment8_t segment, BrigType16_t type, BrigAlignment8_t align, uint64_t dim, bool isConst, bool output)
{
  if (align == BRIG_ALIGNMENT_NONE) { align = getNaturalAlignment(type); }
  DirectiveVariable v = brigantine.addVariable(GetVariableNameHere(name), segment, type);
  v.linkage() = GetVariableLinkageHere();
  switch (segment) {
  case BRIG_SEGMENT_GLOBAL:
    v.allocation() = BRIG_ALLOCATION_PROGRAM;
    break;
  case BRIG_SEGMENT_READONLY:
    v.allocation() = BRIG_ALLOCATION_AGENT;
    break;
  case BRIG_SEGMENT_GROUP:
  case BRIG_SEGMENT_PRIVATE:
  case BRIG_SEGMENT_SPILL:
    v.allocation() = BRIG_ALLOCATION_AUTOMATIC;
    break;
  case BRIG_SEGMENT_ARG:
    v.allocation() = BRIG_ALLOCATION_AUTOMATIC;
    break;
  default:
    v.allocation() = BRIG_ALLOCATION_NONE;
    break;
  }
  v.modifier().isDefinition() = 1;
  v.modifier().isConst() = isConst;
  v.dim() = dim;
  v.align() = align;
  if (currentScope == ES_FUNCARG && (segment == BRIG_SEGMENT_ARG || segment == BRIG_SEGMENT_KERNARG)) {
    if (output && segment == BRIG_SEGMENT_ARG) {
      AddOutputParameter(v);
    } else {
      AddInputParameter(v);
    }
  }
  return v;
}

DirectiveVariable BrigEmitter::EmitPointerDefinition(const std::string& name, BrigSegment8_t segment, BrigSegment8_t asegment)
{
  return EmitVariableDefinition(name, segment, PointerType(asegment));
}

void BrigEmitter::EmitVariableInitializer(HSAIL_ASM::DirectiveVariable var, HSAIL_ASM::SRef data) {
  var.init() = brigantine.createOperandConstantBytes(data, var.elementType(), var.isArray());
}

InstCvt BrigEmitter::EmitCvt(Operand dst, BrigType16_t dstType, Operand src, BrigType16_t srcType)
{
   InstCvt inst = brigantine.addInst<InstCvt>(BRIG_OPCODE_CVT, dstType);
   inst.sourceType() = srcType;
   inst.operands() = Operands(dst, src);
   return inst;
}

InstCvt BrigEmitter::EmitCvt(const TypedReg& dst, const TypedReg& src)
{
   InstCvt inst = brigantine.addInst<InstCvt>(BRIG_OPCODE_CVT, dst->Type());
   inst.sourceType() = src->Type();
   inst.operands() = Operands(dst->Reg(), src->Reg());
   return inst;
}

InstCvt BrigEmitter::EmitCvt(const TypedReg& dst, const TypedReg& src, BrigRound round)
{
   InstCvt inst = brigantine.addInst<InstCvt>(BRIG_OPCODE_CVT, dst->Type());
   inst.sourceType() = src->Type();
   inst.operands() = Operands(dst->Reg(), src->Reg());
   inst.round() = round;
   return inst;
}

void BrigEmitter::EmitCvtOrMov(const TypedReg& dst, const TypedReg& src) 
{
  if (dst->Type() != src->Type()) {
    EmitCvt(dst, src);
  } else {
    EmitMov(dst, src);
  }
}

InstBr BrigEmitter::EmitCall(HSAIL_ASM::DirectiveFunction f, ItemList ins, ItemList outs) 
{
    InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_CALL, BRIG_TYPE_NONE);
    inst.width() = BRIG_WIDTH_ALL;
    std::string name = f.name().str();
    inst.operands() = Operands(
      brigantine.createCodeList(outs),
      brigantine.createExecutableRef(name),
      brigantine.createCodeList(ins));
    return inst;
}

void BrigEmitter::EmitCallSeq(DirectiveFunction f, TypedRegList inRegs, TypedRegList outRegs, bool useVectorInstructions)
{
  ItemList ins;
  ItemList outs;
  StartArgScope();
  DirectiveVariable fArg = f.next();
  for (unsigned j = 0; j < outRegs->Count(); ++j) {
    assert(fArg);
    outs.push_back(EmitVariableDefinition(OName(j), BRIG_SEGMENT_ARG, fArg.type(), fArg.align(), fArg.dim()));
    fArg = fArg.next();
  }
//  assert(fArg = f.firstInArg());
  for (unsigned i = 0; i < inRegs->Count(); ++i) {
    assert(fArg);
    //assert(fArg.type == inRegs->Get(i)->Type());
    ins.push_back(EmitVariableDefinition(IName(i), BRIG_SEGMENT_ARG, fArg.type(), fArg.align(), fArg.dim()));
    fArg = fArg.next();
  }
  EmitStores(inRegs, ins, useVectorInstructions);
  EmitCall(f, ins, outs);
  EmitLoads(outRegs, outs, useVectorInstructions);
  EndArgScope();
}

std::string BrigEmitter::EmitLabel(const std::string& l)
{
  std::string ln = l;
  if (ln.empty()) { ln = AddLabel(); }
  brigantine.addLabel(ln);
  return ln;
}

void BrigEmitter::EmitBr(const std::string &l)
{
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_BR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_ALL;
  inst.operands() = Operands(brigantine.createLabelRef(l));
}

void BrigEmitter::EmitCbr(TypedReg cond, const std::string& l, BrigWidth width)
{
  EmitCbr(cond->Reg(), l, width);
}

void BrigEmitter::EmitCbr(Operand src, const std::string& l, BrigWidth width)
{
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_CBR, BRIG_TYPE_B1);
  if (BRIG_WIDTH_NONE == width) inst.width() = BRIG_WIDTH_1;
  else inst.width() = width;
  inst.operands() = Operands(src, brigantine.createLabelRef(l));
}

void BrigEmitter::EmitSbr(BrigTypeX type, Operand src, const std::vector<std::string>& labels, BrigWidth width)
{
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_SBR, type);
  if (BRIG_WIDTH_NONE == width) inst.width() = BRIG_WIDTH_1;
  else inst.width() = width;
  std::vector<SRef> labelrefs;
  for (const std::string& l : labels) { labelrefs.push_back((SRef) l); }
  Operand labellist = brigantine.createLabelList(labelrefs);
  inst.operands() = Operands(src, labellist);
}

void BrigEmitter::EmitBarrier(BrigWidth width)
{
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_BARRIER, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_ALL;
  inst.operands() = ItemList();
}

DirectiveFbarrier BrigEmitter::EmitFbarrierDefinition(const std::string& name) 
{
  DirectiveFbarrier fb = brigantine.addFbarrier(GetVariableNameHere(name));
  fb.modifier().isDefinition() = true;
  fb.linkage() = GetVariableLinkageHere();
  return fb;
}

void BrigEmitter::EmitInitfbar(DirectiveFbarrier fb) 
{
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_INITFBAR, BRIG_TYPE_NONE);
  inst.operands() = Operands(brigantine.createCodeRef(fb));
}

void BrigEmitter::EmitInitfbarInFirstWI(DirectiveFbarrier fb) 
{
  std::string label = AddLabel();
  TypedReg wiId = EmitWorkitemFlatId();
  TypedReg cmp = AddCTReg();
  EmitCmp(cmp->Reg(), wiId, Immed(wiId->Type(), 0), BRIG_COMPARE_NE);
  EmitCbr(cmp->Reg(), label);
  EmitInitfbar(fb);
  EmitLabel(label);
  EmitBarrier();
}

void BrigEmitter::EmitJoinfbar(DirectiveFbarrier fb) 
{
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_JOINFBAR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_WAVESIZE;
  inst.operands() = Operands(brigantine.createCodeRef(fb));
}

void BrigEmitter::EmitWaitfbar(DirectiveFbarrier fb) 
{
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_WAITFBAR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_WAVESIZE;
  inst.operands() = Operands(brigantine.createCodeRef(fb));
}

void BrigEmitter::EmitArrivefbar(DirectiveFbarrier fb) 
{
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_ARRIVEFBAR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_WAVESIZE;
  inst.operands() = Operands(brigantine.createCodeRef(fb));
}

void BrigEmitter::EmitLeavefbar(DirectiveFbarrier fb) 
{
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_LEAVEFBAR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_WAVESIZE;
  inst.operands() = Operands(brigantine.createCodeRef(fb));
}

void BrigEmitter::EmitReleasefbar(DirectiveFbarrier fb) 
{
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_RELEASEFBAR, BRIG_TYPE_NONE);
  inst.operands() = Operands(brigantine.createCodeRef(fb));
}

void BrigEmitter::EmitReleasefbarInFirstWI(DirectiveFbarrier fb) 
{
  std::string label = AddLabel();
  TypedReg wiId = EmitWorkitemFlatId();
  TypedReg cmp = AddCTReg();
  EmitCmp(cmp->Reg(), wiId, Immed(wiId->Type(), 0), BRIG_COMPARE_NE);
  EmitCbr(cmp->Reg(), label);
  EmitReleasefbar(fb);
  EmitLabel(label);
  EmitBarrier();
}


void BrigEmitter::EmitInitfbar(TypedReg fb) 
{
  assert(fb->Type() == BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_INITFBAR, BRIG_TYPE_NONE);
  inst.operands() = Operands(fb->Reg());
}

void BrigEmitter::EmitInitfbarInFirstWI(TypedReg fb) 
{
  assert(fb->Type() == BRIG_TYPE_U32);
  std::string label = AddLabel();
  TypedReg wiId = EmitWorkitemFlatId();
  TypedReg cmp = AddCTReg();
  EmitCmp(cmp->Reg(), wiId, Immed(wiId->Type(), 0), BRIG_COMPARE_NE);
  EmitCbr(cmp->Reg(), label);
  EmitInitfbar(fb);
  EmitLabel(label);
  EmitBarrier();
}

void BrigEmitter::EmitJoinfbar(TypedReg fb) 
{
  assert(fb->Type() == BRIG_TYPE_U32);
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_JOINFBAR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_WAVESIZE;
  inst.operands() = Operands(fb->Reg());
}

void BrigEmitter::EmitWaitfbar(TypedReg fb) 
{
  assert(fb->Type() == BRIG_TYPE_U32);
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_WAITFBAR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_WAVESIZE;
  inst.operands() = Operands(fb->Reg());
}

void BrigEmitter::EmitArrivefbar(TypedReg fb) 
{
  assert(fb->Type() == BRIG_TYPE_U32);
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_ARRIVEFBAR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_WAVESIZE;
  inst.operands() = Operands(fb->Reg());
}

void BrigEmitter::EmitLeavefbar(TypedReg fb) 
{
  assert(fb->Type() == BRIG_TYPE_U32);
  InstBr inst = brigantine.addInst<InstBr>(BRIG_OPCODE_LEAVEFBAR, BRIG_TYPE_NONE);
  inst.width() = BRIG_WIDTH_WAVESIZE;
  inst.operands() = Operands(fb->Reg());
}

void BrigEmitter::EmitReleasefbar(TypedReg fb) 
{
  assert(fb->Type() == BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_RELEASEFBAR, BRIG_TYPE_NONE);
  inst.operands() = Operands(fb->Reg());
}

void BrigEmitter::EmitReleasefbarInFirstWI(TypedReg fb) 
{
  assert(fb->Type() == BRIG_TYPE_U32);
  std::string label = AddLabel();
  TypedReg wiId = EmitWorkitemFlatId();
  TypedReg cmp = AddCTReg();
  EmitCmp(cmp->Reg(), wiId, Immed(wiId->Type(), 0), BRIG_COMPARE_NE);
  EmitCbr(cmp->Reg(), label);
  EmitReleasefbar(fb);
  EmitLabel(label);
  EmitBarrier();
}

void BrigEmitter::EmitLdf(TypedReg dest, DirectiveFbarrier fb) 
{
  assert(dest->Type() == BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_LDF, BRIG_TYPE_U32);
  inst.operands() = Operands(dest->Reg(), brigantine.createCodeRef(fb));
}

BrigTypeX BrigEmitter::SignalType() const
{
  switch (coreConfig->Model()) {
  case BRIG_MACHINE_SMALL: return BRIG_TYPE_SIG32;
  case BRIG_MACHINE_LARGE: return BRIG_TYPE_SIG64;
  default: assert(false); return BRIG_TYPE_NONE;
  }
}

BrigTypeX BrigEmitter::ImageType(unsigned access) const
{
  switch (access) {
  case 1: return BRIG_TYPE_ROIMG;
  case 2: return BRIG_TYPE_WOIMG;
  case 3: return BRIG_TYPE_RWIMG;
  default: assert(false); return BRIG_TYPE_NONE;
  }
}

BrigTypeX BrigEmitter::SamplerType() const
{
  return BRIG_TYPE_SAMP;
}

BrigTypeX BrigEmitter::AtomicValueBitType() const
{
  switch (coreConfig->Model()) {
  case BRIG_MACHINE_SMALL: return BRIG_TYPE_B32;
  case BRIG_MACHINE_LARGE: return BRIG_TYPE_B64;
  default: assert(false); return BRIG_TYPE_NONE;
  }
}


BrigTypeX BrigEmitter::SignalValueBitType() const
{
  return AtomicValueBitType();
}

BrigTypeX BrigEmitter::AtomicValueIntType(bool isSigned) const
{
  switch (coreConfig->Model()) {
  case BRIG_MACHINE_SMALL: return isSigned ? BRIG_TYPE_S32 : BRIG_TYPE_U32;
  case BRIG_MACHINE_LARGE: return isSigned ? BRIG_TYPE_S64 : BRIG_TYPE_U64;
  default: assert(false); return BRIG_TYPE_NONE;
  }
}

BrigTypeX BrigEmitter::SignalValueIntType(bool isSigned) const
{
  return AtomicValueIntType(isSigned);
}

BrigTypeX BrigEmitter::AtomicValueType(BrigAtomicOperation op, bool isSigned) const
{
  switch (op) {
  case BRIG_ATOMIC_LD:
  case BRIG_ATOMIC_ST:
  case BRIG_ATOMIC_AND:
  case BRIG_ATOMIC_OR:
  case BRIG_ATOMIC_XOR:
  case BRIG_ATOMIC_EXCH:
  case BRIG_ATOMIC_CAS:
    // 6.6.1., 6.7.1. Explanation of Modifiers. Type
    return AtomicValueBitType();
  case BRIG_ATOMIC_ADD:
  case BRIG_ATOMIC_SUB:
  case BRIG_ATOMIC_MAX:
  case BRIG_ATOMIC_MIN:
    // 6.6.1., 6.7.1. Explanation of Modifiers. Type (signed/unsigned)
    return AtomicValueIntType(isSigned);
  case BRIG_ATOMIC_WRAPINC:
  case BRIG_ATOMIC_WRAPDEC:
    // 6.6.1., 6.7.1. Explanation of Modifiers. Type (is always unsigned)
    return SignalValueIntType(false);
  default:
    assert(false); return BRIG_TYPE_NONE;
  }
}

BrigTypeX BrigEmitter::SignalValueType(BrigAtomicOperation signalOp, bool isSigned) const
{
  switch (signalOp) {
  case BRIG_ATOMIC_LD:
  case BRIG_ATOMIC_ST:
  case BRIG_ATOMIC_AND:
  case BRIG_ATOMIC_OR:
  case BRIG_ATOMIC_XOR:
  case BRIG_ATOMIC_EXCH:
  case BRIG_ATOMIC_CAS:
    // 6.8.1. Explanation of Modifiers. Type
    return SignalValueBitType();
  case BRIG_ATOMIC_ADD:
  case BRIG_ATOMIC_SUB:
    // 6.8.1. Explanation of Modifiers. Type (signed/unsigned)
    return SignalValueIntType(isSigned);
  case BRIG_ATOMIC_WAIT_EQ:
  case BRIG_ATOMIC_WAIT_NE:
  case BRIG_ATOMIC_WAIT_LT:
  case BRIG_ATOMIC_WAIT_GTE:
  case BRIG_ATOMIC_WAITTIMEOUT_EQ:
  case BRIG_ATOMIC_WAITTIMEOUT_NE:
  case BRIG_ATOMIC_WAITTIMEOUT_LT:
  case BRIG_ATOMIC_WAITTIMEOUT_GTE:
    // 6.8.1. Explanation of Modifiers. Type (is always signed)
    return SignalValueIntType(true);
  default:
    assert(false); return BRIG_TYPE_NONE;
  }
}

BrigMemoryOrder BrigEmitter::AtomicMemoryOrder(BrigAtomicOperation atomiclOp, BrigMemoryOrder initialMemoryOrder) const
{
  // 6.6.1., 6.7.1., 6.8.1. Explanation of Modifiers. order
  switch (atomiclOp) {
  case BRIG_ATOMIC_LD:
  case BRIG_ATOMIC_WAIT_EQ:
  case BRIG_ATOMIC_WAIT_NE:
  case BRIG_ATOMIC_WAIT_LT:
  case BRIG_ATOMIC_WAIT_GTE:
  case BRIG_ATOMIC_WAITTIMEOUT_EQ:
  case BRIG_ATOMIC_WAITTIMEOUT_NE:
  case BRIG_ATOMIC_WAITTIMEOUT_LT:
  case BRIG_ATOMIC_WAITTIMEOUT_GTE:
    switch (initialMemoryOrder) {
    case BRIG_MEMORY_ORDER_RELAXED:
    case BRIG_MEMORY_ORDER_SC_ACQUIRE:
      return initialMemoryOrder;
    default:
      return BRIG_MEMORY_ORDER_RELAXED;
    }
  case BRIG_ATOMIC_ST:
    switch (initialMemoryOrder) {
    case BRIG_MEMORY_ORDER_RELAXED:
    case BRIG_MEMORY_ORDER_SC_RELEASE:
      return initialMemoryOrder;
    default:
      return BRIG_MEMORY_ORDER_RELAXED;
    }
  default:
    return initialMemoryOrder;
  }
}

BrigMemoryScope BrigEmitter::AtomicMemoryScope(BrigMemoryScope initialMemoryScope, BrigSegment segment) const
{
  // 6.6.1., 6.7.1., 6.8.1. Explanation of Modifiers. scope
  switch (segment) {
  case BRIG_SEGMENT_GLOBAL:
  case BRIG_SEGMENT_FLAT:
    if (BRIG_MEMORY_SCOPE_WORKITEM == initialMemoryScope) return BRIG_MEMORY_SCOPE_WAVEFRONT;
    return initialMemoryScope;
  case BRIG_SEGMENT_GROUP:
    switch (initialMemoryScope) {
      case BRIG_MEMORY_SCOPE_WORKITEM: return BRIG_MEMORY_SCOPE_WAVEFRONT;
      case BRIG_MEMORY_SCOPE_AGENT:
      case BRIG_MEMORY_SCOPE_SYSTEM:   return BRIG_MEMORY_SCOPE_WORKGROUP;
      default:                         return initialMemoryScope;
    }
  default:
    return initialMemoryScope;
  }
}

void BrigEmitter::EmitAtomic(TypedReg dest, OperandAddress addr, TypedReg src0, TypedReg src1, BrigAtomicOperation op, BrigMemoryOrder memoryOrder, BrigMemoryScope memoryScope, BrigSegment segment, bool isSigned, uint8_t equivClass)
{
  unsigned int instType = AtomicValueType(op, isSigned);
  BrigOpcode opcode = BRIG_OPCODE_ATOMICNORET;
  if (dest) opcode = BRIG_OPCODE_ATOMIC;
  InstAtomic inst = brigantine.addInst<InstAtomic>(opcode, instType);
  inst.segment() = segment;
  inst.atomicOperation() = op;
  inst.memoryOrder() = AtomicMemoryOrder(op, memoryOrder);
  inst.memoryScope() = memoryScope;
  inst.equivClass() = equivClass;
  // 6.6.1., 6.7.1. Syntax for Atomic Operations. Operands
  switch (op) {
  case BRIG_ATOMIC_LD: assert(dest); inst.operands() = Operands(dest->Reg(), addr); break;
  case BRIG_ATOMIC_ST: assert(src0); inst.operands() = Operands(addr, src0->Reg()); break;
  case BRIG_ATOMIC_AND:
  case BRIG_ATOMIC_OR:
  case BRIG_ATOMIC_XOR:
  case BRIG_ATOMIC_ADD:
  case BRIG_ATOMIC_SUB:
  case BRIG_ATOMIC_MAX:
  case BRIG_ATOMIC_MIN:
  case BRIG_ATOMIC_WRAPINC:
  case BRIG_ATOMIC_WRAPDEC: {
    assert(src0);
    if (dest) inst.operands() = Operands(dest->Reg(), addr, src0->Reg());
    else inst.operands() = Operands(addr, src0->Reg());
    break;
  }
  case BRIG_ATOMIC_CAS: {
    assert(src1);
    if (dest) inst.operands() = Operands(dest->Reg(), addr, src0->Reg(), src1->Reg());
    else inst.operands() = Operands(addr, src0->Reg(), src1->Reg());
    break;
  }
  case BRIG_ATOMIC_EXCH:
    assert(dest); assert(src0);
    inst.operands() = Operands(dest->Reg(), addr, src0->Reg());
    break;
  default: assert(false); break;
  }
}

void BrigEmitter::EmitSignalOp(TypedReg dest, TypedReg signal, Operand src0, Operand src1, BrigAtomicOperation signalOp, BrigMemoryOrder memoryOrder, bool isSigned, uint64_t timeout)
{
  unsigned int instType = SignalValueType(signalOp, isSigned);
  memoryOrder = AtomicMemoryOrder(signalOp, memoryOrder);
  BrigOpcode signalOpcode = BRIG_OPCODE_SIGNALNORET;
  if (dest)
    signalOpcode = BRIG_OPCODE_SIGNAL;
  InstSignal inst = brigantine.addInst<InstSignal>(signalOpcode, instType);
  inst.signalType() = signal->Type();
  inst.signalOperation() = signalOp;
  inst.memoryOrder() = memoryOrder;
  // 6.8.1. Syntax for Signal Operations. Operands
  switch (signalOp) {
  case BRIG_ATOMIC_LD: assert(dest); inst.operands() = Operands(dest->Reg(), signal->Reg()); break;
  case BRIG_ATOMIC_ST: assert(src0); inst.operands() = Operands(signal->Reg(), src0); break;
  case BRIG_ATOMIC_AND:
  case BRIG_ATOMIC_OR:
  case BRIG_ATOMIC_XOR:
  case BRIG_ATOMIC_ADD:
  case BRIG_ATOMIC_SUB: {
    assert(src0);
    if (dest) inst.operands() = Operands(dest->Reg(), signal->Reg(), src0);
    else inst.operands() = Operands(signal->Reg(), src0);
    break;
  }
  case BRIG_ATOMIC_CAS: {
    assert(src1);
    if (dest) inst.operands() = Operands(dest->Reg(), signal->Reg(), src0, src1);
    else inst.operands() = Operands(signal->Reg(), src0, src1);
    break;
  }
  case BRIG_ATOMIC_EXCH:
  case BRIG_ATOMIC_WAIT_EQ:
  case BRIG_ATOMIC_WAIT_NE:
  case BRIG_ATOMIC_WAIT_LT:
  case BRIG_ATOMIC_WAIT_GTE:
    assert(dest); assert(src0);
    inst.operands() = Operands(dest->Reg(), signal->Reg(), src0);
    break;
  case BRIG_ATOMIC_WAITTIMEOUT_EQ:
  case BRIG_ATOMIC_WAITTIMEOUT_NE:
  case BRIG_ATOMIC_WAITTIMEOUT_LT:
  case BRIG_ATOMIC_WAITTIMEOUT_GTE:
    assert(dest); assert(src0);
    inst.operands() = Operands(dest->Reg(), signal->Reg(), src0, Brigantine().createImmed(timeout, BRIG_TYPE_U64));
    break;
  default: assert(false); break;
  }
}

void BrigEmitter::EmitSignalOp(TypedReg dest, TypedReg signal, TypedReg src0, TypedReg src1, BrigAtomicOperation signalOp, BrigMemoryOrder memoryOrder, bool isSigned, uint64_t timeout)
{
  EmitSignalOp(dest, signal, src0 ? src0->Reg() : Operand(), src1 ? src1->Reg() : Operand(), signalOp, memoryOrder, isSigned, timeout);
}

void BrigEmitter::EmitSignalWaitLoop(TypedReg dest, TypedReg signal, Operand src0, BrigAtomicOperation signalOp, BrigMemoryOrder memoryOrder, uint64_t timeout)
{
  std::string loopBegin = EmitLabel();
  // main signal operation to test
  if (BRIG_ATOMIC_LD == signalOp)
    EmitSignalOp(dest, signal, NULL, NULL, signalOp, memoryOrder);
  else
    EmitSignalOp(dest, signal, src0, Operand(), signalOp, memoryOrder, false, timeout);
  BrigCompareOperation cmpOp;
  switch (signalOp) {
  case BRIG_ATOMIC_WAIT_EQ:
  case BRIG_ATOMIC_WAITTIMEOUT_EQ:
    cmpOp = BRIG_COMPARE_EQ; break;
  case BRIG_ATOMIC_WAIT_NE:
  case BRIG_ATOMIC_WAITTIMEOUT_NE:
    cmpOp = BRIG_COMPARE_NE; break;
  case BRIG_ATOMIC_WAIT_LT:
  case BRIG_ATOMIC_WAITTIMEOUT_LT:
    cmpOp = BRIG_COMPARE_LT; break;
  case BRIG_ATOMIC_WAIT_GTE:
  case BRIG_ATOMIC_WAITTIMEOUT_GTE:
  case BRIG_ATOMIC_LD:
    cmpOp = BRIG_COMPARE_GE; break;
  default: assert(false); break;
  }
  std::string loopExit = AddLabel();
  TypedReg c = AddCTReg();
  EmitCmp(c->Reg(), dest, src0, cmpOp);
  EmitCbr(c, loopExit);
  EmitBr(loopBegin);
  EmitLabel(loopExit);
}

void BrigEmitter::EmitActiveLaneCount(TypedReg dest, Operand src)
{
  InstLane inst = brigantine.addInst<InstLane>(BRIG_OPCODE_ACTIVELANECOUNT, dest->Type());
  inst.sourceType() = BRIG_TYPE_B1;
  inst.operands() = Operands(dest->Reg(), src);
  inst.width() = BRIG_WIDTH_1;
}

void BrigEmitter::EmitActiveLaneId(TypedReg dest)
{
  InstLane inst = brigantine.addInst<InstLane>(BRIG_OPCODE_ACTIVELANEID, dest->Type());
  inst.sourceType() = BRIG_TYPE_NONE;
  inst.operands() = Operands(dest->Reg());
  inst.width() = BRIG_WIDTH_1;
}

void BrigEmitter::EmitActiveLaneMask(TypedReg dest, Operand src)
{
  InstLane inst = brigantine.addInst<InstLane>(BRIG_OPCODE_ACTIVELANEMASK, dest->Type());
  inst.sourceType() = BRIG_TYPE_B1;
  inst.operands() = Operands(brigantine.createOperandList(dest->Regs()), src);
  inst.width() = BRIG_WIDTH_1;
}

void BrigEmitter::EmitActiveLaneShuffle(TypedReg dest, TypedReg src, TypedReg laneId, TypedReg identity, TypedReg useIdentity)
{
  assert(false);
}

TypedReg BrigEmitter::EmitWorkitemFlatAbsId(bool large)
{
  TypedReg dest = AddTReg(large ? BRIG_TYPE_U64 : BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_WORKITEMFLATABSID, dest->Type());
  inst.operands() = Operands(dest->Reg());
  return dest;
}

TypedReg BrigEmitter::WorkitemFlatAbsId(bool large)
{
  unsigned i = large ? 1 : 0;
  if (!workitemflatabsid[i]) {
    workitemflatabsid[i] = EmitWorkitemFlatAbsId(large);
  }
  return workitemflatabsid[i];
}


TypedReg BrigEmitter::EmitWorkitemAbsId(uint32_t dim, bool large)
{
  unsigned i = large ? 1 : 0;
  if (!workitemabsid[i]) {
    workitemabsid[large] = AddTReg(large ? BRIG_TYPE_U64 : BRIG_TYPE_U32);
    InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_WORKITEMABSID, workitemabsid[i]->Type());
    inst.operands() = Operands(workitemabsid[i]->Reg(), brigantine.createImmed(dim, BRIG_TYPE_U32));
  }
  return workitemabsid[i];
}

std::string BrigEmitter::GetVariableNameHere(const std::string& name)
{
  switch (currentScope) {
  case ES_MODULE:
    switch (name[0]) {
    case '&': return name;
    case '%': assert(false); return "bad_variable";
    default: return std::string("&") + name;
    }

  case ES_FUNCARG:
  case ES_LOCAL:
  case ES_ARG:
    switch (name[0]) {
    case '&': assert(false); return "bad_variable";
    case '%': return name;
    default: return std::string("%") + name;
    }

  default:
    assert(false); return "bad_variable";
  }
}

BrigLinkage BrigEmitter::GetVariableLinkageHere() {
  switch (currentScope) {
  case ES_MODULE: return BRIG_LINKAGE_PROGRAM;
  case ES_LOCAL:
  case ES_FUNCARG: return BRIG_LINKAGE_FUNCTION;
  case ES_ARG: return BRIG_LINKAGE_ARG;
  default: assert(false); return BRIG_LINKAGE_NONE;
  }
}

void BrigEmitter::ResetRegs()
{
  nameIndexes["$s"] = 0;
  nameIndexes["$d"] = 0;
  nameIndexes["$q"] = 0;
  nameIndexes["$c"] = 0;
}

void BrigEmitter::EmitControlDirectiveGeometry(BrigControlDirective d, Grid grid)
{
  DirectiveControl dc = Brigantine().append<DirectiveControl>();
  dc.control() = d;
  switch(d) {
  case BRIG_CONTROL_REQUIREDDIM:
    {
      dc.operands() = Operands(Brigantine().createImmed(grid->Dimensions(),BRIG_TYPE_U32));
    }
    break;
  case BRIG_CONTROL_REQUIREDGRIDSIZE:
    {
      dc.operands() = Operands(Brigantine().createImmed(grid->GridSize(0), BRIG_TYPE_U64), 
                               Brigantine().createImmed(grid->GridSize(1), BRIG_TYPE_U64), 
                               Brigantine().createImmed(grid->GridSize(2), BRIG_TYPE_U64));
    }
    break;
  case BRIG_CONTROL_REQUIREDWORKGROUPSIZE:
    {
      dc.operands() = Operands(Brigantine().createImmed(grid->WorkgroupSize(0), BRIG_TYPE_U32), 
                               Brigantine().createImmed(grid->WorkgroupSize(1), BRIG_TYPE_U32), 
                               Brigantine().createImmed(grid->WorkgroupSize(2), BRIG_TYPE_U32));
    }
    break;
  case BRIG_CONTROL_REQUIRENOPARTIALWORKGROUPS:
    {
      // no operands for this directive
      dc.operands() = Operands(BrigEmitter::nullOperand);
    }
    break;
  case BRIG_CONTROL_MAXFLATWORKGROUPSIZE:
    {
      uint32_t maxVal = grid->WorkgroupSize();
      dc.operands() = Operands(Brigantine().createImmed(maxVal,BRIG_TYPE_U32));
    }
    break;
  case BRIG_CONTROL_MAXFLATGRIDSIZE:
    {
      uint64_t maxVal = grid->GridSize();
      dc.operands() = Operands(Brigantine().createImmed(maxVal,BRIG_TYPE_U64));
    }
    break;
  default:
    assert(false);
  }
}

void BrigEmitter::EmitDynamicMemoryDirective(size_t size) {
  DirectiveControl dc = Brigantine().append<DirectiveControl>();
  dc.control() = BRIG_CONTROL_MAXDYNAMICGROUPSIZE;
  dc.operands() = Operands(Immed(BRIG_TYPE_U32, size));
}

DirectiveLoc BrigEmitter::EmitLocDirective(uint32_t line, uint32_t column, const std::string& fileName) {
  assert(line != 0 && column != 0);
  DirectiveLoc loc = Brigantine().append<DirectiveLoc>();
  loc.line() = line;
  loc.column() = column;
  loc.filename() = fileName;
  return loc;
}

DirectivePragma BrigEmitter::EmitPragmaDirective(ItemList operands) {
  DirectivePragma pragma = Brigantine().append<DirectivePragma>();
  pragma.operands() = operands;
  return pragma;
}

DirectiveControl BrigEmitter::EmitEnableExceptionDirective(bool isBreak, uint32_t exceptionNumber) {
  assert(exceptionNumber <= 0x1F); // 0b11111
  DirectiveControl dc = Brigantine().append<DirectiveControl>();
  dc.control() = isBreak ? BRIG_CONTROL_ENABLEBREAKEXCEPTIONS : BRIG_CONTROL_ENABLEDETECTEXCEPTIONS;
  dc.operands() = Operands(Immed(BRIG_TYPE_U32, exceptionNumber));
  return dc;
}

DirectiveExtension BrigEmitter::EmitExtensionDirective(const std::string& name) {
  assert(name == "CORE" || name == "IMAGE" || name == "");
  DirectiveExtension de = Brigantine().append<DirectiveExtension>();
  de.name() = name;
  return de;
}

/*
void BrigEmitter::EmitControlDirectivesGeometry(const ControlDirectives::Set& directives, const GridGeometry::Spec& geometry)
{
  class EmitAction: public Action<BrigControlDirective> {
  private:
    BrigEmitter& be;
    const GridGeometry::Spec& geometry;

  public:
    EmitAction(BrigEmitter& be_, const GridGeometry::Spec& geometry_) : be(be_), geometry(geometry_) { }
    void operator()(const BrigControlDirective& d) {
      be.EmitControlDirectiveGeometry(d, geometry);
    }
  };
  EmitAction emitAction(*this, geometry);
  directives.Iterate(emitAction);
}
*/

/*
UserModeQueue BrigEmitter::Queue(PointerReg address)
{
  return new(ap) EUserModeQueue(*this, AddName("queue"), address);
}
*/
void BrigEmitter::EmitAgentId(TypedReg dest)
{
  EmitMov(dest, Immed(BRIG_TYPE_U32, 0));
  /*
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_AGENTID, BRIG_TYPE_U32);
  inst.operands() = Operands(dest->Reg());
  */
}

TypedReg BrigEmitter::EmitWorkitemFlatId() {
  TypedReg result = AddTReg(BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_WORKITEMFLATID, BRIG_TYPE_U32);
  inst.operands() = Operands(result->Reg());
  return result;
}

TypedReg BrigEmitter::EmitWorkitemId(uint32_t dim) {
  TypedReg result = AddTReg(BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_WORKITEMID, BRIG_TYPE_U32);
  inst.operands() = Operands(result->Reg(), Immed(inst.type(), dim));
  return result;
}

TypedReg BrigEmitter::EmitCurrentWorkitemFlatId() {
    TypedReg xSize = EmitCurrentWorkgroupSize(0);
    TypedReg ySize = EmitCurrentWorkgroupSize(1);
    
    TypedReg xId = EmitWorkitemId(0);
    TypedReg yId = EmitWorkitemId(1);
    TypedReg zId = EmitWorkitemId(2);

    TypedReg size = AddTReg(BRIG_TYPE_U32);

    // workitemid(1) + workitemid(2) * currentworkgroupsize(1)
    EmitArith(BRIG_OPCODE_MAD, size, zId, ySize->Reg(), yId);
    
    // workitemid(0) + workitemid(1) * currentworkgroupsize(0) + workitemid(2) * currentworkgroupsize(1) * currentworkgroupsize(0)
    EmitArith(BRIG_OPCODE_MAD, size, size, xSize->Reg(), xId);
    return size;
}

TypedReg BrigEmitter::EmitCurrentWorkgroupSize(uint32_t dim) {
  TypedReg result = AddTReg(BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_CURRENTWORKGROUPSIZE, BRIG_TYPE_U32);
  inst.operands() = Operands(result->Reg(), Immed(inst.type(), dim));
  return result;
}

TypedReg BrigEmitter::EmitDim() {
  TypedReg result = AddTReg(BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_DIM, BRIG_TYPE_U32);
  inst.operands() = Operands(result->Reg());
  return result;
}

TypedReg BrigEmitter::EmitGridGroups(uint32_t dim) {
  TypedReg result = AddTReg(BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_GRIDGROUPS, BRIG_TYPE_U32);
  inst.operands() = Operands(result->Reg(), Immed(inst.type(), dim));
  return result;
}

TypedReg BrigEmitter::EmitGridSize(uint32_t dim) {
  TypedReg result = AddTReg(BRIG_TYPE_U32);
  InstBasic inst = Brigantine().addInst<InstBasic>(BRIG_OPCODE_GRIDSIZE, BRIG_TYPE_U32);
  inst.operands() = Operands(result->Reg(), Immed(BRIG_TYPE_U32, dim));
  return result;
}

TypedReg BrigEmitter::EmitPacketCompletionSig() {
  TypedReg result = AddTReg(PointerType());
  InstBasic inst = Brigantine().addInst<InstBasic>(BRIG_OPCODE_PACKETCOMPLETIONSIG, SignalType());
  inst.operands() = Operands(result->Reg());
  return result;
}

TypedReg BrigEmitter::EmitPacketId() {
  TypedReg result = AddTReg(BRIG_TYPE_U64);
  InstBasic inst = Brigantine().addInst<InstBasic>(BRIG_OPCODE_PACKETID, BRIG_TYPE_U64);
  inst.operands() = Operands(result->Reg());
  return result;
}

TypedReg BrigEmitter::EmitWorkgroupId(uint32_t dim) {
  TypedReg result = AddTReg(BRIG_TYPE_U32);
  InstBasic inst = Brigantine().addInst<InstBasic>(BRIG_OPCODE_WORKGROUPID, BRIG_TYPE_U32);
  inst.operands() = Operands(result->Reg(), Immed(BRIG_TYPE_U32, dim));
  return result;
}

TypedReg BrigEmitter::EmitWorkgroupSize(uint32_t dim) {
  TypedReg result = AddTReg(BRIG_TYPE_U32);
  InstBasic inst = Brigantine().addInst<InstBasic>(BRIG_OPCODE_WORKGROUPSIZE, BRIG_TYPE_U32);
  inst.operands() = Operands(result->Reg(), Immed(BRIG_TYPE_U32, dim));
  return result;
}

void BrigEmitter::EmitCuid(TypedReg dest) {
  assert(dest->Type() == BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_CUID, BRIG_TYPE_U32);
  inst.operands() = Operands(dest->Reg());
}

void BrigEmitter::EmitKernargBasePtr(PointerReg dest) {
  auto addrSize = getSegAddrSize(BRIG_SEGMENT_KERNARG, coreConfig->IsLarge());
  assert(getBrigTypeNumBits(dest->Type()) == addrSize);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_KERNARGBASEPTR, dest->Type());
  inst.operands() = Operands(dest->Reg());
}

void BrigEmitter::EmitGroupBasePtr(PointerReg dest) {
  auto addrSize = getSegAddrSize(BRIG_SEGMENT_GROUP, coreConfig->IsLarge());
  assert(getBrigTypeNumBits(dest->Type()) == addrSize);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_GROUPBASEPTR, dest->Type());
  inst.operands() = Operands(dest->Reg());
}

void BrigEmitter::EmitLaneid(TypedReg dest) {
  assert(dest->Type() == BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_LANEID, BRIG_TYPE_U32);
  inst.operands() = Operands(dest->Reg());
}

void BrigEmitter::EmitMaxcuid(TypedReg dest) {
  assert(dest->Type() == BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_MAXCUID, BRIG_TYPE_U32);
  inst.operands() = Operands(dest->Reg());
}

void BrigEmitter::EmitMaxwaveid(TypedReg dest) {
  assert(dest->Type() == BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_MAXWAVEID, BRIG_TYPE_U32);
  inst.operands() = Operands(dest->Reg());
}

void BrigEmitter::EmitNop() {
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_NOP, BRIG_TYPE_NONE);
  inst.operands() = ItemList();
}

void BrigEmitter::EmitClock(TypedReg dest) {
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_CLOCK, BRIG_TYPE_U64);
  inst.operands() = Operands(dest->Reg());
}

void BrigEmitter::EmitWaveid(TypedReg dest) {
  assert(dest->Type() == BRIG_TYPE_U32);
  InstBasic inst = brigantine.addInst<InstBasic>(BRIG_OPCODE_WAVEID, BRIG_TYPE_U32);
  inst.operands() = Operands(dest->Reg());
}

void BrigEmitter::EmitMemfence(BrigMemoryOrder memoryOrder, BrigMemoryScope globalScope, BrigMemoryScope groupScope, BrigMemoryScope imageScope) {
  // TODO: Change to 1.0 Final Spec
  assert(BRIG_MEMORY_SCOPE_NONE == imageScope);
  InstMemFence inst = brigantine.addInst<InstMemFence>(BRIG_OPCODE_MEMFENCE, BRIG_TYPE_NONE);
  inst.memoryOrder() = memoryOrder;
  inst.globalSegmentMemoryScope() = globalScope;
  inst.groupSegmentMemoryScope() = groupScope;
  inst.imageSegmentMemoryScope() = imageScope;
  inst.operands() = ItemList();
}

}

}
