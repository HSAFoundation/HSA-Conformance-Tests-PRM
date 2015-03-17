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

#ifndef BRIG_EMITTER_HPP
#define BRIG_EMITTER_HPP

#include <vector>
#include "HSAILBrigContainer.h"
#include "HSAILBrigantine.h"
#include "hsail_c.h"
#include "Arena.hpp"
#include "HexlTest.hpp"
#include "EmitterCommon.hpp"

#define OFFSETOF_FIELD(structName, field) ((size_t)&(((structName*)0)->field))
#define EmitStructLoad(data, ptr, structName, field) te->Brig()->EmitLoad(BRIG_SEGMENT_GLOBAL, data, te->Brig()->Address(ptr, OFFSETOF_FIELD(structName, field)))
#define EmitStructStore(data, ptr, structName, field) te->Brig()->EmitStore(BRIG_SEGMENT_GLOBAL, data, te->Brig()->Address(ptr, OFFSETOF_FIELD(structName, field)))
#define EmitStructStoreI(type, data, ptr, structName, field) te->Brig()->EmitStore(BRIG_SEGMENT_GLOBAL, type, data, te->Brig()->Address(ptr, OFFSETOF_FIELD(structName, field)))
#define EmitDispatchStore(data, ptr, field) EmitStructStore(data, ptr, hsa_kernel_dispatch_packet_t, field)
#define EmitDispatchStoreI(type, data, ptr, field) EmitStructStoreI(type, data, ptr, hsa_kernel_dispatch_packet_t, field)

namespace hexl {

namespace emitter {

class CoreConfig;

namespace Variables { class Spec; }

class BrigEmitter {
private:
  hexl::Arena* ap;
  brig_container_t brig;
  CoreConfig* coreConfig;
  HSAIL_ASM::Brigantine brigantine;

  std::map<std::string, unsigned> nameIndexes;

  static const HSAIL_ASM::Operand nullOperand;

  EmitterScope currentScope;
  HSAIL_ASM::DirectiveExecutable currentExecutable;

  TypedReg workitemabsid[2];
  TypedReg workitemflatabsid[2];

  HSAIL_ASM::OperandAddress IncrementAddress(HSAIL_ASM::OperandAddress addr, int64_t offset);

  void ResetRegs();

public:
  BrigEmitter();
  ~BrigEmitter();

  void SetCoreConfig(CoreConfig* coreConfig) { assert(coreConfig && !this->coreConfig); this->coreConfig = coreConfig; }
  HSAIL_ASM::BrigContainer* BrigC();
  brig_container_t Brig();
  HSAIL_ASM::Brigantine& Brigantine() { return brigantine; }

  std::string AddName(const std::string& name, bool addZero = false);

  HSAIL_ASM::OperandRegister Reg(const std::string& name);

  HSAIL_ASM::OperandRegister AddReg(const std::string& name);
  HSAIL_ASM::OperandRegister AddReg(BrigType16_t type);
  HSAIL_ASM::OperandRegister AddSReg() { return AddReg("$s"); }
  HSAIL_ASM::OperandRegister AddDReg() { return AddReg("$d"); }
  HSAIL_ASM::OperandRegister AddQReg() { return AddReg("$q"); }
  HSAIL_ASM::OperandRegister AddCReg() { return AddReg("$c"); }

  HSAIL_ASM::OperandOperandList AddVec(BrigType16_t type, unsigned count);

  TypedReg AddCTReg();

  PointerReg AddAReg(BrigSegment8_t segment = BRIG_SEGMENT_GLOBAL);
  PointerReg AddAReg(HSAIL_ASM::DirectiveVariable v) { return AddAReg(v.segment()); }

  TypedReg AddTReg(BrigType16_t type, unsigned count = 1);
  TypedRegList AddTRegList();

  BrigType PointerType(BrigSegment8_t asegment = BRIG_SEGMENT_GLOBAL) const;

  std::string TName(unsigned n = 0) { return AddName("tmp"); }
  std::string IName(unsigned n = 0) { return AddName("in"); }
  std::string OName(unsigned n = 0) { return AddName("out"); }
  std::string GenVariableName(BrigSegment segment, bool output = false);

