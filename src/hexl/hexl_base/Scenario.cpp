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

#include "Scenario.hpp"
#include "RuntimeContext.hpp"
#include "HexlTest.hpp"
#include "HexlResource.hpp"
#include "HexlTestFactory.hpp"
#include <thread>
#include <sstream>

namespace hexl {

namespace scenario {

const std::string Defaults::PROGRAM_ID("program");
const std::string Defaults::MODULE_ID("module");
const std::string Defaults::BRIG_ID("brig");
const std::string Defaults::CODE_ID("code");
const std::string Defaults::DISPATCH_SETUP_ID("dispatchSetup");
const std::string Defaults::SCENARIO_ID("scenario");
const std::string Defaults::SIGNAL_ID("signal");
const std::string Defaults::SIGNAL_ATOMIC_ID("signalAtomic");

void CommandSequence::Add(Command* command)
{
  commands.push_back(std::unique_ptr<Command>(command));
}

bool CommandSequence::Run(Context *context)
{
  assert(context);
  SetContext(context);
  bool result = true;
  unsigned commandID = 0;
  for (std::unique_ptr<Command>& command : commands) {
    command->SetContext(context);
    if (context->IsVerbose("scenario")) {
      std::stringstream ss;
      IndentStream indent(ss);
      ss << id << "." << ++commandID << ". ";
      command->Print(ss); ss << std::endl;
      context->Debug() << ss.str();
    }
    if (!command->Run()) { result = false; break; }
  }
  return result;
}

bool CommandSequence::Finish(Context* context)
{
  bool result = true;
  for (std::unique_ptr<Command>& command : commands) {
    if (!command->Finish()) { result = false; }
  }
  return result;
}

void CommandSequence::Print(std::ostream& out) const
{
  IndentStream indent(out);
  unsigned num = 0;
  out << "Thread[" << id << "] :" << std::endl;
  IndentStream indent2(out);
  for (const std::unique_ptr<Command>& command : commands) {
    out << ++num << ". "; command->Print(out); out << std::endl;
  }
}

class CreateProgramCommand : public Command {
private:
  std::string programId;
  Program* program;

public:
  CreateProgramCommand(const std::string& programId_ = Defaults::PROGRAM_ID)
    : programId(programId_), program(0) { }

  bool Finish() {
    if (program) { delete program; program = 0; }
    return true;
  }

  virtual bool Run() {
    program = context->State()->NewProgram();
    if (!program) { return false; }
    context->Put(programId, program);
    return true;
  }

  void Print(std::ostream& out) const {
    out << "create_program " << programId;
  }
};

class AddBrigModuleCommand : public Command {
private:
  std::string programId;
  std::string moduleId;
  std::string brigId;
  Module* module;

public:
  AddBrigModuleCommand(const std::string& programId_ = Defaults::PROGRAM_ID, const std::string& moduleId_ = Defaults::MODULE_ID, const std::string& brigId_ = Defaults::BRIG_ID)
    : programId(programId_), moduleId(moduleId_), brigId(brigId_), module(0) { }

  bool Finish() {
    if (module) { delete module; module = 0; }
    return true;
  }

  virtual bool Run() {
    Program* program = context->Get<Program*>(programId);
    if (!program) {
      context->Env()->Error("Program not found: %s", programId.c_str());
      return false;
    }
    brig_container_t brig = context->Get<brig_container_t>(brigId);
    if (!brig) {
      context->Env()->Error("Brig not found: %s", brigId.c_str());
      return false;
    }
    Module* module = context->State()->NewModuleFromBrig(brig);
    if (!module) { return false; }
    context->Put(moduleId, module);
    program->AddModule(module);
    return true;
  }

  void Print(std::ostream& out) const {
    out << "add_brig_module " << moduleId << " " << programId << " " << brigId;
  }
};

class ValidateProgramCommand : public Command {
private:
  std::string programId;

public:
  ValidateProgramCommand(const std::string& programId_ = Defaults::PROGRAM_ID)
    : programId(programId_) { }

  virtual bool Run() {
    Program* program = context->Get<Program*>(programId);
    if (!program) { return false; }
    std::string errMsg;
    if (!program->Validate(errMsg)) {
      context->Env()->Error("Program %s validation failed: %s", programId.c_str(), errMsg.c_str());
      return false;
    }
    return true;
  }

  void Print(std::ostream& out) const {
    out << "validate_program " << programId;
  }
};

class FinalizeCommand : public Command {
private:
  std::string codeId;
  std::string programId;
  std::vector<uint32_t> kernelOffsets;
  Code* code;

public:
  FinalizeCommand(const std::string& codeId_, const std::string& programId_ = Defaults::PROGRAM_ID, uint32_t kernelOffset = 0)
    : codeId(codeId_), programId(programId_), code(0)
  {
    kernelOffsets.push_back(kernelOffset);
  }

  bool Finish() {
    if (code) { delete code; code = 0; }
    return true;
  }

  virtual bool Run() {
    Program* program = context->Get<Program*>(programId);
    if (!program) { return false; }
    // TODO: support multiple kernels
    assert(kernelOffsets.size() == 1);
    uint32_t kernelOffset = kernelOffsets[0];
    DispatchSetup* dsetup = context->Get<DispatchSetup*>(Defaults::DISPATCH_SETUP_ID);
    code = context->State()->Finalize(program, kernelOffset, dsetup);
    if (!code) { return false; }
    context->Put(codeId, code);
    return true;
  }

