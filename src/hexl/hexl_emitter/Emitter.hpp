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
#include "Image.hpp"

namespace hexl {

namespace Bools {
  hexl::Sequence<bool>* All();
  hexl::Sequence<bool>* Value(bool val);
}

std::string Dir2Str(BrigControlDirective d);

template <>
inline const char *EmptySequenceName<BrigControlDirective>() { return "ND"; }

template <>
inline void PrintSequenceItem<BrigControlDirective>(std::ostream& out, const BrigControlDirective& d) {
  out << Dir2Str(d);
}

namespace emitter {

Sequence<Location>* CodeLocations();
Sequence<Location>* KernelLocation();

BrigLinkage Location2Linkage(Location loc);

enum BufferType {
  HOST_INPUT_BUFFER,
  HOST_RESULT_BUFFER,
  MODULE_BUFFER,
  KERNEL_BUFFER,
};

class EString {
private:
  typedef std::basic_string<char, std::char_traits<char>, ArenaAllocator<char>> estring;
  estring s;

public:
  EString(const std::string& s_, ArenaAllocator<char> allocator)
    : s(s_.c_str(), allocator) { }

  const char *c_str() const { return s.c_str(); }
  std::string str() const { return s.c_str(); }
  operator const char *() const { return s.c_str(); }
};

//typedef std::basic_string<char, std::char_traits<char>, ArenaAllocator<char>> EString;

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

  virtual void ScenarioInit() { }
  virtual void ScenarioCodes() { }
  virtual void ScenarioDispatch() { }
  virtual void SetupDispatch(const std::string& dispatchId) { } 
  virtual void ScenarioValidation() { }
  virtual void ScenarioEnd() { }
};

class ETypedReg : public EmitterObject {
  static const unsigned MAX_REGS = 8;
public:
  ETypedReg(unsigned count = MAX_REGS)
    : type(BRIG_TYPE_NONE), count(0) { }
  ETypedReg(BrigType16_t type_)
    : type(type_), count(0) { }
  ETypedReg(HSAIL_ASM::OperandRegister reg, BrigType16_t type_)
    : type(type_), count(0) { Add(reg); }

  HSAIL_ASM::OperandRegister Reg() const { assert(Count() == 1); return regs[0]; }
  HSAIL_ASM::OperandRegister Reg(size_t i) const { return regs[(int) i]; }
  HSAIL_ASM::OperandOperandList CreateOperandList(HSAIL_ASM::Brigantine& be);
  BrigType16_t Type() const { return type; }
  unsigned TypeSizeBytes() const { return HSAIL_ASM::getBrigTypeNumBytes(type); }
  unsigned TypeSizeBits() const { return HSAIL_ASM::getBrigTypeNumBits(type); }
  unsigned RegSizeBytes() const { return (std::max)(TypeSizeBytes(), (unsigned) 4); }
  unsigned RegSizeBits() const { return (std::max)(TypeSizeBits(), (unsigned) 32); }
  size_t Count() const { return count; }
  void Add(HSAIL_ASM::OperandRegister reg) { assert(count < MAX_REGS); regs[count++] = reg; }

private:
  BrigType16_t type;
  size_t count;
  HSAIL_ASM::OperandRegister regs[MAX_REGS];
};

class ETypedRegList : public EmitterObject {
private:
  std::vector<TypedReg, ArenaAllocator<TypedReg>> tregs;

public:
  ETypedRegList(Arena* ap);
  size_t Count() const { return tregs.size(); }
  TypedReg Get(size_t i) { return tregs[i]; }
  void Add(TypedReg treg) { tregs.push_back(treg); }
  void Clear() { tregs.clear(); }
};

class EPointerReg : public ETypedReg {
public:
  static BrigType GetSegmentPointerType(BrigSegment8_t segment, bool large);

  EPointerReg(HSAIL_ASM::OperandRegister reg_, BrigType16_t type_, BrigSegment8_t segment_)
    : ETypedReg(reg_, type_), segment(segment_) { }