  void Start();
  void End();
  HSAIL_ASM::DirectiveKernel StartKernel(const std::string& name = "&test_kernel");
  void EndKernel();
  HSAIL_ASM::DirectiveExecutable CurrentExecutable() { return currentExecutable; }
  HSAIL_ASM::DirectiveKernel CurrentKernel() { return currentExecutable; }
  HSAIL_ASM::DirectiveFunction CurrentFunction() { return currentExecutable; }
  HSAIL_ASM::DirectiveFunction StartFunction(const std::string& id = "&test_function");
  void EndFunction();
  void StartBody();
  void EndBody();
  void StartArgScope();
  void EndArgScope();
  void AddOutputParameter(HSAIL_ASM::DirectiveVariable sym) { brigantine.addOutputParameter(sym); }
  void AddInputParameter(HSAIL_ASM::DirectiveVariable sym) { brigantine.addInputParameter(sym); }

  HSAIL_ASM::ItemList Operands(HSAIL_ASM::Operand o1, HSAIL_ASM::Operand o2 = nullOperand, HSAIL_ASM::Operand o3 = nullOperand, HSAIL_ASM::Operand o4 = nullOperand, HSAIL_ASM::Operand o5 = nullOperand);

  hexl::Value GenerateTestValue(BrigType type, uint64_t id = 0) const;

  // Immediates
  HSAIL_ASM::Operand Immed(BrigType16_t type, uint64_t imm);
  HSAIL_ASM::Operand Immed(BrigType16_t type, HSAIL_ASM::SRef data);
  HSAIL_ASM::Operand Immed(float imm);
  HSAIL_ASM::Operand ImmedString(const std::string& str);
  HSAIL_ASM::Operand Wavesize();

  HSAIL_ASM::InstBasic EmitMov(HSAIL_ASM::Operand dst, HSAIL_ASM::Operand src, unsigned sizeBits);
  void EmitMov(TypedReg dst, HSAIL_ASM::Operand src);
  void EmitMov(TypedReg dst, TypedReg src);
  HSAIL_ASM::InstBasic EmitTypedMov(BrigType16_t moveType, HSAIL_ASM::OperandRegister dst, HSAIL_ASM::Operand src);
  TypedReg AddInitialTReg(BrigType16_t type, uint64_t initialValue, unsigned count = 1);

  // Memory operations
  HSAIL_ASM::OperandAddress Address(HSAIL_ASM::DirectiveVariable v, HSAIL_ASM::OperandRegister reg, int64_t offset);
  HSAIL_ASM::OperandAddress Address(PointerReg ptr, int64_t offset = 0);
  HSAIL_ASM::OperandAddress Address(HSAIL_ASM::DirectiveVariable v, int64_t offset = 0);

  HSAIL_ASM::InstMem EmitLoad(BrigSegment8_t segment, BrigType16_t type, HSAIL_ASM::Operand dst, HSAIL_ASM::OperandAddress addr, uint8_t equiv = 0);
  void EmitLoad(BrigSegment8_t segment, TypedReg dst, HSAIL_ASM::OperandAddress addr, bool useVectorInstructions = true, uint8_t equiv = 0);
  //void EmitLoad(TypedReg dst, HSAIL_ASM::DirectiveVariable v, int64_t offset = 0, bool useVectorInstructions = true);
  void EmitLoad(TypedReg dst, PointerReg addr, int64_t offset = 0, bool useVectorInstructions = true, uint8_t equiv = 0);
  void EmitLoads(TypedRegList dsts, HSAIL_ASM::ItemList vars, bool useVectorInstructions = true);

  BrigType16_t MemOpType(BrigType16_t type);
  HSAIL_ASM::InstMem EmitStore(BrigSegment8_t segment, BrigType16_t type, HSAIL_ASM::Operand src, HSAIL_ASM::OperandAddress addr, uint8_t equiv = 0);
  void EmitStore(BrigSegment8_t segment, TypedReg src, HSAIL_ASM::OperandAddress addr, bool useVectorInstructions = true, uint8_t equiv = 0);
  //void EmitStore(TypedReg src, HSAIL_ASM::DirectiveVariable v, int64_t offset = 0, bool useVectorInstructions = false);
  void EmitStore(TypedReg src, PointerReg addr, int64_t offset = 0, bool useVectorInstructions = false, uint8_t equiv = 0);
  void EmitStore(BrigSegment8_t segment, BrigType type, HSAIL_ASM::Operand src, HSAIL_ASM::OperandAddress addr, uint8_t equiv = 0);
  void EmitStore(BrigType type, HSAIL_ASM::Operand src, PointerReg addr, uint8_t equiv = 0);
  void EmitStores(TypedRegList src, HSAIL_ASM::ItemList vars, bool useVectorInstructions = true);

