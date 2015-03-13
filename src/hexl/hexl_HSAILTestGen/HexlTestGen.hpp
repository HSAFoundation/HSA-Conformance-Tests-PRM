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

#ifndef HEXL_TESTGEN_HPP
#define HEXL_TESTGEN_HPP

#include "HexlTest.hpp"
#include "Brig.h"

namespace hexl {

namespace TestGen {

class TestGenConfig {
private:
  BrigMachineModel8_t model;
  BrigProfile8_t profile;

public:
  TestGenConfig(BrigMachineModel8_t model_, BrigProfile8_t profile_)
    : model(model_), profile(profile_) { }

  static const std::string ID;

  BrigMachineModel8_t Model() const { return model; }
  BrigProfile8_t Profile() const { return profile; }
};

class TestGenTestSet : public TestSet {
private:
  std::string path;
  std::string prefix;
  const BrigOpcode opcode;
  Context* context;

public:
  TestGenTestSet(const std::string& path_, const std::string& prefix_, BrigOpcode opcode_)
    : path(path_), prefix(prefix_), opcode(opcode_) { }

  virtual void InitContext(Context* context) { assert(context); this->context = context; }
  virtual void Name(std::ostream& out) const { out << path << "/" << prefix; }
  virtual void Description(std::ostream& out) const { out << path << "/" << prefix; }
  virtual void Iterate(TestSpecIterator& it);
  virtual TestSet* Filter(TestNameFilter* filter) { return new FilteredTestSet(this, filter); }
  virtual TestSet* Filter(ExcludeListFilter* filter) { return new FilteredTestSet(this, filter); }

};

}

}

#endif // HEXL_TESTGEN_HPP