  BrigSegment8_t Segment() const { return segment; }
  bool IsLarge() const { return Type() == BRIG_TYPE_U64; }

private:
  BrigSegment8_t segment;
};

class EVariableSpec : public Emittable {
protected:
  Location location;
  BrigSegment segment;
  BrigType type;
  BrigAlignment align;
  uint64_t dim;
  bool isConst;
  bool output;

  bool IsValidVar() const;
  bool IsValidAt(Location location) const;

public:
  EVariableSpec(BrigSegment segment_, BrigType type_, Location location_ = AUTO, BrigAlignment align_ = BRIG_ALIGNMENT_NONE, uint64_t dim_ = 0, bool isConst_ = false, bool output_ = false)
    : location(location_), segment(segment_), type(type_), align(align_), dim(dim_), isConst(isConst_), output(output_) { }
  EVariableSpec(const EVariableSpec& spec, bool output_)
    : location(spec.location), segment(spec.segment), type(spec.type), align(spec.align), dim(spec.dim), isConst(spec.isConst), output(output_) { }

  bool IsValid() const;
  void Name(std::ostream& out) const;
  BrigSegment Segment() const { return segment; }
  BrigType Type() const { return type; }
  ValueType VType() const { return Brig2ValueType(type); }
  BrigAlignment Align() const { return align; }
  unsigned AlignNum() const { return HSAIL_ASM::align2num(align); }
  uint64_t Dim() const { return dim; }
  uint32_t Dim32() const { assert(dim <= UINT32_MAX); return (uint32_t) dim; }
  uint32_t Count() const { return (std::max)(Dim32(), (uint32_t) 1); }
  bool IsArray() const { return dim > 0; }
};


class EVariable : public EVariableSpec {
private:
  EString id;
  HSAIL_ASM::DirectiveVariable var;
  std::unique_ptr<hexl::Values> data;

  Location RealLocation() const;

public:
  EVariable(TestEmitter* te_, const std::string& id_,
    BrigSegment segment_, BrigType type_, Location location_, BrigAlignment align_ = BRIG_ALIGNMENT_NONE, uint64_t dim_ = 0, bool isConst_ = false, bool output_ = false);
  EVariable(TestEmitter* te_, const std::string& id_, const EVariableSpec* spec_);
  EVariable(TestEmitter* te_, const std::string& id_, const EVariableSpec* spec_, bool output);

  std::string VariableName() const;
  HSAIL_ASM::DirectiveVariable Variable() { assert(var != 0); return var; }

  void AddData(hexl::Value val);

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

  void SetupDispatch(const std::string& dispatchId);
};

class EmittableWithId : public Emittable {
protected:
  EString id;
public:
  EmittableWithId(TestEmitter* te, const std::string& id);

  const char *Id() const { return id.c_str(); }
};

class EFBarrier : public EmittableWithId {
private:
  Location location;
  bool output;
  HSAIL_ASM::DirectiveFbarrier fb;

public:
  EFBarrier(TestEmitter* te, const std::string& id, Location location = Location::KERNEL, bool output = false);

  std::string FBarrierName() const;
  HSAIL_ASM::DirectiveFbarrier FBarrier() const { assert(fb); return fb; }

  void EmitDefinition();
  void EmitInitfbar();
  void EmitInitfbarInFirstWI();
  void EmitJoinfbar();
  void EmitWaitfbar();
  void EmitArrivefbar();
  void EmitLeavefbar();
  void EmitReleasefbar();
  void EmitReleasefbarInFirstWI();
  void EmitLdf(TypedReg dest);

  void Name(std::ostream& out) const override;

  void ModuleVariables() override;
  void FunctionFormalOutputArguments() override;
  void FunctionFormalInputArguments() override;
  void FunctionVariables() override;
  void KernelVariables() override;
};


class EAddressSpec : public Emittable {
protected:
  VariableSpec varSpec;

public:
  BrigType Type() { return varSpec->Type(); }
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
  const hexl::Sequence<BrigControlDirective>* spec;

  void Emit();

public:
  EControlDirectives(const hexl::Sequence<BrigControlDirective>* spec_)
    : spec(spec_) { }

  void Name(std::ostream& out) const;

