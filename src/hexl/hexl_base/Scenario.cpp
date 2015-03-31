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

using namespace runtime;

namespace scenario {

  void CommandSequence::Add(Command* command)
  {
    commands.push_back(std::unique_ptr<Command>(command));
  }

  void CommandSequence::Print(std::ostream& out) const
  {
    for (const std::unique_ptr<Command>& c : commands) {
      c->Print(out);
      out << std::endl;
    }
  }

  bool CommandSequence::Execute(runtime::RuntimeState* rt)
  {
    for (const std::unique_ptr<Command>& c : commands) {
      // rt->GetContext()->Debug() << "Execute command: "; c->Print(rt->GetContext()->Debug()); rt->GetContext()->Debug() << std::endl;
      if (!c->Execute(rt)) { return false; }
    }
    return true;
  }

  bool CommandSequence::Finish(runtime::RuntimeState* rt)
  {
    bool result = true;
    for (const std::unique_ptr<Command>& c : commands) {
      result &= c->Finish(rt);
    }
    return result;
  }

  CommandsBuilder::CommandsBuilder(Context* initialContext_)
    : initialContext(initialContext_), commands(new CommandSequence())
  {
  }

  CommandSequence* CommandsBuilder::ReleaseCommands()
  {
    return commands.release();
  }

  CommandsBuilder* ScenarioBuilder::Commands(unsigned id)
  {
    for (size_t i = commands.size(); i <= id; ++i) {
      commands.push_back(std::unique_ptr<CommandsBuilder>(new CommandsBuilder(initialContext)));
    };
    return commands[id].get();
  }

  Scenario* ScenarioBuilder::ReleaseScenario()
  {
    Scenario* scenario = new Scenario();
    for (unsigned i = 0; i < commands.size(); ++i) {
      scenario->AddCommands(commands[i]->ReleaseCommands());
    }
    return scenario;
  }

  CommandSequence* Scenario::Commands(unsigned id)
  {
    for (size_t i = commands.size(); i <= id; ++i) {
      commands.push_back(std::unique_ptr<CommandSequence>(new CommandSequence()));
    };
    return commands[id].get();
  }

  void Scenario::AddCommands(CommandSequence* c)
  {
    commands.push_back(std::unique_ptr<CommandSequence>(c));
  }

  bool Scenario::Execute(runtime::RuntimeState* rt)
  {
    bool result = true;
    result &= rt->StartThread(0, commands[0].get());
    result &= rt->WaitThreads();
    for (std::unique_ptr<CommandSequence>& c : commands) {
      result &= c->Finish(rt);
    }
    return result;
  }

  void Scenario::Print(std::ostream& out) const
  {
    unsigned i = 0;
    for (const std::unique_ptr<CommandSequence>& c : commands) {
      if (i == 0) {
        c->Print(out);
      } else {
        out << "Thread " << i << ":" << std::endl;
        IndentStream indent(out);
        c->Print(out);
      }
      ++i;
    }
  }

  class StartThreadCommand : public Command {
  private:
    unsigned id;
    std::thread thread;
    runtime::RuntimeState* runtime;
    Scenario* scenario;
    volatile bool result;

    void RunThread() {
      result = scenario->Commands(id)->Execute(runtime);
    }

    void Start(runtime::RuntimeState* runtime) {
      this->runtime = runtime;
      Context* context = runtime->GetContext();
      scenario = Scenario::Get(runtime->GetContext());
      std::stringstream ss;
      ss << "Starting thread: " << id << std::endl;
      context->Info() << ss.str();
      assert(scenario);
      thread = std::thread(&hexl::scenario::StartThreadCommand::RunThread, this);
    }

    void Wait(runtime::RuntimeState* runtime) {
      Context* context = runtime->GetContext();
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

    bool Finish(runtime::RuntimeState* runtime) {
      Scenario* scenario = Scenario::Get(runtime->GetContext());
      Wait(runtime);
      if (!scenario->Commands(id)->Finish(runtime)) { result = false; }
      return result;
    }

    virtual bool Execute(runtime::RuntimeState* runtime) {
      Start(runtime);
      return true;
    }

    void Print(std::ostream& out) const {
      out << "start_thread " << id;
    }
  };

  bool CommandsBuilder::StartThread(unsigned id, Command* commandToRun)
  {
    commands->Add(new StartThreadCommand(id));
    return true;
  }

