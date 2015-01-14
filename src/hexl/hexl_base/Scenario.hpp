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
#include <memory>

namespace hexl {

namespace scenario {

class Command;
class StartThreadCommand;

struct Defaults {
  static const std::string PROGRAM_ID;
  static const std::string MODULE_ID;
  static const std::string BRIG_ID;
  static const std::string CODE_ID;
  static const std::string DISPATCH_SETUP_ID;
  static const std::string SCENARIO_ID;
  static const std::string SIGNAL_ID;
  static const std::string SIGNAL_ATOMIC_ID;
};

class CommandSequence {
private:
  unsigned id;
  Context *context;
  std::vector<std::unique_ptr<Command>> commands;

  void Add(Command* command);
  void SetContext(Context *context) { this->context = context; }

public:
  CommandSequence(unsigned id_): id(id_) {}
  // Methods
  bool Run(Context *context);
  bool Finish(Context *context);
  void Print(std::ostream& out) const;

  // Commands
  void CreateProgram(const std::string& programId = Defaults::PROGRAM_ID);
  void AddBrigModule(const std::string& programId = Defaults::PROGRAM_ID, const std::string& moduleId = Defaults::MODULE_ID, const std::string& brigId = Defaults::BRIG_ID); 
  void ValidateProgram(const std::string& programId = Defaults::PROGRAM_ID);
  void Finalize(const std::string& codeId = Defaults::CODE_ID, const std::string& programId = Defaults::PROGRAM_ID, uint32_t kernelOffset = 0);
  void Dispatch(const std::string& codeId = Defaults::CODE_ID, const std::string& dispatchSetupId = Defaults::DISPATCH_SETUP_ID);
  void StartThread(unsigned id);

  // HSAIL specific commands
  // todo: signalInitialValue, signalExpectedValue should be of hsa_signal_value_t type
  // m.b. better create derived class SignalScenario in HsailRuntime to hide HSAIL specifics?
  void CreateSignal(const std::string& signalId, uint64_t signalInitialValue = 1);
  void SendSignal(const std::string& signalId, uint64_t signalSendValue = 1);
  void WaitSignal(const std::string& signalId, uint64_t signalExpectedValue = 1);
  void CreateQueue(const std::string& queueId, uint32_t size = 0);
};

class Command {
protected:
  Context* context;

public:
  Command() : context(0) {}
  virtual ~Command() { }
  void SetContext(Context* context) { this->context = context; }
  virtual bool Run() = 0;
  virtual bool Finish() { return true; }
  virtual void Print(std::ostream& out) const { }
};

class Scenario {
private:
  std::vector<std::unique_ptr<CommandSequence>> commandSequences;

public:
  // Methods
  bool Run(Context* context);
  void Print(std::ostream& out);
  CommandSequence& Commands(unsigned id = 0) { for (size_t i = commandSequences.size(); i <= id; ++i) { commandSequences.push_back(std::unique_ptr<CommandSequence>(new CommandSequence(id))); }; return *commandSequences[id]; }
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

}

#endif // HEXL_SCENARIO_HPP
