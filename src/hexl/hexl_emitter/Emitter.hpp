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

#ifndef HEXL_EMITTER_HPP
#define HEXL_EMITTER_HPP

#include "Arena.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include "MObject.hpp"
#include "HSAILItems.h"
#include "EmitterCommon.hpp"
#include "Grid.hpp"
#include "MObject.hpp"
#include "HexlTest.hpp"
#include "Scenario.hpp"
#include "Sequence.hpp"
#include "CoreConfig.hpp"
#include "Utils.hpp"

namespace hexl {

namespace Bools {
  hexl::Sequence<bool>* All();
  hexl::Sequence<bool>* Value(bool val);
}

std::string Dir2Str(Brig::BrigControlDirective d);

template <>
inline const char *EmptySequenceName<Brig::BrigControlDirective>() { return "ND"; }

template <>
inline void PrintSequenceItem<Brig::BrigControlDirective>(std::ostream& out, const Brig::BrigControlDirective& d) {
  out << Dir2Str(d);
}

namespace emitter {

Sequence<Location>* CodeLocations();
Sequence<Location>* KernelLocation();

enum BufferType {
  HOST_INPUT_BUFFER,
  HOST_RESULT_BUFFER,
  MODULE_BUFFER,
  KERNEL_BUFFER,
};

class EmitterObject {
  ARENAMEM

protected:
  EmitterObject(const EmitterObject& other) { }

public:
  EmitterObject() { }
  virtual void Name(std::ostream& out) const { assert(0); }
  virtual void Print(std::ostream& out) const { Name(out); }
  virtual ~EmitterObject() { }
};

class Emittable : public EmitterObject {
protected:
  TestEmitter* te;

public:
  explicit Emittable(TestEmitter* te_ = 0)
    : te(te_) { }
  virtual ~Emittable() { }

  Grid Geometry();

  virtual void Reset(TestEmitter* te) { this->te = te; }

  virtual bool IsValid() const { return true; }

  virtual void Name(std::ostream& out) const { }

  virtual void Test() { }

  virtual void Init() { }
  virtual void Finish() { }

  virtual void StartProgram() { }
  virtual void EndProgram() { }

  virtual void StartModule() { }
  virtual void ModuleDirectives() { }
  virtual void ModuleVariables() { }
  virtual void EndModule() { }

  virtual void StartFunction() { }
  virtual void FunctionFormalOutputArguments() { }
  virtual void FunctionFormalInputArguments() { }
  virtual void StartFunctionBody() { }
  virtual void FunctionDirectives() { }
  virtual void FunctionVariables() { }
  virtual void FunctionInit() { }
  virtual void EndFunction() { }

  virtual void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs) { }

  virtual void StartKernel() { }
  virtual void KernelArguments() { }
  virtual void StartKernelBody() { }
  virtual void KernelDirectives() { }
  virtual void KernelVariables() { }
  virtual void KernelInit() { }
  virtual void EndKernel() { }

  virtual void SetupDispatch(DispatchSetup* dispatch) { }

  virtual void ScenarioInit() { }
  virtual void ScenarioCodes() { }
  virtual void ScenarioDispatches() { }
  virtual void ScenarioValidation() { }
  virtual void ScenarioEnd() { }
};

class ETypedReg : public EmitterObject {
public:
  ETypedReg()
    : type(Brig::BRIG_TYPE_NONE) { }
  ETypedReg(Brig::BrigType16_t type_)
    : type(type_) { }
  ETypedReg(HSAIL_ASM::OperandReg reg, Brig::BrigType16_t type_)
    : type(type_) { Add(reg); }

