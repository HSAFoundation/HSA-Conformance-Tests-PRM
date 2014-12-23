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

#ifndef HEXL_TEST_LIST_HPP
#define HEXL_TEST_LIST_HPP

#include "HexlTest.hpp"
#include <vector>
#include <set>

namespace hexl {

class ResourceManager;
class TestFactory;

class SimpleTestList : public TestSet {
public:
  SimpleTestList(const std::string& name_, TestFactory* testFactory_, const std::string& testType_, const std::string& key_)
    : name(name_), testFactory(testFactory_), testType(testType_), key(key_) { }
  void Iterate(TestSpecIterator& it);
  void InitContext(Context *context) { }
  void Name(std::ostream& out) const { out << name; }
  void Description(std::ostream& out) const { out << name; }
  void Name(TestSpec* test, std::ostream& out) const { out << name << ":"; test->Name(out); }
  void AddTest(const std::string& name) { testNames.push_back(name); }
  bool ReadFrom(ResourceManager* rm, const std::string& name);
  TestSet* Filter(TestNameFilter* filter) { return new FilteredTestSet(this, filter); }
  TestSet* Filter(ExcludeListFilter* filter) { return new FilteredTestSet(this, filter); }

private:
  std::string name;
  std::vector<std::string> testNames;
  TestFactory* testFactory;
  std::string testType;
  const std::string key;
  int readFromNestingDepth;
  
  bool ReadFromImpl(ResourceManager* rm, const std::string& name);
  bool Parse(std::string line, const unsigned lineNumber, const std::string& testlist, std::string * const testname,
             std::set<std::string> * const keywords, std::string * const nestedTestlist) const;
  bool IsMatchKey(const std::set<std::string>& keywords) const;
  
  friend class SimpleTestSpec;
};

}
#endif // HEXL_TEST_LIST_HPP