  const hexl::Sequence<BrigControlDirective>* Spec() const { return spec; }
  bool Has(BrigControlDirective d) const { return spec->Has(d); }
  void FunctionDirectives();
  void KernelDirectives();
};

class EmittableContainer : public Emittable {
private:
  std::vector<Emittable*, ArenaAllocator<Emittable*>> list;

public:
  EmittableContainer(TestEmitter* te);

  void Add(Emittable* e) { list.push_back(e); }
  void Clear() { list.clear(); }
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

  void ScenarioInit() { for (Emittable* e : list) { e->ScenarioInit(); } }
  void ScenarioCodes() { for (Emittable* e : list) { e->ScenarioCodes(); } }
  void ScenarioDispatch() { for (Emittable* e : list) { e->ScenarioDispatch(); } }
  void SetupDispatch(const std::string& dispatchId) override { for (Emittable* e : list) { e->SetupDispatch(dispatchId); } }
  void ScenarioValidation() { for (Emittable* e : list) { e->ScenarioValidation(); } }
  void ScenarioEnd() { for (Emittable* e : list) { e->ScenarioEnd(); } }

  Variable NewVariable(const std::string& id, BrigSegment segment, BrigType type, Location location = AUTO, BrigAlignment align = BRIG_ALIGNMENT_NONE, uint64_t dim = 0, bool isConst = false, bool output = false);
  Variable NewVariable(const std::string& id, VariableSpec varSpec);
  Variable NewVariable(const std::string& id, VariableSpec varSpec, bool output);
  FBarrier NewFBarrier(const std::string& id, Location location = Location::KERNEL, bool output = false);
  Buffer NewBuffer(const std::string& id, BufferType type, ValueType vtype, size_t count);
  UserModeQueue NewQueue(const std::string& id, UserModeQueueType type);
  Signal NewSignal(const std::string& id, uint64_t initialValue);
  Kernel NewKernel(const std::string& id);
  Function NewFunction(const std::string& id);
  Image NewImage(const std::string& id, ImageSpec spec);
  Sampler NewSampler(const std::string& id, SamplerSpec spec);
  Dispatch NewDispatch(const std::string& id, const std::string& executableId, const std::string& kernelName);
};

class EmittableContainerWithId : public EmittableContainer {
protected:
  EString id;
public:
  EmittableContainerWithId(TestEmitter* te, const std::string& id);

  const char *Id() const { return id.c_str(); }
};


class EBuffer : public EmittableWithId {
private:
  BufferType type;
  ValueType vtype;
  size_t count;
  std::unique_ptr<Values> data;
  HSAIL_ASM::DirectiveVariable variable;
  PointerReg address[2];
  PointerReg dataOffset;
  std::string comparisonMethod;

  HSAIL_ASM::DirectiveVariable EmitAddressDefinition(BrigSegment segment);
  void EmitBufferDefinition();

  HSAIL_ASM::OperandAddress DataAddress(TypedReg index, bool flat = false, uint64_t count = 1);

public:
  EBuffer(TestEmitter* te, const std::string& id_, BufferType type_, ValueType vtype_, size_t count_)
    : EmittableWithId(te, id_), type(type_), vtype(vtype_), count(count_), data(new Values()), dataOffset(0) {
    address[0] = 0; address[1] = 0;
  }

  std::string IdData() const { return std::string(Id()) + ".data"; }
  std::string BufferName() const { return Id(); }
  void AddData(Value v) { data->push_back(v); }
  void SetData(Values* values) { data.reset(values); }
  Values* ReleaseData() { return data.release(); }
  void SetComparisonMethod(const std::string& comparisonMethod) { this->comparisonMethod = comparisonMethod; }

  HSAIL_ASM::DirectiveVariable Variable();
  PointerReg Address(bool flat = false);

  size_t Count() const { return count; }
  ValueType VType() const { return vtype; }
  size_t TypeSize() const { return ValueTypeSize(vtype); }
  size_t Size() const;

  void KernelArguments();
  void KernelVariables();
  void SetupDispatch(const std::string& dispatchId);
  void ScenarioInit();
  void ScenarioValidation();