  HSAIL_ASM::OperandReg Reg() const { assert(Count() == 1); return regs[0]; }
  HSAIL_ASM::OperandReg Reg(size_t i) const { return regs[(int) i]; }
  HSAIL_ASM::ItemList&  Regs() { return regs; }
  const HSAIL_ASM::ItemList& Regs() const { return regs; }
  Brig::BrigType16_t Type() const { return type; }
  unsigned TypeSizeBytes() const { return HSAIL_ASM::getBrigTypeNumBytes(type); }
  unsigned TypeSizeBits() const { return HSAIL_ASM::getBrigTypeNumBits(type); }
  size_t Count() const { return regs.size(); }
  void Add(HSAIL_ASM::OperandReg reg) { regs.push_back(reg); }

private:
  HSAIL_ASM::ItemList regs;
  Brig::BrigType16_t type;
};

class ETypedRegList : public EmitterObject {
private:
  std::vector<TypedReg> tregs;

public:
  size_t Count() const { return tregs.size(); }
  TypedReg Get(size_t i) { return tregs[i]; }
  void Add(TypedReg treg) { tregs.push_back(treg); }
  void Clear() { tregs.clear(); }
};

class EPointerReg : public ETypedReg {
public:
  static Brig::BrigTypeX GetSegmentPointerType(Brig::BrigSegment8_t segment, bool large);

  EPointerReg(HSAIL_ASM::OperandReg reg_, Brig::BrigType16_t type_, Brig::BrigSegment8_t segment_)
    : ETypedReg(reg_, type_), segment(segment_) { }

  Brig::BrigSegment8_t Segment() const { return segment; }
  bool IsLarge() const { return Type() == Brig::BRIG_TYPE_U64; }

private:
  Brig::BrigSegment8_t segment;
};

class EVariableSpec : public Emittable {
protected:
  Location location;
  Brig::BrigSegment segment;
  Brig::BrigTypeX type;
  Brig::BrigAlignment align;
  uint64_t dim;
  bool isConst;
  bool output;

  bool IsValidVar() const;
  bool IsValidAt(Location location) const;

public:
  EVariableSpec(Brig::BrigSegment segment_, Brig::BrigTypeX type_, Location location_ = AUTO, Brig::BrigAlignment align_ = Brig::BRIG_ALIGNMENT_NONE, uint64_t dim_ = 0, bool isConst_ = false, bool output_ = false)
    : location(location_), segment(segment_), type(type_), align(align_), dim(dim_), isConst(isConst_), output(output_) { }
  EVariableSpec(const EVariableSpec& spec, bool output_)
    : location(spec.location), segment(spec.segment), type(spec.type), align(spec.align), dim(spec.dim), isConst(spec.isConst), output(output_) { }

  bool IsValid() const;
  void Name(std::ostream& out) const;
  Brig::BrigSegment Segment() const { return segment; }
  Brig::BrigTypeX Type() const { return type; }
  ValueType VType() const { return Brig2ValueType(type); }
  Brig::BrigAlignment Align() const { return align; }
  unsigned AlignNum() const { return HSAIL_ASM::align2num(align); }
  uint64_t Dim() const { return dim; }
  uint32_t Dim32() const { assert(dim <= UINT32_MAX); return (uint32_t) dim; }
  uint32_t Count() const { return (std::max)(Dim32(), (uint32_t) 1); }
  bool IsArray() const { return dim > 0; }
};


class EVariable : public EVariableSpec {
private:
  std::string id;
  HSAIL_ASM::DirectiveVariable var;
  HSAIL_ASM::ArbitraryData data;

  Location RealLocation() const;

public:
  EVariable(TestEmitter* te_, const std::string& id_, Brig::BrigSegment segment_, Brig::BrigTypeX type_, Location location_, Brig::BrigAlignment align_ = Brig::BRIG_ALIGNMENT_NONE, uint64_t dim_ = 0, bool isConst_ = false, bool output_ = false)
    : EVariableSpec(segment_, type_, location_, align_, dim_, isConst_, output_), id(id_) { te = te_;  }
  EVariable(TestEmitter* te_, const std::string& id_, const EVariableSpec* spec_)
    : EVariableSpec(*spec_), id(id_) { te = te_; }
  EVariable(TestEmitter* te_, const std::string& id_, const EVariableSpec* spec_, bool output)
    : EVariableSpec(*spec_, output), id(id_) { te = te_; }