  bool CommandsBuilder::WaitThreads()
  {
    assert(!"CommandsBuilder::WaitThreads should not be used");
    return false;
  }

  class ModuleCreateFromBrigCommand : public Command {
  private:
    std::string moduleId;
    std::string brigId;

  public:
    ModuleCreateFromBrigCommand(const std::string& moduleId_, const std::string& brigId_)
      : moduleId(moduleId_), brigId(brigId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ModuleCreateFromBrig(moduleId, brigId);
    }

    void Print(std::ostream& out) const {
      out << "module_create_from_brig " << moduleId << " " << brigId;
    }
  };

  bool CommandsBuilder::ModuleCreateFromBrig(const std::string& moduleId, const std::string& brigId)
  {
    commands->Add(new ModuleCreateFromBrigCommand(moduleId, brigId));
    return true;
  }

  class ProgramCreateCommand : public Command {
  private:
    std::string programId;

  public:
    ProgramCreateCommand(const std::string& programId_)
      : programId(programId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ProgramCreate(programId);
    }

    void Print(std::ostream& out) const {
      out << "program_create " << programId;
    }
  };

  bool CommandsBuilder::ProgramCreate(const std::string& programId)
  {
    commands->Add(new ProgramCreateCommand(programId));
    return true;
  }

  class ProgramAddModuleCommand : public Command {
  private:
    std::string programId;
    std::string moduleId;

  public:
    ProgramAddModuleCommand (const std::string& programId_, const std::string& moduleId_)
      : programId(programId_), moduleId(moduleId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ProgramAddModule(programId, moduleId);
    }

    void Print(std::ostream& out) const {
      out << "program_add_module " << programId << " " << moduleId;
    }
  };

  bool CommandsBuilder::ProgramAddModule(const std::string& programId, const std::string& moduleId)
  {
    commands->Add(new ProgramAddModuleCommand(programId, moduleId));
    return true;
  }

  class ProgramFinalizeCommand : public Command {
  private:
    std::string codeId;
    std::string programId;

  public:
    ProgramFinalizeCommand (const std::string& codeId_, const std::string& programId_)
      : codeId(codeId_), programId(programId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ProgramFinalize(codeId, programId);
    }

    void Print(std::ostream& out) const {
      out << "program_finalize " << codeId << " " << programId;
    }
  };

  bool CommandsBuilder::ProgramFinalize(const std::string& codeId, const std::string& programId)
  {
    commands->Add(new ProgramFinalizeCommand(codeId, programId));
    return true;
  }

  class ExecutableCreateCommand : public Command {
  private:
    std::string executableId;

  public:
    ExecutableCreateCommand(const std::string& executableId_)
      : executableId(executableId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ExecutableCreate(executableId);
    }

    void Print(std::ostream& out) const {
      out << "executable_create " << executableId;
    }
  };

  bool CommandsBuilder::ExecutableCreate(const std::string& executableId)
  {
    commands->Add(new ExecutableCreateCommand(executableId));
    return true;
  }

  class ExecutableLoadCodeCommand : public Command {
  private:
    std::string executableId;
    std::string codeId;

  public:
    ExecutableLoadCodeCommand (const std::string& executableId_, const std::string& codeId_)
      : executableId(executableId_), codeId(codeId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ExecutableLoadCode(executableId, codeId);
    }

    void Print(std::ostream& out) const {
      out << "executable_load_code " << executableId << " " << codeId;
    }
  };

  bool CommandsBuilder::ExecutableLoadCode(const std::string& executableId, const std::string& codeId)
  {
    commands->Add(new ExecutableLoadCodeCommand(executableId, codeId));
    return true;
  }

  class ExecutableFreezeCommand : public Command {
  private:
    std::string executableId;

  public:
    ExecutableFreezeCommand (const std::string& executableId_)
      : executableId(executableId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ExecutableFreeze(executableId);
    }

    void Print(std::ostream& out) const {
      out << "executable_freeze " << executableId;
    }
  };

  bool CommandsBuilder::ExecutableFreeze(const std::string& executableId)
  {
    commands->Add(new ExecutableFreezeCommand(executableId));
    return true;
  }

  class BufferCreateCommand : public Command {
  private:
    std::string bufferId;
    size_t size;
    std::string initValuesId;

