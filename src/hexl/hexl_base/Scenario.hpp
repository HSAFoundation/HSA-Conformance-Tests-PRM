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

#ifndef HEXL_SCENARIO_HPP
#define HEXL_SCENARIO_HPP

#include "MObject.hpp"
#include "HexlTest.hpp"
#include "RuntimeCommon.hpp"
#include <memory>

namespace hexl {

namespace scenario {
  using runtime::Command;

  class CommandSequence : public Command {
  private:
    std::vector<std::unique_ptr<Command>> commands;

  public:
    void Add(Command* command);
    virtual void Print(std::ostream& out) const override;
    bool Execute(runtime::RuntimeState* runtime) override;
    bool Finish(runtime::RuntimeState* runtime) override;
  };

  class Scenario {
  private:
    std::vector<std::unique_ptr<CommandSequence>> commands;

  public:
    CommandSequence* Commands(unsigned id = 0);
    void AddCommands(CommandSequence* commands);
    bool Execute(runtime::RuntimeState* runtime);
    bool Finish(runtime::RuntimeState* runtime);
    void Print(std::ostream& out) const;

    static Scenario* Get(Context* context) { return context->Get<Scenario>("scenario"); }
  };

  class CommandsBuilder : public runtime::RuntimeState {
  private:
    Context* initialContext;
    std::unique_ptr<CommandSequence> commands;

  public:
    CommandsBuilder(Context* initialContext_);

    Context* GetContext() { return initialContext; }

    CommandSequence* ReleaseCommands();

    bool StartThread(unsigned id, Command* commandToRun = 0);
    bool WaitThreads();

    bool ModuleCreateFromBrig(const std::string& moduleId = "module", const std::string& brigId = "brig");

    bool ProgramCreate(const std::string& programId = "program");
    bool ProgramAddModule(const std::string& programId = "program", const std::string& moduleId = "module");
    bool ProgramFinalize(const std::string& codeId = "code", const std::string& programId = "program");

    bool ExecutableCreate(const std::string& executableId = "executable");
    bool ExecutableLoadCode(const std::string& executableId = "executable", const std::string& codeId = "code");
    bool ExecutableFreeze(const std::string& executableId = "executable");

    bool BufferCreate(const std::string& bufferId, size_t size, const std::string& initValuesId);
    bool BufferValidate(const std::string& bufferId, const std::string& expectedValuesId, const std::string& method = "");

    bool ImageCreate(const std::string& imageId, const std::string& imageParamsId, bool optionalFormat);
    bool ImageInitialize(const std::string& imageId, const std::string& imageParamsId, const std::string& initValueId);
    bool ImageWrite(const std::string& imageId, const std::string& writeValuesId, const ImageRegion& region);
    bool ImageValidate(const std::string& imageId, const std::string& expectedValuesId, const std::string& method = "");
    bool SamplerCreate(const std::string& samplerID, const std::string& samplerParamsId);

    bool DispatchCreate(const std::string& dispatchId = "dispatch", const std::string& executableId = "executable", const std::string& kernelName = "");
    bool DispatchSet(const std::string& dispatchId, const std::string& key, const std::string& value);
    virtual bool DispatchArg(const std::string& dispatchId, runtime::DispatchArgType argType, const std::string& argKey) override;
    bool DispatchExecute(const std::string& dispatchId = "dispatch");

    bool SignalCreate(const std::string& signalId, uint64_t signalInitialValue = 1);
    bool SignalSend(const std::string& signalId, uint64_t signalSendValue = 1);
    bool SignalWait(const std::string& signalId, uint64_t signalExpectedValue = 1);

    bool QueueCreate(const std::string& queueId, uint32_t size = 0);
  };

  class ScenarioBuilder {
  private:
    Context* initialContext;
    std::vector<std::unique_ptr<CommandsBuilder>> commands;

  public:
    ScenarioBuilder(Context* initialContext_)
      : initialContext(initialContext_) { }
    CommandsBuilder* Commands(unsigned id = 0);
    Scenario* ReleaseScenario();
  };

}

class ScenarioTest : public TestImpl {
private:
  std::string name;
  scenario::Scenario* scenario;

public:
  ScenarioTest(const std::string& name_, Context* initialContext);
  std::string Type() const { return "scenario_test"; }
  void Name(std::ostream& out) const { out << name; }
  void Description(std::ostream& out) const { }
  void Run();
};

  template <>
  inline void Print(const scenario::Scenario& o, std::ostream& out) { o.Print(out); }

}

#endif // HEXL_SCENARIO_HPP