  std::string VariableName() const;
  HSAIL_ASM::DirectiveVariable Variable() { assert(var != 0); return var; }

  template<typename T>
  void PushBack(T val) {
    if (!Is128Bit(type)) {
      assert(sizeof(T) == HSAIL_ASM::getBrigTypeNumBytes(type));    
    } else {
      assert(sizeof(T) == HSAIL_ASM::getBrigTypeNumBytes(type) / 2);    
    }
    data.push_back(val);
  }

  template<typename T>
  void WriteData(T val, size_t pos) {
    if (!Is128Bit(type)) {
      assert(sizeof(T) == HSAIL_ASM::getBrigTypeNumBytes(type));    
    } else {
      assert(sizeof(T) == HSAIL_ASM::getBrigTypeNumBytes(type) / 2);    
    }
    data.write(val, pos);
  }

  void Name(std::ostream& out) const;

  TypedReg AddDataReg();
  void EmitDefinition();
  void EmitInitializer();
  void EmitLoadTo(TypedReg dst, bool useVectorInstructions = true);
  void EmitStoreFrom(TypedReg src, bool useVectorInstructions = true);

  void ModuleVariables();
  void FunctionFormalOutputArguments();
  void FunctionFormalInputArguments();
  void FunctionVariables();
  void KernelArguments();
  void KernelVariables();
};

class EAddressSpec : public Emittable {
protected:
  VariableSpec varSpec;

public:
  Brig::BrigTypeX Type() { return varSpec->Type(); }
  ValueType VType() { return varSpec->VType(); }
};

class EAddress : public EAddressSpec {
public:
  struct Spec {
    VariableSpec varSpec;
    bool hasOffset;
    bool hasRegister;
  };

private:
  Spec spec;
  Variable var;

public:
  HSAIL_ASM::OperandAddress Address();
};


class EControlDirectives : public Emittable {
private:
  const hexl::Sequence<Brig::BrigControlDirective>* spec;

  void Emit();

public:
  EControlDirectives(const hexl::Sequence<Brig::BrigControlDirective>* spec_)
    : spec(spec_) { }

  void Name(std::ostream& out) const;

  const hexl::Sequence<Brig::BrigControlDirective>* Spec() const { return spec; }
  bool Has(Brig::BrigControlDirective d) const { return spec->Has(d); }
  void FunctionDirectives();
  void KernelDirectives();
};

class EmittableContainer : public Emittable {
private:
  std::vector<Emittable*> list;

public:
  EmittableContainer(TestEmitter* te = 0)
    : Emittable(te) { } 

  void Add(Emittable* e) { list.push_back(e); }
  void Name(std::ostream& out) const;
  void Reset(TestEmitter* te) { Emittable::Reset(te); for (Emittable* e : list) { e->Reset(te); } }

  void Init() { for (Emittable* e : list) { e->Init(); } }
  void StartModule() { for (Emittable* e : list) { e->StartModule(); } }
  void ModuleVariables() { for (Emittable* e : list) { e->ModuleVariables(); } }
  void EndModule() { for (Emittable* e : list) { e->EndModule(); } }

  void FunctionFormalInputArguments() { for (Emittable* e : list) { e->FunctionFormalInputArguments(); } }
  void FunctionFormalOutputArguments() { for (Emittable* e : list) { e->FunctionFormalOutputArguments(); } }
  void FunctionVariables() { for (Emittable* e : list) { e->FunctionVariables(); } }
  void FunctionDirectives() { for (Emittable* e : list) { e->FunctionDirectives(); }}
  void FunctionInit() { for (Emittable* e : list) { e->FunctionInit(); }}
  void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs) { for (Emittable* e : list) { e->ActualCallArguments(inputs, outputs); } }

