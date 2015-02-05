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

#include "BasicHexlTests.hpp"

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "HexlResource.hpp"
#include "HexlTestFactory.hpp"
#include "Utils.hpp"

namespace hexl {

void FinalizeProgramTest::Run()
{
  assert(!state && context && context->Runtime());
  Program* program = 0;
  Code* code = 0;
  do {
    state = context->Runtime()->NewState(context.get());
    if (!state) { Fail("Failed to create runtime context state"); break; }
    program = SetupProgram();
    if (!program) { Fail("Failed to setup program"); break; }
    std::string errMsg;
    if (!program->Validate(errMsg)) { context->Error() << errMsg << std::endl; Fail("BRIG failed validation"); break; }
    code = state->Finalize(program);
    if (!code) { Fail("Failed to finalize program"); break; }
  } while (0);
  if (code) delete code;
  if (program) delete program;
  if (state) { delete state; state = 0; }
}

Program* FinalizeHsailTest::SetupProgram()
{
  std::string hsailText = SetupHsail();
  if (hsailText.empty()) { Fail("Failed to setup hsail"); return 0; }
  DumpTextIfEnabled("hsail", hsailText);
  assert(state);
  Program* program = state->NewProgram();
  if (!program) { Fail("Failed to setup program"); return 0; }
  Module* module = state->NewModuleFromHsailText(hsailText); /// \todo who owns module?
  if (!module) {
    Fail("Failed to create module from hsail");
    delete program;
    return 0;
  }
  return program;
}

FinalizeHsailResourceTest::FinalizeHsailResourceTest(std::istream& in)
{
  ReadData(in, hsailName);
}

void FinalizeHsailResourceTest::SerializeData(std::ostream& out) const
{
  WriteData(out, hsailName);
}

std::string FinalizeHsailResourceTest::SetupHsail()
{
  assert(context);
  return LoadTextResource(context->RM(), hsailName);
}

void ValidateDispatchTest::Run()
{
  assert(!state);
  Dispatch* dispatch = 0;
  Code* code = 0;
  do {
    assert(context && context->Runtime());
    state = context->Runtime()->NewState(context.get());
    if (!state) { Fail("Failed to create runtime context state"); break; }
    if (!StartAgents()) { Fail("Failed to setup agents"); break; }
    if (!InitDispatchSetup()) { Fail("Failed to init dispatch"); break; }
    code = SetupCode();
    if (!code) { Fail("Failed to setup code"); break; }
    dispatch = state->NewDispatch(code);
    if (!dispatch) { Fail("Failed to create dispatch"); break; }
    if (!SetupDispatch(dispatch)) { Fail("Failed to setup dispatch"); break; }
    context->Info() << "Executing dispatch" << std::endl;
    if (!dispatch->Execute()) { Fail("Failed to execute dispatch"); break; }
    context->Info() << "Returned from dispatch" << std::endl;
    StopAgents();
    context->Info() << "Validating results " << std::endl;
    if (dispatch->MState()->Validate() != 0) { SetFailed(); }
  } while (0);
  if (code) { delete code; }
  if (state) { delete state; state = 0; }
}

bool ValidateDispatchTest::SetupDispatch(Dispatch* dispatch)
{
  dispatch->Apply(*dsetup);
  return true;
}

Code* ValidateProgramTest::SetupCode()
{
  Program* program = SetupProgram();
  if (!program) { Fail("Failed to setup program"); return 0; }
  assert(state);
  std::string errMsg;
  if (!program->Validate(errMsg)) { context->Error() << errMsg << std::endl; Fail("BRIG failed validation"); return 0; }
  Code* code = state->Finalize(program, 0, dsetup.get());
  if (!code) {
    Fail("Failed to finalize program");
    delete program;
    return 0;
  }
  return code;
}

Program* ValidateModuleTest::SetupProgram()
{
  assert(state);
  Program* program = state->NewProgram();
  if (!program) { Fail("Failed to create program"); return 0; }
  Module* module = SetupModule();
  if (!module) {
    Fail("Failed to setup module");
    delete program;
    return 0;
  }
  program->AddModule(module); /// \todo delete module?
  return program;
}

Module* ValidateBrigTest::SetupModule()
{
  brig_container_t brig = SetupBrig();
  if (!brig) { Fail("Failed to setup brig"); return 0; }
  assert(state);
  return state->NewModuleFromBrig(brig);
}

void ValidateBrigTest::DumpIfEnabled()
{
  brig_container_t brig = SetupBrig();
  if (brig) context->DumpBrigIfEnabled(TestName(), brig);
}

ValidateBrigContainerTest::~ValidateBrigContainerTest()
{
}

bool ValidateBrigContainerTest::SetupDispatch(Dispatch* dispatch)
{
  dispatch->Apply(*dsetup);
  return true;
}

void ValidateBrigContainerTest::PrintExtraInfo(std::ostream &s) const {
  const char *filename = tmpnam(NULL);
  brig_container_disassemble_to_file(brig, filename);
  s << LoadFile(filename);
  remove(filename);
}

void ValidateBrigContainerTest::DumpIfEnabled()
{
  ValidateBrigTest::DumpIfEnabled();
  const DispatchSetup* const dsetup_ = dsetup.get();
  if (dsetup_) context->DumpDispatchsetupIfEnabled(TestName(), dsetup_);
}

void ValidateBrigContainerTest::SerializeData(std::ostream& out) const
{
  WriteData(out, TestName());
  WriteData(out, *dsetup);
  WriteData(out, brig);
}

void ValidateBrigContainerTest::DeserializeData(std::istream& in)
{
  ReadData(in, name);
  dsetup.reset(new DispatchSetup());
  ReadData(in, *dsetup);
  ReadData(in, brig);
}

}
