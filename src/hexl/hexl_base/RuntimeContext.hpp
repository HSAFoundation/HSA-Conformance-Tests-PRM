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

#ifndef HEXL_RUNTIME_CONTEXT_HPP
#define HEXL_RUNTIME_CONTEXT_HPP

#include "MObject.hpp"
#include "Options.hpp"
#include "hsail_c.h"
#include <map>

namespace hexl {

class Module {
public:
  virtual ~Module() { }
};

class Program {
public:
  virtual ~Program() { }
  virtual void AddModule(Module* module) = 0;
  virtual bool Validate(std::string& errMsg) = 0;
};

class Code {
public:
  virtual ~Code() { }
};

class MemoryState {
public:
  virtual ~MemoryState() { }
  virtual void Add(MObject* mo) = 0;
  virtual bool Allocate() = 0;
  virtual void Push() = 0;
  virtual void Pull() = 0;
  virtual unsigned Validate() = 0;
};

class Dispatch {
public:
  virtual ~Dispatch() { }
  virtual MemoryState* MState() = 0;
  virtual void SetDimensions(uint32_t dimensions) = 0;
  virtual void SetGridSize(uint32_t x, uint32_t y = 1, uint32_t z = 1) = 0;
  void SetGridSize(const uint32_t* size) { SetGridSize(size[0], size[1], size[2]); }
  virtual void SetWorkgroupSize(uint16_t x, uint16_t y = 1, uint16_t z = 1) = 0;
  void SetWorkgroupSize(const uint16_t* size) { SetWorkgroupSize(size[0], size[1], size[2]); }
  virtual bool Execute() = 0;
  void Apply(const DispatchSetup& setup);
};

class RuntimeContextState {
protected:
  Context* context;

public:
  RuntimeContextState(Context* context_)
    : context(context_) { }
  virtual ~RuntimeContextState() { }
  Context* GetContext() { return context; }
  virtual Program* NewProgram() = 0;
  virtual Module* NewModuleFromBrig(brig_container_t brig) = 0;
  virtual Module* NewModuleFromHsailText(const std::string& hsailText) = 0;
  virtual Code* Finalize(Program* program, uint32_t kernelOffset = 0, DispatchSetup* dsetup = 0) = 0;
  virtual Dispatch* NewDispatch(Code* code) = 0;
};

class RuntimeContext {
protected:
  Context* context;

public:
  RuntimeContext(Context* context_)
    : context(context_) { }
  virtual ~RuntimeContext() { }
  Context* GetContext() { return context; }
  virtual std::string Name() const = 0;
  virtual bool Init() = 0;
  virtual std::string Description() const =0;
  virtual RuntimeContextState* NewState(Context* context) = 0;
};

template <class MState>
class IObject {
protected:
  MState* state;

public:
  IObject(MState* state_) : state(state_) { }
  virtual ~IObject() { }

//  virtual void *Ptr() = 0;
  virtual bool Push() = 0;
  virtual bool Pull() = 0;
  virtual bool Validate() = 0;
  virtual void Print(std::ostream& out) const = 0;
  MState* State() { return state; }
};

template <class MState>
class MemoryStateBase : public MemoryState {
private:
  std::map<unsigned, MObject*> mos;
  std::vector<unsigned> mosOrder;
  std::map<unsigned, IObject<MState>*> hos;

protected:
  Context* context;

  virtual bool PreAllocate() {
    for (size_t i = 0; i < mosOrder.size(); ++i) {
      if (!PreAllocate(mos[mosOrder[i]])) { return false; }
    }
    return true;
  }

  bool PreAllocate(MObject* mo) {
    assert(mo);
    switch (mo->Type()) {
    case MBUFFER: return PreAllocateBuffer(static_cast<MBuffer*>(mo));
    case MRBUFFER: return PreAllocateRBuffer(static_cast<MRBuffer*>(mo));
    case MIMAGE: return PreAllocateImage(static_cast<MImage*>(mo));
    case MRIMAGE: return PreAllocateRImage(static_cast<MRImage*>(mo));
    case MSAMPLER: return PreAllocateSampler(static_cast<MSampler*>(mo));
    case MRSAMPLER: return PreAllocateRSampler(static_cast<MRSampler*>(mo));
    default: assert(false); return false;
    }
  }

  IObject<MState>* Allocate(MObject* mo) {
    assert(mo);
    switch (mo->Type()) {
    case MBUFFER: return AllocateBuffer(static_cast<MBuffer*>(mo));
    case MRBUFFER: return AllocateRBuffer(static_cast<MRBuffer*>(mo));
    case MIMAGE: return AllocateImage(static_cast<MImage*>(mo));
    case MRIMAGE: return AllocateRImage(static_cast<MRImage*>(mo));
    case MSAMPLER: return AllocateSampler(static_cast<MSampler*>(mo));
    case MRSAMPLER: return AllocateRSampler(static_cast<MRSampler*>(mo));
    default: assert(false); return 0;
    }
  }

  virtual bool PreAllocateBuffer(MBuffer* mo) { return true; }
  virtual bool PreAllocateRBuffer(MRBuffer* mo) { return true; }
  virtual bool PreAllocateImage(MImage* mo) { return true; }
  virtual bool PreAllocateRImage(MRImage* mo) { return true; }
  virtual bool PreAllocateSampler(MSampler* mo) { return true; }
  virtual bool PreAllocateRSampler(MRSampler* mo) { return true; }
  virtual IObject<MState>* AllocateBuffer(MBuffer* mo) = 0;
  virtual IObject<MState>* AllocateRBuffer(MRBuffer* mo) = 0;
  virtual IObject<MState>* AllocateImage(MImage* mi) = 0;
  virtual IObject<MState>* AllocateRImage(MRImage* mi) = 0;
  virtual IObject<MState>* AllocateSampler(MSampler* mi) = 0;
  virtual IObject<MState>* AllocateRSampler(MRSampler* mi) = 0;
  virtual Value GetValue(Value v) = 0;