  TypedReg AddDataReg();
  PointerReg AddAReg(bool flat = false);
  void EmitLoadData(TypedReg dest, TypedReg index, bool flat = false);
  void EmitLoadData(TypedReg dest, bool flat = false);
  void EmitStoreData(TypedReg src, TypedReg index, bool flat = false);
  void EmitStoreData(TypedReg src, bool flat = false);
};

class EUserModeQueue : public EmittableWithId {
private:
  UserModeQueueType type;
  HSAIL_ASM::DirectiveVariable queueKernelArg;
  PointerReg address;
  PointerReg serviceQueue;
  TypedReg doorbellSignal;
  TypedReg size;
  PointerReg baseAddress;

public:
  EUserModeQueue(TestEmitter* te, const std::string& id_, UserModeQueueType type_)
    : EmittableWithId(te, id_), type(type_), address(0), doorbellSignal(0), size(0), baseAddress(0) { }
  EUserModeQueue(TestEmitter* te, const std::string& id_, PointerReg address_ = 0)
    : EmittableWithId(te, id_), type(USER_PROVIDED), address(address_), doorbellSignal(0), size(0), baseAddress(0) { }

//  void Name(std::ostream& out) { out << UserModeQueueType2Str(type); }

  PointerReg Address(BrigSegment segment = BRIG_SEGMENT_GLOBAL);

  void KernelArguments();
  void StartKernelBody();
  void SetupDispatch(const std::string& dispatchId);
  void ScenarioInit();

  void EmitLdQueueReadIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest);
  void EmitLdQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest);
  void EmitStQueueReadIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg src);
  void EmitStQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg src);  
  void EmitAddQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest, HSAIL_ASM::Operand src);
  void EmitCasQueueWriteIndex(BrigSegment segment, BrigMemoryOrder memoryOrder, TypedReg dest, HSAIL_ASM::Operand src0, HSAIL_ASM::Operand src1);

  TypedReg DoorbellSignal();
  TypedReg EmitLoadDoorbellSignal();
  TypedReg Size();
  TypedReg EmitLoadSize();
  PointerReg ServiceQueue();
  PointerReg EmitLoadServiceQueue();
  PointerReg BaseAddress();
  PointerReg EmitLoadBaseAddress();
};

class ESignal : public EmittableWithId {
private:
  uint64_t initialValue;
  HSAIL_ASM::DirectiveVariable kernelArg;
  TypedReg handle;

public:
  ESignal(TestEmitter* te, const std::string& id_, uint64_t initialValue_)
    : EmittableWithId(te, id_), initialValue(initialValue_), handle(0) { }

  HSAIL_ASM::DirectiveVariable KernelArg() { assert(kernelArg != 0); return kernelArg; }

  void KernelArguments();
  void ScenarioInit();
  void SetupDispatch(const std::string& dispatchId);

  TypedReg AddReg();
  TypedReg AddValueReg();
  TypedReg Handle();
};

class EImageSpec : public EVariableSpec, protected ImageParams {
protected:
  bool IsValidSegment() const;
  bool IsValidType() const;
  
public:
  explicit EImageSpec(
    BrigSegment brigseg_ = BRIG_SEGMENT_GLOBAL, 
    BrigType imageType_ = BRIG_TYPE_RWIMG, 
    Location location_ = Location::KERNEL, 
    uint64_t dim_ = 0, 
    bool isConst_ = false, 
    bool output_ = false,
    BrigImageGeometry geometry_ = BRIG_GEOMETRY_1D,
    BrigImageChannelOrder channel_order_ = BRIG_CHANNEL_ORDER_A, 
    BrigImageChannelType channel_type_ = BRIG_CHANNEL_TYPE_SNORM_INT8,
    size_t width_ = 0, 
    size_t height_ = 0, 
    size_t depth_ = 0, 
    size_t array_size_ = 0,
    bool bLimitTest = false
    );

  bool IsValid() const;