  void Print(std::ostream& out) const {
    out << "finalize " << codeId << " " << programId;
    if (kernelOffsets[0]) { out << " " << kernelOffsets[0]; }
  }
};

class DispatchCommand : public Command {
private:
  std::string codeId;
  std::string dispatchSetupId;
  Dispatch* dispatch;

public:
  DispatchCommand(const std::string& codeId_, const std::string& dispatchSetupId_)
    : codeId(codeId_), dispatchSetupId(dispatchSetupId_), dispatch(0) { }

  bool Finish() {
    if (dispatch) { delete dispatch; dispatch = 0; }
    return true;
  }

  virtual bool Run() {
    Code* code = context->Get<Code*>(codeId);
    if (!code) { return false; }
    DispatchSetup* dsetup = context->Get<DispatchSetup*>(dispatchSetupId);
    if (!dsetup) { return false; }
    dispatch = context->State()->NewDispatch(code);
    {
      context->Info() << "Dispatch setup: " << std::endl;
      IndentStream indent(context->Info());
      dsetup->Print(context->Info());
    }
    dispatch->Apply(*dsetup);
    context->Info() << "Executing dispatch." << std::endl;
    if (!dispatch->Execute()) { context->Info() << "Failed to execute dispatch" << std::endl; return false; }
    context->Info() << "Returned from dispatch:" << std::endl;
    context->Info() << "Validating results:" << std::endl;
    if (dispatch->MState()->Validate() != 0) { return false; }
    return true;
  }

  void Print(std::ostream& out) const {
    out << "dispatch " << codeId << " " << dispatchSetupId;
  }
};

class StartThreadCommand : public Command {
private:
  unsigned id;
  std::thread thread;
  Scenario* scenario;
  volatile bool result;

  void RunThread() {
    result = scenario->Commands(id).Run(context);
  }

  void Start() {
    std::stringstream ss;
    ss << "Starting thread: " << id << std::endl;
    context->Info() << ss.str();
    scenario = context->Get<Scenario*>(Defaults::SCENARIO_ID);
    assert(scenario);
    thread = std::thread(&hexl::scenario::StartThreadCommand::RunThread, this);
  }

  void Wait() {
    if (thread.joinable()) {
      std::stringstream ss;
      ss << "Joining thread: " << id << std::endl;
      context->Info() << ss.str();
      thread.join();
    }
    std::stringstream ss;
    ss << "Thread [" << id << "] result: " << (result ? "PASSED" : "FAILED") << std::endl;
    context->Info() << ss.str();
  }

public:
  StartThreadCommand(unsigned id_): id(id_) { }

  bool Finish() {
    Wait();
    if (!scenario->Commands(id).Finish(context)) { result = false; }
    return result;
  }

  virtual bool Run() {
    Start();
    return true;
  }

  void Print(std::ostream& out) const {
    out << "start_thread " << id;
  }
};

void CommandSequence::CreateProgram(const std::string& programId)
{
  Add(new CreateProgramCommand(programId));
}

void CommandSequence::AddBrigModule(const std::string& programId, const std::string& moduleId, const std::string& brigId)
{
  Add(new AddBrigModuleCommand(programId, moduleId, brigId));
}

void CommandSequence::ValidateProgram(const std::string& programId)
{
  Add(new ValidateProgramCommand(programId));
}

void CommandSequence::Finalize(const std::string& codeId, const std::string& programId, uint32_t kernelOffset)
{
  Add(new FinalizeCommand(codeId, programId, kernelOffset));
}

void CommandSequence::Dispatch(const std::string& codeId, const std::string& dispatchSetupId)
{
  Add(new DispatchCommand(codeId, dispatchSetupId));
}

void CommandSequence::StartThread(unsigned id)
{
  Add(new StartThreadCommand(id));
}

bool Scenario::Run(Context* context)
{
  bool result = true;
  if (context->IsVerbose("scenario")) {
    std::stringstream ss;
    ss << "Execute test scenario:" << std::endl;
    context->Debug() << ss.str();
  }
  StartThreadCommand command0(0);
  command0.SetContext(context);
  if (!command0.Run()) { result = false; }
  if (!command0.Finish()) { result = false; }
  return result;
}

void Scenario::Print(std::ostream& out)
{
  std::vector<std::unique_ptr<CommandSequence>>::iterator it = commandSequences.begin();
  for (it; it != commandSequences.end(); ++it) {
    (*it)->Print(out);
  }
}

}

using namespace scenario;

ScenarioTest::ScenarioTest(const std::string& name_, Context* initialContext)
  : TestImpl(initialContext), name(name_)
{
}


void ScenarioTest::Run()
{
  Scenario *scenario = context->Get<Scenario*>(Defaults::SCENARIO_ID);
  if (context->IsVerbose("scenario")) {
    context->Debug() << "Test scenario:" << std::endl;
    scenario->Print(context->Debug());
  }
  RuntimeContext* runtime = context->Runtime();
  RuntimeContextState* state = runtime->NewState(context.get());
  context->Put("hexl.runtimestate", state);
  brig_container_t brig = context->Get<brig_container_t>("brig");
  if (brig) context->DumpBrigIfEnabled("brig", brig);
  if (!scenario->Run(context.get())) { SetFailed(); }
  delete scenario;
  delete state;
}

}