  void KernelArguments() { for (Emittable* e : list) { e->KernelArguments(); } }
  void KernelVariables() { for (Emittable* e : list) { e->KernelVariables(); } }
  void KernelDirectives() { for (Emittable* e : list) { e->KernelDirectives(); }}
  void KernelInit() { for (Emittable* e : list) { e->KernelInit(); }}
  void StartKernelBody() { for (Emittable* e : list) { e->StartKernelBody(); } }

  void SetupDispatch(DispatchSetup* dispatch) { for (Emittable* e : list) { e->SetupDispatch(dispatch); } }
  void ScenarioInit() { for (Emittable* e : list) { e->ScenarioInit(); } }
  void ScenarioCodes() { for (Emittable* e : list) { e->ScenarioCodes(); } }
  void ScenarioDispatches() { for (Emittable* e : list) { e->ScenarioDispatches(); } }

  void ScenarioEnd() { for (Emittable* e : list) { e->ScenarioEnd(); } }

  Variable NewVariable(const std::string& id, Brig::BrigSegment segment, Brig::BrigTypeX type, Location location = AUTO, Brig::BrigAlignment align = Brig::BRIG_ALIGNMENT_NONE, uint64_t dim = 0, bool isConst = false, bool output = false);
  Variable NewVariable(const std::string& id, VariableSpec varSpec);
  Variable NewVariable(const std::string& id, VariableSpec varSpec, bool output);
  Buffer NewBuffer(const std::string& id, BufferType type, ValueType vtype, size_t count);
  UserModeQueue NewQueue(const std::string& id, UserModeQueueType type);
  Kernel NewKernel(const std::string& id);
  Function NewFunction(const std::string& id);
};

class EBuffer : public Emittable {
private:
  std::string id;
  BufferType type;
  ValueType vtype;
  size_t count;
  std::unique_ptr<Values> data;
  HSAIL_ASM::DirectiveVariable variable;
  PointerReg address[2];
  PointerReg dataOffset;

  HSAIL_ASM::DirectiveVariable EmitAddressDefinition(Brig::BrigSegment segment);
  void EmitBufferDefinition();

  HSAIL_ASM::OperandAddress DataAddress(TypedReg index, bool flat = false, uint64_t count = 1);

public:
  EBuffer(TestEmitter* te, const std::string& id_, BufferType type_, ValueType vtype_, size_t count_)
    : Emittable(te), id(id_), type(type_), vtype(vtype_), count(count_), data(new Values()), dataOffset(0) {
    address[0] = 0; address[1] = 0;
  }

  std::string IdData() const { return id + ".data"; }
  void AddData(Value v) { data->push_back(v); }
  void SetData(Values* values) { data.reset(values); }
  Values* ReleaseData() { return data.release(); }

  HSAIL_ASM::DirectiveVariable Variable();
  PointerReg Address(bool flat = false);

  size_t Count() const { return count; }
  size_t TypeSize() const { return ValueTypeSize(vtype); }
  size_t Size() const;

  void KernelArguments();
  void KernelVariables();
  void SetupDispatch(DispatchSetup* dsetup);
  void ScenarioInit();
  void Validation();

  TypedReg AddDataReg();
  PointerReg AddAReg(bool flat = false);
  void EmitLoadData(TypedReg dest, TypedReg index, bool flat = false);
  void EmitLoadData(TypedReg dest, bool flat = false);
  void EmitStoreData(TypedReg src, TypedReg index, bool flat = false);
  void EmitStoreData(TypedReg src, bool flat = false);
};

