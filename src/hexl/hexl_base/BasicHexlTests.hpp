/*
   Copyright 2014-2015 Heterogeneous System Architecture (HSA) Foundation

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

#ifndef BASIC_HEXL_TESTS_HPP
#define BASIC_HEXL_TESTS_HPP

#include "HexlTest.hpp"
#include "RuntimeContext.hpp"
#include <memory>
#include <thread>

namespace hexl {

/*
class FinalizeProgramTest : public TestImpl {
public:
  void Run();

protected:
  virtual Program* SetupProgram() = 0;

  RuntimeContextState* state;
};

class FinalizeHsailTest : public FinalizeProgramTest {
protected:
  Program* SetupProgram();
  virtual std::string SetupHsail() = 0;
};

class FinalizeHsailResourceTest : public FinalizeHsailTest {
public:
  FinalizeHsailResourceTest(const std::string hsailName_)
    : hsailName(hsailName_) { }
  FinalizeHsailResourceTest(std::istream& in);

  std::string Type() const { return "finalize"; }
  void Name(std::ostream& out) const { out << hsailName; }
  void Description(std::ostream& out) const { out << "Finalize hsail " << hsailName; }
  void SerializeData(std::ostream& out) const;

protected:
  std::string SetupHsail();
private:
  std::string hsailName;
};

class ValidateDispatchTest : public TestImpl {
public:
  ValidateDispatchTest() : state(0) { }
  void Run();

protected:
  virtual bool StartAgents() { return true; }
  virtual void StopAgents() { }
  virtual bool InitDispatchSetup() = 0;
  virtual bool SetupDispatch(Dispatch* dispatch) = 0;
  virtual Code* SetupCode() = 0;

  RuntimeContextState* state;
  std::unique_ptr<DispatchSetup> dsetup;
};

class ValidateProgramTest : public ValidateDispatchTest {
protected:
  Code* SetupCode();
  virtual Program* SetupProgram() = 0;
};

class ValidateModuleTest : public ValidateProgramTest {
protected:
  Program* SetupProgram();
  virtual Module* SetupModule() = 0;
};

class ValidateBrigTest : public ValidateModuleTest {
public:
  virtual void DumpIfEnabled();
protected:
  Module* SetupModule();
  virtual brig_container_t SetupBrig() = 0;
};

class ValidateBrigContainerTest : public ValidateBrigTest {
public:
  ValidateBrigContainerTest(const std::string& name_, brig_container_t brig_, DispatchSetup* dsetup_)
    : name(name_), brig(brig_) { dsetup.reset(dsetup_); assert(brig); }
  ValidateBrigContainerTest(std::istream& in) { DeserializeData(in); }
  ~ValidateBrigContainerTest();
  std::string Type() const { return "validate_brig_container"; }
  void Name(std::ostream& out) const { out << name; }
  void Description(std::ostream& out) const { out << "Validate Brig " << name; }
  virtual void PrintExtraInfo(std::ostream &s) const;
  virtual void SerializeData(std::ostream& out) const;
  virtual void DumpIfEnabled();

protected:
  brig_container_t SetupBrig() { return brig; }
  virtual bool InitDispatchSetup() { return true; }
  virtual bool SetupDispatch(Dispatch* dispatch);

private:
  std::string name;
  brig_container_t brig;

  void DeserializeData(std::istream& in);
};
*/

}

#endif // BASIC_HEXL_TESTS_HPP