  BrigImageGeometry Geometry() { return geometry; }
  BrigImageChannelOrder ChannelOrder() { return channelOrder; }
  BrigImageChannelType ChannelType() { return channelType; }
  size_t Width() { return width; }
  size_t Height() { return height; }
  size_t Depth() { return depth; }
  size_t ArraySize() { return arraySize; }
  bool IsLimitTest() { return bLimitTest; }
  void Geometry(BrigImageGeometry geometry_) { geometry = geometry_; }
  void ChannelOrder(BrigImageChannelOrder channelOrder_) { channelOrder = channelOrder_; }
  void ChannelType(BrigImageChannelType channelType_) { channelType = channelType_; }
  void Width(size_t width_) { width = width_; }
  void Height(size_t height_) { height = height_; }
  void Depth(size_t depth_) { depth = depth_; }
  void ArraySize(size_t arraySize_) { arraySize = arraySize_; }
  void LimitTest(bool bLimitTest_) { bLimitTest = bLimitTest_; }

  hexl::ImageGeometry ImageGeometry() { return hexl::ImageGeometry((unsigned)width, (unsigned)height, (unsigned)depth, (unsigned)arraySize); }
};

class EImageCalc {
private:
  ImageGeometry imageGeometry;
  BrigImageGeometry imageGeometryProp;
  BrigImageChannelOrder imageChannelOrder;
  BrigImageChannelType imageChannelType;
  BrigSamplerCoordNormalization samplerCoord;
  BrigSamplerFilter samplerFilter;
  BrigSamplerAddressing samplerAddressing;

  Value color_zero;
  Value color_one;
  Value existVal;
  int bits_per_channel;
  bool isSRGB;
  bool isDepth;

  void SetupDefaultColors();
  int32_t round_downi(float f) const; //todo make true rounding
  int32_t round_neari(float f) const; //todo make true rounding
  int32_t clamp_i(int32_t a, int32_t min, int32_t max) const;
  float clamp_f(float a, float min, float max) const;
  
  //Addressing
  double UnnormalizeCoord(Value* c, unsigned dimSize) const;
  double UnnormalizeArrayCoord(Value* c) const;
  uint32_t GetTexelIndex(double f, uint32_t dimSize) const;
  uint32_t GetTexelArrayIndex(double f, uint32_t dimSize) const;

  //Load
  Value ConvertRawData(uint32_t data) const;
  void LoadBorderData(Value* channels) const;
  void LoadColorData(uint32_t x_ind, uint32_t y_ind, uint32_t z_ind, Value* _color) const;
  void LoadTexel(uint32_t x_ind, uint32_t y_ind, uint32_t z_ind, Value* _color) const;
  void LoadFloatTexel(uint32_t x, uint32_t y, uint32_t z, double* const df) const;
  float SRGBtoLinearRGB(float f) const;
  uint32_t GetRawPixelData(uint32_t x_ind, uint32_t y_ind, uint32_t z_ind) const;
  uint32_t GetRawChannelData(uint32_t x_ind, uint32_t y_ind, uint32_t z_ind, uint32_t channel) const;
  int32_t SignExtend(uint32_t c, unsigned int bit_size) const;
  float    ConvertionLoadSignedNormalize(uint32_t c, unsigned int bit_size) const;
  float    ConvertionLoadUnsignedNormalize(uint32_t c, unsigned int bit_size) const;
  int32_t  ConvertionLoadSignedClamp(uint32_t c, unsigned int bit_size) const;
  uint32_t ConvertionLoadUnsignedClamp(uint32_t c, unsigned int bit_size) const;
  float    ConvertionLoadHalfFloat(uint32_t data) const;
  float    ConvertionLoadFloat(uint32_t data) const;

  //Store
  uint32_t ConvertionStoreSignedNormalize(float f, unsigned int bit_size) const;
  uint32_t ConvertionStoreUnsignedNormalize(float f, unsigned int bit_size) const;
  uint32_t ConvertionStoreSignedClamp(int32_t c, unsigned int bit_size) const;
  uint32_t ConvertionStoreUnsignedClamp(uint32_t c, unsigned int bit_size) const;
  uint32_t ConvertionStoreHalfFloat(float f) const;
  uint32_t ConvertionStoreFloat(float f) const;
  float LinearRGBtoSRGB(float f) const;
  Value PackChannelDataToMemoryFormat(Value* _color) const;

public:
  EImageCalc() { };
  