  public:
    BufferCreateCommand (const std::string& bufferId_, size_t size_, const std::string& initValuesId_)
      : bufferId(bufferId_), size(size_), initValuesId(initValuesId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->BufferCreate(bufferId, size, initValuesId);
    }

    void Print(std::ostream& out) const {
      out << "buffer_create " << bufferId << " " << size << " "<< initValuesId;
    }
  };

  bool CommandsBuilder::BufferCreate(const std::string& bufferId, size_t size, const std::string& initValuesId)
  {
    commands->Add(new BufferCreateCommand(bufferId, size, initValuesId));
    return true;
  }

  class BufferValidateCommand : public Command {
  private:
    std::string bufferId;
    std::string expectedDataId;
    std::string method;

  public:
    BufferValidateCommand (const std::string& bufferId_, const std::string& expectedDataId_, const std::string& method_)
      : bufferId(bufferId_), expectedDataId(expectedDataId_), method(method_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->BufferValidate(bufferId, expectedDataId, method);
    }

    void Print(std::ostream& out) const {
      out << "buffer_validate " << bufferId << " " << expectedDataId << " " << method;
    }
  };

  bool CommandsBuilder::BufferValidate(const std::string& bufferId, const std::string& expectedDataId, const std::string& method)
  {
    commands->Add(new BufferValidateCommand(bufferId, expectedDataId, method));
    return true;
  }

  class ImageCreateCommand : public Command {
  private:
    std::string imageId;
    std::string imageParamsId;
    std::string imageDataId;

  public:
    ImageCreateCommand(const std::string& imageId_, const std::string& imageParamsId_, const std::string& imageDataId_)
      : imageId(imageId_), imageParamsId(imageParamsId_), imageDataId(imageDataId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ImageCreate(imageId, imageParamsId, imageDataId);
    }

    void Print(std::ostream& out) const {
      out << "image_create " << imageId << " " << imageParamsId << " " << imageDataId;
    }
  };

  bool CommandsBuilder::ImageCreate(const std::string& imageId, const std::string& imageParamsId, const std::string& initValuesId)
  {
    commands->Add(new ImageCreateCommand(imageId, imageParamsId, initValuesId));
    return true;
  }

  class ImageValidateCommand : public Command {
  private:
    std::string imageId;
    std::string expectedDataId;
    std::string method;

  public:
    ImageValidateCommand(const std::string& imageId_, const std::string& expectedDataId_, const std::string& method_)
      : imageId(imageId_), expectedDataId(expectedDataId_), method(method_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->ImageValidate(imageId, expectedDataId, method);
    }

    void Print(std::ostream& out) const {
      out << "image_validate " << imageId << " " << expectedDataId << " " << method;
    }
  };

  bool CommandsBuilder::ImageValidate(const std::string& imageId, const std::string& expectedDataId, const std::string& method)
  {
    commands->Add(new ImageValidateCommand(imageId, expectedDataId, method));
    return true;
  }

  class SamplerCreateCommand : public Command {
  private:
    std::string samplerId;
    std::string samplerParamsId;

  public:
    SamplerCreateCommand(const std::string& samplerId_, const std::string& samplerParamsId_)
      : samplerId(samplerId_), samplerParamsId(samplerParamsId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->SamplerCreate(samplerId, samplerParamsId);
    }

    void Print(std::ostream& out) const {
      out << "sampler_create " << samplerId << " " << samplerParamsId;
    }
  };

  bool CommandsBuilder::SamplerCreate(const std::string& samplerId, const std::string& samplerParamsId)
  {
    commands->Add(new SamplerCreateCommand(samplerId, samplerParamsId));
    return true;
  }

class DispatchCreateCommand : public Command {
  private:
    std::string dispatchId;
    std::string executableId;
    std::string kernelName;

  public:
    DispatchCreateCommand(const std::string& dispatchId_, const std::string& executableId_, const std::string& kernelName_)
      : dispatchId(dispatchId_), executableId(executableId_), kernelName(kernelName_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->DispatchCreate(dispatchId, executableId, kernelName);
    }

    void Print(std::ostream& out) const {
      out << "dispatch_create " << dispatchId << " " << executableId << " " << kernelName;
    }
  };

  bool CommandsBuilder::DispatchCreate(const std::string& dispatchId, const std::string& executableId, const std::string& kernelName)
  {
    commands->Add(new DispatchCreateCommand(dispatchId, executableId, kernelName));
    return true;
  }