class EUserModeQueue : public Emittable {
private:
  std::string id;
  UserModeQueueType type;
  HSAIL_ASM::DirectiveVariable queueKernelArg;
  PointerReg address;
  PointerReg flatAddress;
  PointerReg serviceQueue;
  TypedReg doorbellSignal;
  TypedReg size;
  PointerReg baseAddress;

public:
  EUserModeQueue(TestEmitter* te, const std::string& id_, UserModeQueueType type_)
    : Emittable(te), id(id_), type(type_), address(0), flatAddress(0), doorbellSignal(0), size(0), baseAddress(0) { }
  EUserModeQueue(TestEmitter* te, const std::string& id_, PointerReg address_ = 0)
    : Emittable(te), id(id_), type(USER_PROVIDED), address(address_), flatAddress(0), doorbellSignal(0), size(0), baseAddress(0) { }

//  void Name(std::ostream& out) { out << UserModeQueueType2Str(type); }

  PointerReg Address(Brig::BrigSegment segment = Brig::BRIG_SEGMENT_GLOBAL);

  void KernelArguments();
  void StartKernelBody();
  void SetupDispatch(hexl::DispatchSetup* setup);
  void ScenarioInit();

  void EmitLdQueueReadIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg dest);
  void EmitLdQueueWriteIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg dest);
  void EmitStQueueReadIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg src);
  void EmitStQueueWriteIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg src);  
  void EmitAddQueueWriteIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg dest, HSAIL_ASM::Operand src);
  void EmitCasQueueWriteIndex(Brig::BrigSegment segment, Brig::BrigMemoryOrder memoryOrder, TypedReg dest, HSAIL_ASM::Operand src0, HSAIL_ASM::Operand src1);

  TypedReg DoorbellSignal();
  TypedReg EmitLoadDoorbellSignal();
  TypedReg Size();
  TypedReg EmitLoadSize();
  PointerReg ServiceQueue();
  PointerReg EmitLoadServiceQueue();
  PointerReg BaseAddress();
  PointerReg EmitLoadBaseAddress();
};

class ESignal : public Emittable {
private:
  std::string id;
  uint64_t initialValue;
  HSAIL_ASM::DirectiveVariable kernelArg;

public:
  ESignal(TestEmitter* te, const std::string& id_, uint64_t initialValue_)
    : Emittable(te), id(id_), initialValue(initialValue_) { }

  const std::string& Id() const { return id; }
  HSAIL_ASM::DirectiveVariable KernelArg() { assert(kernelArg != 0); return kernelArg; }

  void ScenarioInit();
  void KernelArguments();
  void SetupDispatch(DispatchSetup* dispatch);

  TypedReg AddReg();
  TypedReg AddValueReg();
};

class EKernel : public EmittableContainer {
private:
  std::string id;
  HSAIL_ASM::DirectiveKernel kernel;

public:
  EKernel(TestEmitter* te, const std::string& id_)
    : EmittableContainer(te), id(id_) { }

  std::string KernelName() const { return "&" + id; }
  HSAIL_ASM::DirectiveKernel Directive() { assert(kernel != 0); return kernel; }
  HSAIL_ASM::Offset BrigOffset() { return Directive().brigOffset(); }
  void StartKernel();
  void StartKernelBody();
  void EndKernel();
};

class EFunction : public EmittableContainer {
private:
  std::string id;
  HSAIL_ASM::DirectiveFunction function;

public:
  EFunction(TestEmitter* te, const std::string& id_)
    : EmittableContainer(te), id(id_) { }

  std::string FunctionName() const { return "&" + id; }
  HSAIL_ASM::DirectiveFunction Directive() { assert(function != 0); return function; }
  HSAIL_ASM::Offset BrigOffset() { return Directive().brigOffset(); }
  void StartFunction();
  void StartFunctionBody();
  void EndFunction();
};

const char* ConditionType2Str(ConditionType type);
const char* ConditionInput2Str(ConditionInput input);

class ECondition : public Emittable {
private:
  std::string id;
  ConditionType type;
  ConditionInput input;
  Brig::BrigTypeX itype;
  Brig::BrigWidth width;

  HSAIL_ASM::DirectiveVariable kernarg, funcarg;
  TypedReg kerninp, funcinp;
  Buffer condBuffer;
  std::string lThen, lElse, lEnd;
  std::vector<std::string> labels;
  