  void Init(EImage * eimage, ESampler* esampler);
  void ValueSet(Value val) { existVal = val; }
  void EmulateImageRd(Value* _coords, Value* _channels) const;
  void EmulateImageLd(Value* _coords, Value* _channels) const;
  Value EmulateImageSt(Value* _coords, Value* _channels) const;
};

class EImage : public EImageSpec {
private:
  EString id;
  HSAIL_ASM::DirectiveVariable var;
  std::unique_ptr<Values> data;
  EImageCalc calculator;

  HSAIL_ASM::DirectiveVariable EmitAddressDefinition(BrigSegment segment);
  void EmitInitializer();
  void EmitDefinition();

  Location RealLocation() const;

public:
  EImage(TestEmitter* te_, const std::string& id_, const EImageSpec* spec);
  ~EImage() {}

  const char *Id() const { return id; }
  std::string IdData() const { return id.str() + ".data"; }
  std::string IdParams() const { return id.str() + ".params"; }

  void KernelArguments();
  void ModuleVariables();
  void FunctionFormalOutputArguments();
  void FunctionFormalInputArguments();
  void FunctionVariables();
  void KernelVariables();

  void ScenarioInit();
  void SetupDispatch(const std::string& dispatchId);
  void EmitImageRd(TypedReg dest, TypedReg image, TypedReg sampler, TypedReg coord);
  void EmitImageLd(TypedReg dest, TypedReg image, TypedReg coord);
  void EmitImageSt(TypedReg src, TypedReg image, TypedReg coord);
  void EmitImageQuery(TypedReg dest, TypedReg image, BrigImageQuery query);

  HSAIL_ASM::DirectiveVariable Variable() { assert(var != 0); return var; }

  Value GenMemValue(Value v);
  void AddData(Value v) { data->push_back(v); }
  void SetData(Values* values) { data.reset(values); }
  Values* ReleaseData() { return data.release(); }
  Value GetRawData() { return (*data).at(0); }
  void InitImageCalculator(Sampler pSampler) { calculator.Init(this, pSampler); }
  void SetValueForCalculator(Value val) { calculator.ValueSet(val); }
  void ReadColor(Value* _coords, Value* _channels) const { calculator.EmulateImageRd(_coords, _channels); }
  void LoadColor(Value* _coords, Value* _channels) const { calculator.EmulateImageLd(_coords, _channels); }
  Value StoreColor(Value* _coords, Value* _channels) const {  return calculator.EmulateImageSt(_coords, _channels); }
};

class ESamplerSpec : public EVariableSpec, protected SamplerParams {
protected:
  BrigSamplerAddressing addressing;
  BrigSamplerCoordNormalization coord;
  BrigSamplerFilter filter;

  bool IsValidSegment() const;
  
public:
  explicit ESamplerSpec(
    BrigSegment brigseg_ = BRIG_SEGMENT_GLOBAL, 
    Location location_ = Location::KERNEL, 
    uint64_t dim_ = 0, 
    bool isConst_ = false, 
    bool output_ = false,
    BrigSamplerCoordNormalization coord_ = BRIG_COORD_UNNORMALIZED,
    BrigSamplerFilter filter_ = BRIG_FILTER_NEAREST,
    BrigSamplerAddressing addressing_ = BRIG_ADDRESSING_UNDEFINED
  ) 
  : EVariableSpec(brigseg_, BRIG_TYPE_SAMP, location_, BRIG_ALIGNMENT_8, dim_, isConst_, output_), 
    SamplerParams(addressing, coord, filter) { }

  bool IsValid() const;

  BrigSamplerCoordNormalization CoordNormalization() { return coord; }
  BrigSamplerFilter Filter() { return filter; }
  BrigSamplerAddressing Addresing() { return addressing; }
  
  void CoordNormalization(BrigSamplerCoordNormalization coord_) { coord = coord_; }
  void Filter(BrigSamplerFilter filter_) { filter = filter_; }
  void Addresing(BrigSamplerAddressing addressing_) { addressing = addressing_; }
};