  // Buffer memory operations.
  void EmitBufferIndex(PointerReg dst, BrigType16_t type, TypedReg index, size_t count = 1);
  void EmitBufferIndex(PointerReg dst, BrigType16_t type, size_t count = 1);
  void EmitLoadFromBuffer(TypedReg dst, HSAIL_ASM::DirectiveVariable buffer, BrigSegment8_t segment = BRIG_SEGMENT_GLOBAL, bool useVectorInstructions = true);
  void EmitStoreToBuffer(TypedReg src, HSAIL_ASM::DirectiveVariable buffer, BrigSegment8_t segment = BRIG_SEGMENT_GLOBAL, bool useVectorInstructions = true);
  void EmitLoadsFromBuffers(TypedRegList dsts, HSAIL_ASM::ItemList buffers, BrigSegment8_t segment = BRIG_SEGMENT_GLOBAL, bool useVectorInstructions = true);
  void EmitStoresToBuffers(TypedRegList srcs, HSAIL_ASM::ItemList buffers, BrigSegment8_t segment = BRIG_SEGMENT_GLOBAL, bool useVectorInstructions = true);

  BrigType16_t ArithType(BrigOpcode16_t opcode, BrigType16_t operandType) const;
  HSAIL_ASM::InstBasic EmitArith(BrigOpcode16_t opcode, BrigType16_t type, HSAIL_ASM::Operand dst, HSAIL_ASM::Operand src0, HSAIL_ASM::Operand src1 = HSAIL_ASM::Operand(), HSAIL_ASM::Operand src2 = HSAIL_ASM::Operand());
  HSAIL_ASM::InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, HSAIL_ASM::Operand op);
  HSAIL_ASM::InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, const TypedReg& src1, HSAIL_ASM::Operand src2);
  HSAIL_ASM::InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, HSAIL_ASM::Operand src1, const TypedReg& src2);
  HSAIL_ASM::InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, const TypedReg& src0, HSAIL_ASM::Operand src1, HSAIL_ASM::Operand src2);
  HSAIL_ASM::InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, HSAIL_ASM::Operand o);
  HSAIL_ASM::InstBasic EmitArith(BrigOpcode16_t opcode, const TypedReg& dst, HSAIL_ASM::Operand src0, HSAIL_ASM::Operand op);
  HSAIL_ASM::InstCmp EmitCmp(HSAIL_ASM::OperandRegister b, BrigType16_t type, HSAIL_ASM::Operand src0, HSAIL_ASM::Operand src1, BrigCompareOperation8_t cmp);
  HSAIL_ASM::InstCmp EmitCmp(HSAIL_ASM::OperandRegister b, const TypedReg& src0, HSAIL_ASM::Operand src1, BrigCompareOperation8_t cmp);
  HSAIL_ASM::InstCmp EmitCmp(HSAIL_ASM::OperandRegister b, const TypedReg& src0, const TypedReg& src1, BrigCompareOperation8_t cmp);
  void EmitCmpTo(TypedReg result, TypedReg src0, HSAIL_ASM::Operand src1, BrigCompareOperation8_t cmp);
  HSAIL_ASM::InstCvt EmitCvt(HSAIL_ASM::Operand dst, BrigType16_t dstType, HSAIL_ASM::Operand src, BrigType16_t srcType);
  HSAIL_ASM::InstCvt EmitCvt(const TypedReg& dst, const TypedReg& src);
  HSAIL_ASM::InstCvt EmitCvt(const TypedReg& dst, const TypedReg& src, BrigRound round);
  void EmitCvtOrMov(const TypedReg& dst, const TypedReg& src);

  HSAIL_ASM::InstAddr EmitLda(PointerReg dst, HSAIL_ASM::OperandAddress addr);
  HSAIL_ASM::InstAddr EmitLda(PointerReg dst, HSAIL_ASM::DirectiveVariable v);

  HSAIL_ASM::InstSegCvt EmitStof(PointerReg dst, PointerReg src, bool nonull = false);
  HSAIL_ASM::InstSegCvt EmitFtos(PointerReg dst, PointerReg src, bool nonull = false);
  HSAIL_ASM::InstSegCvt EmitSegmentp(const TypedReg& dst, PointerReg src, BrigSegment8_t segment, bool nonull = false);

  HSAIL_ASM::InstSeg EmitNullPtr(PointerReg dst);

  HSAIL_ASM::DirectiveVariable EmitVariableDefinition(const std::string& name, BrigSegment8_t segment, BrigType16_t type, BrigAlignment8_t align = BRIG_ALIGNMENT_NONE, uint64_t dim = 0, bool isConst = false, bool output = false);
  HSAIL_ASM::DirectiveVariable EmitPointerDefinition(const std::string& name, BrigSegment8_t segment, BrigSegment8_t asegment = BRIG_SEGMENT_GLOBAL);
  void EmitVariableInitializer(HSAIL_ASM::DirectiveVariable var, HSAIL_ASM::SRef data);

  HSAIL_ASM::InstBr EmitCall(HSAIL_ASM::DirectiveFunction f, HSAIL_ASM::ItemList ins, HSAIL_ASM::ItemList outs);
  void EmitCallSeq(HSAIL_ASM::DirectiveFunction f, TypedRegList inRegs, TypedRegList outRegs, bool useVectorInstructions = true);
  void EmitCallSeq(Function f, TypedRegList inRegs, TypedRegList outRegs, bool useVectorInstructions = true);
  void EmitControlDirectiveGeometry(BrigControlDirective d, hexl::Grid grid);
  void EmitDynamicMemoryDirective(size_t size);
  HSAIL_ASM::DirectiveLoc EmitLocDirective(uint32_t line, uint32_t column = 1, const std::string& fileName = "");
  HSAIL_ASM::DirectivePragma EmitPragmaDirective(HSAIL_ASM::ItemList operands);
  HSAIL_ASM::DirectiveControl EmitEnableExceptionDirective(bool isBreak, uint32_t exceptionNumber);
  HSAIL_ASM::DirectiveExtension EmitExtensionDirective(const std::string& name);