  std::string KernargName();
  TypedReg InputData();
  
  std::string Id();

public:
  ECondition(ConditionType type_, ConditionInput input_, Brig::BrigWidth width_)
    : type(type_), input(input_), itype(Brig::BRIG_TYPE_U32), width(width_),
      kerninp(0), funcinp(0), condBuffer(0)
      { }
  ECondition(ConditionType type_, ConditionInput input_, Brig::BrigTypeX itype_, Brig::BrigWidth width_)
    : type(type_), input(input_), itype(itype_), width(width_),
      kerninp(0), funcinp(0), condBuffer(0)
      { }
    
  ConditionInput Input() { return input; }

  void Name(std::ostream& out) const;
  void Reset(TestEmitter* te);

  void Init();
  void KernelArguments();
  void KernelVariables();
  void KernelInit();
  void FunctionFormalInputArguments();
  void FunctionInit();
  void SetupDispatch(DispatchSetup* dsetup);
  void ScenarioInit();
  void Validation();
  void ActualCallArguments(TypedRegList inputs, TypedRegList outputs);

  bool IsTrueFor(uint64_t wi);
  bool IsTrueFor(const Dim& point) { return IsTrueFor(Geometry()->WorkitemFlatAbsId(point)); }

  HSAIL_ASM::Operand CondOperand();

  HSAIL_ASM::Operand EmitIfCond();
  void EmitIfThenStart();
  void EmitIfThenEnd();
  bool ExpectThenPath(uint64_t wi);
  bool ExpectThenPath(const Dim& point) { return ExpectThenPath(Geometry()->WorkitemFlatAbsId(point)); }

  void EmitIfThenElseStart();
  void EmitIfThenElseOtherwise();
  void EmitIfThenElseEnd();

  HSAIL_ASM::Operand EmitSwitchCond();
  void EmitSwitchStart();
  void EmitSwitchBranchStart(unsigned i);
  void EmitSwitchEnd();
  unsigned SwitchBranchCount();
  unsigned ExpectedSwitchPath(uint64_t i);

  uint32_t InputValue(uint64_t id, Brig::BrigWidth width = Brig::BRIG_WIDTH_NONE);
};

class BrigEmitter;
class CoreConfig;

class TestEmitter {
private:
  Arena ap;
  std::unique_ptr<BrigEmitter> be;
  std::unique_ptr<Context> initialContext;
  std::unique_ptr<hexl::scenario::Scenario> scenario;
  CoreConfig* coreConfig;

public:
  TestEmitter();

  void SetCoreConfig(CoreConfig* cc);

  Arena* Ap() { return &ap; }

  BrigEmitter* Brig() { return be.get(); }
  CoreConfig* CoreCfg() { return coreConfig; }

  Context* InitialContext() { return initialContext.get(); }
  Context* ReleaseContext() { return initialContext.release(); }
  hexl::scenario::Scenario* TestScenario() { return scenario.get(); }
  hexl::scenario::Scenario* ReleaseScenario() { return scenario.release(); }

  Variable NewVariable(const std::string& id, Brig::BrigSegment segment, Brig::BrigTypeX type, Location location = AUTO, Brig::BrigAlignment align = Brig::BRIG_ALIGNMENT_NONE, uint64_t dim = 0, bool isConst = false, bool output = false);
  Variable NewVariable(const std::string& id, VariableSpec varSpec);
  Variable NewVariable(const std::string& id, VariableSpec varSpec, bool output);
  Buffer NewBuffer(const std::string& id, BufferType type, ValueType vtype, size_t count);
  UserModeQueue NewQueue(const std::string& id, UserModeQueueType type);
  Signal NewSignal(const std::string& id, uint64_t initialValue);
  Kernel NewKernel(const std::string& id);
  Function NewFunction(const std::string& id);
};

}