class ESampler : public ESamplerSpec {
private:
  EString id;
  HSAIL_ASM::DirectiveVariable var;

  HSAIL_ASM::DirectiveVariable EmitAddressDefinition(BrigSegment segment);
  void EmitInitializer();
  void EmitDefinition();

  bool IsValidSegment() const;
  Location RealLocation() const;

public:
  ESampler(TestEmitter* te_, const std::string& id_, const ESamplerSpec* spec_);
  
  const char *Id() const { return id.c_str(); }
  std::string IdParams() const { return id.str() + ".params"; }

  void KernelArguments();
  void ModuleVariables();
  void FunctionFormalOutputArguments();
  void FunctionFormalInputArguments();
  void FunctionVariables();
  void KernelVariables();

  void ScenarioInit();
  void SetupDispatch(const std::string& dispatchId);
  void EmitSamplerQuery(TypedReg dest, TypedReg sampler, BrigSamplerQuery query);
  HSAIL_ASM::DirectiveVariable Variable() { assert(var != 0); return var; }
  TypedReg AddReg();
  TypedReg AddValueReg();
};


class EKernel : public EmittableContainerWithId {
private:
  HSAIL_ASM::DirectiveKernel kernel;

public:
  EKernel(TestEmitter* te, const std::string& id_)
    : EmittableContainerWithId(te, id_) { }

  std::string KernelName() const { return std::string("&") + Id(); }
  HSAIL_ASM::DirectiveKernel Directive() { assert(kernel != 0); return kernel; }
  HSAIL_ASM::Offset BrigOffset() { return Directive().brigOffset(); }
  void StartKernel();
  void StartKernelBody();
  void EndKernel();
  void Declaration();
};

class EFunction : public EmittableContainerWithId {
private:
  HSAIL_ASM::DirectiveFunction function;

public:
  EFunction(TestEmitter* te, const std::string& id_)
    : EmittableContainerWithId(te, id_) { }

  std::string FunctionName() const { return std::string("&") + Id(); }
  HSAIL_ASM::DirectiveFunction Directive() { assert(function != 0); return function; }
  HSAIL_ASM::Offset BrigOffset() { return Directive().brigOffset(); }
  void StartFunction();
  void StartFunctionBody();
  void EndFunction();
  void Declaration();
};

class EDispatch : public EmittableContainerWithId {
private:
  EString executableId;
  EString kernelName;

public:
  EDispatch(TestEmitter* te, const std::string& id_,
    const std::string& executableId_, const std::string& kernelName_);

  void ScenarioInit() override;
  void ScenarioDispatch() override;
  void SetupDispatch(const std::string& dispatchId) override;
};

const char* ConditionType2Str(ConditionType type);
const char* ConditionInput2Str(ConditionInput input);

class ECondition : public Emittable {
private:
  std::string id;
  ConditionType type;
  ConditionInput input;
  BrigType itype;
  BrigWidth width;

  HSAIL_ASM::DirectiveVariable kernarg, funcarg;
  TypedReg kerninp, funcinp;
  Buffer condBuffer;
  std::string lThen, lElse, lEnd;
  std::vector<std::string> labels;
  
  std::string KernargName();
  TypedReg InputData();
  
  std::string Id();

public:
  ECondition(ConditionType type_, ConditionInput input_, BrigWidth width_)
    : type(type_), input(input_), itype(BRIG_TYPE_U32), width(width_),
      kerninp(0), funcinp(0), condBuffer(0)
      { }
  ECondition(ConditionType type_, ConditionInput input_, BrigType itype_, BrigWidth width_)
    : type(type_), input(input_), itype(itype_), width(width_),
      kerninp(0), funcinp(0), condBuffer(0)
      { }
    
  ConditionType Type() { return type; }
  BrigType IType() { return itype; }
  ConditionInput Input() { return input; }
  BrigWidth Width() { return width; }
  std::string EndLabel() { return lEnd; }
  std::string ThenLabel() { return lThen; }

  void Name(std::ostream& out) const;
  void Reset(TestEmitter* te);