  class DispatchArgCommand : public Command {
  private:
    std::string dispatchId;
    DispatchArgType argType;
    std::string argKey;

  public:
    DispatchArgCommand(const std::string& dispatchId_, DispatchArgType argType_, const std::string& argKey_)
      : dispatchId(dispatchId_), argType(argType_), argKey(argKey_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->DispatchArg(dispatchId, argType, argKey);
    }

    void Print(std::ostream& out) const {
      out << "dispatch_arg " << dispatchId << " " << argType << " " << argKey;
    }
  };

  bool CommandsBuilder::DispatchArg(const std::string& dispatchId, DispatchArgType argType, const std::string& argKey)
  {
    commands->Add(new DispatchArgCommand(dispatchId, argType, argKey));
    return true;
  }

  class DispatchExecuteCommand : public Command {
  private:
    std::string dispatchId;

  public:
    DispatchExecuteCommand (const std::string& dispatchId_)
      : dispatchId(dispatchId_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->DispatchExecute(dispatchId);
    }

    void Print(std::ostream& out) const {
      out << "dispatch_execute " << dispatchId;
    }
  };

  bool CommandsBuilder::DispatchExecute(const std::string& dispatchId)
  {
    commands->Add(new DispatchExecuteCommand(dispatchId));
    return true;
  }

  class SignalCreateCommand : public Command {
  private:
    std::string signalId;
    uint64_t initialValue;

  public:
    SignalCreateCommand(const std::string& signalId_, uint64_t initialValue_)
      : signalId(signalId_), initialValue(initialValue_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->SignalCreate(signalId, initialValue);
    }

    void Print(std::ostream& out) const {
      out << "signal_create " << signalId << " " << initialValue;
    }
  };

  bool CommandsBuilder::SignalCreate(const std::string& signalId, uint64_t signalInitialValue)
  {
    commands->Add(new SignalCreateCommand(signalId, signalInitialValue));
    return true;
  }

  class SignalSendCommand : public Command {
  private:
    std::string signalId;
    uint64_t value;

  public:
    SignalSendCommand(const std::string& signalId_, uint64_t value_)
      : signalId(signalId_), value(value_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->SignalSend(signalId, value);
    }

    void Print(std::ostream& out) const {
      out << "signal_send " << signalId << " " << value;
    }
  };

  bool CommandsBuilder::SignalSend(const std::string& signalId, uint64_t signalSendValue)
  {
    commands->Add(new SignalSendCommand(signalId, signalSendValue));
    return true;
  }

  class SignalWaitCommand : public Command {
  private:
    std::string signalId;
    uint64_t value;

  public:
    SignalWaitCommand(const std::string& signalId_, uint64_t value_)
      : signalId(signalId_), value(value_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->SignalWait(signalId, value);
    }

    void Print(std::ostream& out) const {
      out << "signal_wait " << signalId << " " << value;
    }
  };

  bool CommandsBuilder::SignalWait(const std::string& signalId, uint64_t signalExpectedValue)
  {
    commands->Add(new SignalWaitCommand(signalId, signalExpectedValue));
    return true;

  }

  class QueueCreateCommand : public Command {
  private:
    std::string queueId;
    uint32_t size;

  public:
    QueueCreateCommand(const std::string& queueId_, uint32_t size_)
      : queueId(queueId_), size(size_) { }

    virtual bool Execute(runtime::RuntimeState* rt) {
      return rt->QueueCreate(queueId, size);
    }

    void Print(std::ostream& out) const {
      out << "queue_create " << queueId << " " << size;
    }
  };


  bool CommandsBuilder::QueueCreate(const std::string& queueId, uint32_t size)
  {
    commands->Add(new QueueCreateCommand(queueId, size));
    return true;
  }
}

using namespace scenario;

ScenarioTest::ScenarioTest(const std::string& name_, Context* initialContext)
  : TestImpl(initialContext), name(name_)
{
}


void ScenarioTest::Run()
{
  Scenario *scenario = context->Get<Scenario>("scenario");
  RuntimeContext* runtime = context->Runtime();
  std::unique_ptr<RuntimeState> rt(runtime->NewState(context.get()));
  if (!scenario->Execute(rt.get())) { SetFailed(); }
}

}