  virtual void Print(std::ostream& out) {
    if (context->IsVerbose("mstate")) {
      out << "Runtime memory state: " << std::endl;
      IndentStream indent(out);
      for (unsigned i = 0; i < hos.size(); ++i) {
        assert(i < mos.size() && mos[i]);
        assert(hos[i]);
        out << "'" << mos[i]->Name() << "', ";
        hos[i]->Print(out);
        out << std::endl;
      }
    }
    out << std::flush;
  }

  template<typename T> T* Get(unsigned id) {
    IObject<MState>* ho = hos[id];
    if (!ho) { return 0; }
    return static_cast<T*>(ho);
  }

public:
  MemoryStateBase(Context* context_)
    : context(context_) { }

  ~MemoryStateBase() {
    for (unsigned i = 0; i < hos.size(); ++i) {
      if (hos[i]) delete hos[i];
      hos[i] = NULL;
    }
    // mos are not owned by MemoryStateBase, they are managed by MemorySetup.
  }

  Context* GetContext() { return context; }

  void Add(MObject* mo) {
    assert(mo);
    mos[mo->Id()] = mo;
    mosOrder.push_back(mo->Id());
  }

  bool Allocate() {
    if (!PreAllocate()) { return false; }
    for (size_t i = 0; i < mosOrder.size(); ++i) {
      MObject* mo = mos[mosOrder[i]];
      IObject<MState>* ho = Allocate(mo);
      assert(mo && ho);
      hos[mo->Id()] = ho;
    }
    assert(mos.size() == hos.size());
    return true;
  }

  void Push() {
    for (unsigned i = 0; i < hos.size(); ++i) {
      assert(hos[i]);
      hos[i]->Push();
    }
    Print(context->Debug());
  }

  void Pull() {
    for (unsigned i = 0; i < hos.size(); ++i) {
      assert(hos[i]);
      hos[i]->Pull();
    }
  }

  unsigned Validate() {
    Pull();
    unsigned errors = 0;
    for (unsigned i = 0; i < hos.size(); ++i) {
      assert(hos[i]);
      if (!hos[i]->Validate()) { errors++; }
    }
    return errors;
  }

  bool ValidateRBuffer(MBuffer* mb, MRBuffer* mr, Values actual, const Options* options);
  bool ValidateRImage(MImage* mi, MRImage* mr, Values actual, const Options* options);
};

class Options;

static const unsigned MAX_SHOWN_FAILURES = 16;

template <class MState>
bool MemoryStateBase<MState>::ValidateRBuffer(MBuffer* mb, MRBuffer* mr, Values actual, const Options* options)
{
  Comparison& comparison = mr->GetComparison();
  assert(mr->Data().size() == actual.size());
  comparison.Reset(mr->VType());
  unsigned maxShownFailures = options->GetUnsigned("hexl.max_shown_failures", MAX_SHOWN_FAILURES);
  bool verboseData = context->IsVerbose("data");
  unsigned shownFailures = 0;
  for (unsigned i = 0; i < actual.size(); ++i) {
    Value expected = GetValue(mr->Data()[i]);
    bool passed = comparison.Compare(expected, actual[i]);
    if ((!passed && comparison.GetFailed() < maxShownFailures) || verboseData) {
      context->Info() << "  " << mb->GetPosStr(i) << ": ";
      mb->PrintComparisonInfo(context->Info(), i, comparison, true);
      context->Info() << std::endl;
      if (!passed) { shownFailures++; }
    }
  }
  if (comparison.GetFailed() > shownFailures) {
    context->Info() << "  ... (" << (comparison.GetFailed() - shownFailures) << " more failures not shown)" << std::endl;
  }
  context->Info() << "  "; mb->PrintComparisonSummary(context->Info(), comparison);
  return !comparison.IsFailed();
}

template <class MState>
bool MemoryStateBase<MState>::ValidateRImage(MImage* mi, MRImage* mr, Values actual, const Options* options)
{
  Comparison& comparison = mr->GetComparison();
  assert(mr->Data().size() == actual.size());
  comparison.Reset(mr->VType());
  unsigned maxShownFailures = options->GetUnsigned("hexl.max_shown_failures", MAX_SHOWN_FAILURES);
  bool verboseData = context->IsVerbose("data");
  unsigned shownFailures = 0;
  for (unsigned i = 0; i < actual.size(); ++i) {
    Value expected = GetValue(mr->Data()[i]);
    bool passed = comparison.Compare(expected, actual[i]);
    if ((!passed && comparison.GetFailed() < maxShownFailures) || verboseData) {
      context->Info() << "  " << mi->GetPosStr(i) << ": ";
      mi->PrintComparisonInfo(context->Info(), i, comparison);
      context->Info() << std::endl;
      if (!passed) { shownFailures++; }
    }
  }
  if (comparison.GetFailed() > shownFailures) {
    context->Info() << "  ... (" << (comparison.GetFailed() - shownFailures) << " more failures not shown)" << std::endl;
  }
  context->Info() << "  "; mi->PrintComparisonSummary(context->Info(), comparison);
  return !comparison.IsFailed();
}

//bool ValidateRBuffer(MBuffer* mb, MRBuffer* mr, Values actual, const Options* options);

void *alignedMalloc(size_t size, size_t align);
void *alignedFree(void *ptr);

};

#endif // HEXL_RUNTIME_CONTEXT_HPP