  void Init();
  void KernelArguments();
  void KernelVariables();
  void KernelInit();
  void FunctionFormalInputArguments();
  void FunctionInit();
  void SetupDispatch(const std::string& dispatchId);
  void ScenarioInit();
  void ScenarioValidation();
  void ActualCallArguments(TypedRegList inputs, TypedRegList outputs);

  bool IsTrueFor(uint64_t wi);
  bool IsTrueFor(const Dim& point) { return IsTrueFor(Geometry()->WorkitemFlatAbsId(point)); }

  HSAIL_ASM::Operand CondOperand();

  HSAIL_ASM::Operand EmitIfCond();
  void EmitIfThenStart();
  void EmitIfThenStartSand(Condition condition);
  void EmitIfThenStartSor();
  void EmitIfThenStartSor(Condition condition);
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

  uint32_t InputValue(uint64_t id, BrigWidth width = BRIG_WIDTH_NONE);
};

class BrigEmitter;
class CoreConfig;

class TestEmitter {
private:
  Context* context;
  Arena ap;
  std::unique_ptr<BrigEmitter> be;
  std::unique_ptr<Context> initialContext;
  std::unique_ptr<hexl::scenario::ScenarioBuilder> scenario;
  CoreConfig* coreConfig;

public:
  TestEmitter(Context* context_ = 0);

  Context* EmitContext() { return context; }
  void SetCoreConfig(CoreConfig* cc);

  Arena* Ap() { return &ap; }

  BrigEmitter* Brig() { return be.get(); }
  CoreConfig* CoreCfg() { return coreConfig; }

  Context* InitialContext() { return initialContext.get(); }
  Context* ReleaseContext() { return initialContext.release(); }
  hexl::scenario::ScenarioBuilder* TestScenario() { return scenario.get(); }
  hexl::scenario::ScenarioBuilder* ReleaseScenario() { return scenario.release(); }

  Variable NewVariable(const std::string& id, BrigSegment segment, BrigType type, Location location = AUTO, BrigAlignment align = BRIG_ALIGNMENT_NONE, uint64_t dim = 0, bool isConst = false, bool output = false);
  Variable NewVariable(const std::string& id, VariableSpec varSpec);
  Variable NewVariable(const std::string& id, VariableSpec varSpec, bool output);
  FBarrier NewFBarrier(const std::string& id, Location location = Location::KERNEL, bool output = false);
  Buffer NewBuffer(const std::string& id, BufferType type, ValueType vtype, size_t count);
  UserModeQueue NewQueue(const std::string& id, UserModeQueueType type);
  Signal NewSignal(const std::string& id, uint64_t initialValue);
  Kernel NewKernel(const std::string& id);
  Function NewFunction(const std::string& id);
  Image NewImage(const std::string& id, ImageSpec spec);
  Sampler NewSampler(const std::string& id, SamplerSpec spec);
  Dispatch NewDispatch(const std::string& id, const std::string& executableId, const std::string& kernelName);
};

}

class EmittedTestBase : public TestSpec {
protected:
  std::unique_ptr<hexl::Context> context;
  std::unique_ptr<emitter::TestEmitter> te;

public:
  EmittedTestBase()
    : context(new hexl::Context()),
      te(new hexl::emitter::TestEmitter(context.get())) { }

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
  emitter::Dispatch dispatch;
    
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

  virtual void SetupDispatch(const std::string& dispatchId);

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

  virtual BrigType ResultType() const { assert(0); return BRIG_TYPE_NONE; }
  virtual uint64_t ResultDim() const { return 0; }
  uint32_t ResultCount() const { assert(ResultDim() < UINT32_MAX); return (std::max)((uint32_t) ResultDim(), (uint32_t) 1); }
  bool IsResultType(BrigType type) const { return ResultType() == type; }
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
  virtual void ScenarioDispatch();
  virtual void ScenarioValidation();
  virtual void ScenarioEnd();
  virtual void Finish();
};

inline std::ostream& operator<<(std::ostream& out, const hexl::emitter::EmitterObject& o) { o.Name(out); return out; }
inline std::ostream& operator<<(std::ostream& out, const hexl::emitter::EmitterObject* o) { o->Name(out); return out; }

}

#endif // HEXL_EMITTER_HPP