class EmittedTestBase : public TestSpec {
protected:
  std::unique_ptr<hexl::Context> context;
  std::unique_ptr<emitter::TestEmitter> te;

public:
  EmittedTestBase()
    : context(new hexl::Context()),
      te(new hexl::emitter::TestEmitter()) { }

  void InitContext(hexl::Context* context) { this->context->SetParent(context); }
  hexl::Context* GetContext() { return context.get(); }

  Test* Create();

  virtual void Test() = 0;
};

class EmittedTest : public EmittedTestBase {
protected:
  hexl::emitter::CoreConfig* cc;
  emitter::Location codeLocation;
  Grid geometry;
  emitter::Buffer output;
  emitter::Kernel kernel;
  emitter::Function function;
  emitter::Variable functionResult;
  emitter::TypedReg functionResultReg;
    
public:
  EmittedTest(emitter::Location codeLocation_ = emitter::KERNEL, Grid geometry_ = 0);

  std::string CodeLocationString() const;

  virtual void Test();
  virtual void Init();
  virtual void KernelArgumentsInit();
  virtual void FunctionArgumentsInit();
  virtual void GeometryInit();

  virtual void Programs();
  virtual void Program();
  virtual void StartProgram();
  virtual void EndProgram();

  virtual void Modules();
  virtual void Module();
  virtual void StartModule();
  virtual void EndModule();
  virtual void ModuleDirectives();
  virtual void ModuleVariables();

  virtual void Executables();

  virtual void Kernel();
  virtual void StartKernel();
  virtual void EndKernel();
  virtual void KernelArguments();
  virtual void StartKernelBody();
  virtual void KernelDirectives();
  virtual void KernelVariables();
  virtual void KernelInit();
  virtual emitter::TypedReg KernelResult();
  virtual void KernelCode();

  virtual void SetupDispatch(DispatchSetup* dispatch);

  virtual void Function();
  virtual void StartFunction();
  virtual void FunctionFormalOutputArguments();
  virtual void FunctionFormalInputArguments();
  virtual void StartFunctionBody();
  virtual void FunctionDirectives();
  virtual void FunctionVariables();
  virtual void FunctionInit();
  virtual void FunctionCode();
  virtual void EndFunction();

  virtual void ActualCallArguments(emitter::TypedRegList inputs, emitter::TypedRegList outputs);

  virtual Brig::BrigTypeX ResultType() const { assert(0); return Brig::BRIG_TYPE_NONE; }
  virtual uint64_t ResultDim() const { return 0; }
  uint32_t ResultCount() const { assert(ResultDim() < UINT32_MAX); return (std::max)((uint32_t) ResultDim(), (uint32_t) 1); }
  bool IsResultType(Brig::BrigTypeX type) const { return ResultType() == type; }
  ValueType ResultValueType() const { return Brig2ValueType(ResultType()); }
  virtual emitter::TypedReg Result() { assert(0); return 0; }
  virtual size_t OutputBufferSize() const;
  virtual Value ExpectedResult() const { assert(0); return Value(MV_UINT64, 0); }
  virtual Value ExpectedResult(uint64_t id, uint64_t pos) const { return ExpectedResult(id); }
  virtual Value ExpectedResult(uint64_t id) const { return ExpectedResult(); }
  virtual Values* ExpectedResults() const;
  virtual void ExpectedResults(Values* result) const;

  virtual void Scenario();
  virtual void ScenarioInit();
  virtual void ScenarioCodes();
  virtual void ScenarioDispatches();
  virtual void ScenarioValidation();
  virtual void ScenarioEnd();
  virtual void Finish();
};

inline std::ostream& operator<<(std::ostream& out, const hexl::emitter::EmitterObject& o) { o.Name(out); return out; }
inline std::ostream& operator<<(std::ostream& out, const hexl::emitter::EmitterObject* o) { o->Name(out); return out; }

}

#endif // HEXL_EMITTER_HPP
