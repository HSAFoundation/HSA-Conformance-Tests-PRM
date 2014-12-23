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

#ifndef HEXL_TEST_FACTORY_HPP
#define HEXL_TEST_FACTORY_HPP

#include <string>
#include <iostream>
#include <cstdint>
#include "Options.hpp"

namespace hexl {

class Test;
class TestSet;
class HostAgent;
class RuntimeContext;
class Context;

class TestFactory {
public:
  virtual ~TestFactory() { }
  virtual Test* CreateTest(const std::string& type, const std::string& name, const Options& options = Options()) = 0;
  Test* CreateTest(std::istream& in);
  virtual Test* CreateTestDeserialize(const std::string& type, std::istream& in) = 0;
  virtual TestSet* CreateTestSet(const std::string& type) = 0;
  void Serialize(std::ostream& out, const Test* test);
//  void Deserialize(std::istream& in, Test*& test);
};

class DefaultTestFactory : public TestFactory {
public:
  virtual Test* CreateTest(const std::string& type, const std::string& name, const Options& options);
  virtual Test* CreateTestDeserialize(const std::string& type, std::istream& in);
  virtual TestSet* CreateTestSet(const std::string& type) { return 0; }
};

}

#endif // HEXL_TEST_FACTORY_HPP