//  void EmitControlDirectivesGeometry(const ControlDirectives::Set& directives, const GridGeometry& g);

  void EmitMemfence(BrigMemoryOrder memoryOrder, BrigMemoryScope globalScope, BrigMemoryScope groupScope, BrigMemoryScope imageScope);

  // Branches
  std::string AddLabel() { return AddName("@L"); }
  std::string EmitLabel(const std::string& l = "");
  void EmitBr(const std::string &l);
  void EmitCbr(TypedReg cond, const std::string& l, BrigWidth width = BRIG_WIDTH_1);
  void EmitCbr(HSAIL_ASM::Operand src, const std::string& l, BrigWidth width = BRIG_WIDTH_1);
  void EmitSbr(BrigType type, HSAIL_ASM::Operand src, const std::vector<std::string>& labels, BrigWidth width = BRIG_WIDTH_1);

  // Barriers
  void EmitBarrier(BrigWidth width = BRIG_WIDTH_ALL);
  HSAIL_ASM::DirectiveFbarrier EmitFbarrierDefinition(const std::string& name);
  void EmitInitfbar(HSAIL_ASM::DirectiveFbarrier fb);
  void EmitInitfbarInFirstWI(HSAIL_ASM::DirectiveFbarrier fb);
  void EmitJoinfbar(HSAIL_ASM::DirectiveFbarrier fb);
  void EmitWaitfbar(HSAIL_ASM::DirectiveFbarrier fb);
  void EmitArrivefbar(HSAIL_ASM::DirectiveFbarrier fb);
  void EmitLeavefbar(HSAIL_ASM::DirectiveFbarrier fb);
  void EmitReleasefbar(HSAIL_ASM::DirectiveFbarrier fb);
  void EmitReleasefbarInFirstWI(HSAIL_ASM::DirectiveFbarrier fb);
  void EmitInitfbar(TypedReg fb);
  void EmitInitfbarInFirstWI(TypedReg fb);
  void EmitJoinfbar(TypedReg fb);
  void EmitWaitfbar(TypedReg fb);
  void EmitArrivefbar(TypedReg fb);
  void EmitLeavefbar(TypedReg fb);
  void EmitReleasefbar(TypedReg fb);
  void EmitReleasefbarInFirstWI(TypedReg fb);
  void EmitLdf(TypedReg dest, HSAIL_ASM::DirectiveFbarrier fb);

  // Atomics
  BrigType AtomicValueBitType() const;
  BrigType AtomicValueIntType(bool isSigned = false) const;
  BrigType AtomicValueType(BrigAtomicOperation op, bool isSigned = false) const;
  BrigMemoryOrder AtomicMemoryOrder(BrigAtomicOperation atomiclOp, BrigMemoryOrder initialMemoryOrder) const;
  BrigMemoryScope AtomicMemoryScope(BrigMemoryScope initialMemoryScope, BrigSegment segment) const;
  void EmitAtomic(TypedReg dest, HSAIL_ASM::OperandAddress addr, TypedReg src0, TypedReg src1, BrigAtomicOperation op, BrigMemoryOrder memoryOrder, BrigMemoryScope memoryScope, BrigSegment segment = BRIG_SEGMENT_FLAT, bool isSigned = false, uint8_t equivClass = 0);

  // Signals
  BrigType SignalType() const;
  BrigType SignalValueBitType() const;
  BrigType SignalValueIntType(bool isSigned = false) const;
  BrigType SignalValueType(BrigAtomicOperation signalOp, bool isSigned = false) const;
  BrigMemoryOrder SignalMemoryOrder(BrigAtomicOperation signalOp, BrigMemoryOrder initialMemoryOrder) const;
  void EmitSignalOp(TypedReg dest, TypedReg signal, HSAIL_ASM::Operand src0, HSAIL_ASM::Operand src1, BrigAtomicOperation signalOp, BrigMemoryOrder memoryOrder, bool isSigned = false, uint64_t timeout = 0);
  void EmitSignalOp(TypedReg dest, TypedReg signal, TypedReg src0, TypedReg src1, BrigAtomicOperation signalOp, BrigMemoryOrder memoryOrder, bool isSigned = false, uint64_t timeout = 0);
  void EmitSignalOp(TypedReg signal, TypedReg src0, TypedReg src1, BrigAtomicOperation signalOp, BrigMemoryOrder memoryOrder, bool isSigned = false, uint64_t timeout = 0) {
    EmitSignalOp(0, signal, src0, src1, signalOp, memoryOrder, isSigned, timeout);
  }
  // Emits simple wait loop, returns register with the last read signal value during wait loop in order to
  // verify further that this acquired value is equal to the dest's ones
  void EmitSignalWaitLoop(TypedReg dest, TypedReg signal, HSAIL_ASM::Operand src0, BrigAtomicOperation signalOp, BrigMemoryOrder memoryOrder, uint64_t timeout = 0);

  // Cross-lane operations.
  void EmitActiveLaneCount(TypedReg dest, HSAIL_ASM::Operand src);
  void EmitActiveLaneId(TypedReg dest);
  void EmitActiveLaneMask(TypedReg dest, HSAIL_ASM::Operand src);
  void EmitActiveLaneShuffle(TypedReg dest, TypedReg src, TypedReg laneId, TypedReg identity, TypedReg useIdentity);

  // User mode queues.
  void EmitAgentId(TypedReg dest);

  // Images operations
  BrigType ImageType(unsigned access) const;
  BrigType SamplerType() const;

  // Exception operations
  HSAIL_ASM::InstBasic EmitClearDetectExcept(uint32_t exceptionNumber);
  HSAIL_ASM::InstBasic EmitGetDetectExcept(TypedReg dest);
  HSAIL_ASM::InstBasic EmitSetDetectExcept(uint32_t exceptionNumber);

  // Dispatch packet operations
  TypedReg EmitCurrentWorkgroupSize(uint32_t dim);
  TypedReg EmitDim();
  TypedReg EmitGridGroups(uint32_t dim);
  TypedReg EmitGridSize(uint32_t dim);
  TypedReg EmitPacketCompletionSig();
  TypedReg EmitPacketId();
  TypedReg EmitWorkgroupId(uint32_t dim);
  TypedReg EmitWorkgroupSize(uint32_t dim);
  TypedReg EmitWorkitemAbsId(uint32_t dim, bool large);
  TypedReg EmitWorkitemFlatAbsId(bool large);
  TypedReg EmitWorkitemFlatId();
  TypedReg EmitWorkitemId(uint32_t dim);
  TypedReg EmitCurrentWorkitemFlatId();

  // Cached (once per kernel) geometry properties.
  TypedReg WorkitemFlatAbsId(bool large);

  // Miscallaneous operations
  void EmitCuid(TypedReg dest);
  void EmitKernargBasePtr(PointerReg dest);
  void EmitGroupBasePtr(PointerReg dest);
  void EmitLaneid(TypedReg dest);
  void EmitMaxcuid(TypedReg dest);
  void EmitMaxwaveid(TypedReg dest);
  void EmitNop();
  void EmitClock(TypedReg dest);
  void EmitWaveid(TypedReg dest);

  std::string GetVariableNameHere(const std::string& name);
  BrigLinkage GetVariableLinkageHere();
  EmitterScope Scope() const { return currentScope; }
};

}
}

#endif // BRIG_EMITTER_HPP
