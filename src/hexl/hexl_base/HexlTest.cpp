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

#include "HexlTest.hpp"

#include "hsail_c.h"

#include <string>
#include <cassert>
#include <sstream>
#include <cstdio>
#include <fstream>

#include "HexlResource.hpp"
#include "Grid.hpp"

namespace hexl {


const char *TestStatusString(TestStatus status)
{
  switch (status) {
  case PASSED: return "PASSED";
  case FAILED: return "FAILED";
  case ERROR: return "ERROR";
  case NA: return "NA";
  default: assert(false); return "<ErrorTestStatus>";
  }
}

void TestResult::IncStats(AllStats& allStats) const
{
  switch (status) {
  case PASSED: allStats.TestSet().IncPassed(); break;
  case FAILED: allStats.TestSet().IncFailed(); break;
  case ERROR: allStats.TestSet().IncError(); break;
  case NA: allStats.TestSet().IncNa(); break;
  default: assert(false); break;
  }
}

void TestResult::Serialize(std::ostream& out) const
{
  WriteData(out, status);
  WriteData(out, output);
}

void TestResult::Deserialize(std::istream& in)
{
  ReadData(in, status);
  ReadData(in, output);
}

std::string Test::TestName() const
{
  std::ostringstream ss;
  Name(ss);
  return ss.str();
}

void TestImpl::Fail(const std::string& msg)
{
  assert(context);
  SetFailed();
  context->Error() << msg << std::endl;
}

void TestImpl::Serialize(std::ostream& out) const
{
  WriteData(out, Type());
  SerializeData(out);
}

std::string TestImpl::GetOutputName(const std::string& what)
{
  return context->GetOutputName(TestName(), what);
}

bool TestImpl::DumpTextIfEnabled(const std::string& what, const std::string& text)
{
  return context->DumpTextIfEnabled(TestName(), what, text);
}

bool TestImpl::DumpBinaryIfEnabled(const std::string& what, const void *buffer, size_t bufferSize)
{
  return context->DumpBinaryIfEnabled(TestName(), what, buffer, bufferSize);
}

void TestSpec::Run()
{
  if (!IsValid()) { result = TestResult(NA, "Skipped: spec is not valid"); return; }
  Test* test = Create();
  assert(test);
  test->Run();
  result = test->Result();
  delete test;
}

TestSpecList::~TestSpecList()
{
  for (size_t i = 0; i < specs.size(); ++i) {
    assert(specs[i]);
    delete specs[i];
    specs[i] = 0;
  }
}

class AddBaseTestSpecIterator : public TestSpecIterator {
public:
  AddBaseTestSpecIterator(const std::string& base_, TestSpecIterator& it_)
    : base(base_), it(it_) { }

  void operator()(const std::string& path, TestSpec* test)
  {
//    it(base + "/" + path, test);
    it((base.empty() ? "" : base + "/") + path, test);
  }
 
private:
  const std::string& base;
  TestSpecIterator& it;
};

bool TestNameFilter::Matches(const std::string& path, Test* test)
{
  std::string name = path + "/" + test->TestName();
  return Matches(name);
}

bool TestNameFilter::Matches(const std::string& name)
{
  return (name.length() >= namePattern.length()) &&
         (name.substr(0, namePattern.length()) == namePattern);
}

bool ExcludeListFilter::Matches(const std::string& path, Test* test)
{
  std::string name = path + "/" + test->TestName();
  return Matches(name);
}

bool ExcludeListFilter::Matches(const std::string& name)
{
  for (std::string prefix : excludePrefixes) {
    if (name.compare(0, prefix.length(), prefix) == 0) {
      return false;
    }
  }
  return true;
}

bool ExcludeListFilter::Load(ResourceManager* rm, const std::string& name)
{
  std::unique_ptr<std::istream> in(rm->Get(name));
  if (!in.get()) { return false; }
  std::string line;
  while (getline(*in, line)) {
    if (line.empty()) { continue; }
    std::string::iterator last = line.end();
    --last;
    if (*last == '\r') {line.erase(last);}
    if (line.empty()) { continue; }
    AddPrefix(line);
  }
  return true;
}

TestSet* BasicTestSet::Filter(TestNameFilter* filter)
{
  return new FilteredTestSet(this, filter);
}

TestSet* BasicTestSet::Filter(ExcludeListFilter* filter)
{
  return new FilteredTestSet(this, filter);
}

class FilterIterator : public TestSpecIterator {
public:
  FilterIterator(TestSpecIterator& it_, TestFilter* filter_)
    : it(it_), filter(filter_) { }

  void operator()(const std::string& path, TestSpec* test)
  {
    if (filter->Matches(path, test)) {
      it(path, test);
    } else {
      delete test;
    }
  }
 
private:
  TestSpecIterator& it;
  TestFilter* filter;
};


TestSet* FilteredTestSet::Filter(TestNameFilter* filter)
{
  return new FilteredTestSet(parent, new AndFilter(this->filter, filter));
}

TestSet* FilteredTestSet::Filter(ExcludeListFilter* filter)
{
  return new FilteredTestSet(parent, new AndFilter(this->filter, filter));
}

void FilteredTestSet::Iterate(TestSpecIterator& it)
{
  FilterIterator fi(it, filter);
  parent->Iterate(fi);
}

TestSet* OneTest::Filter(TestNameFilter* filter)
{
  if (filter->Matches("", test)) {
    return new OneTest(test);
  } else {
    return new EmptyTestSet();
  }
}

TestSet* OneTest::Filter(ExcludeListFilter* filter)
{
  if (filter->Matches("", test)) {
    return new OneTest(test);
  } else {
    return new EmptyTestSet();
  }
}

void TestSetUnion::InitContext(Context* context)
{
  this->context = context;
  for (std::unique_ptr<TestSet>& ts : testSets) {
    ts->InitContext(context);
  }
}

bool CutTestNamePrefix(const std::string& name, std::string& prefix, std::string& rest, bool allowPartial)
{
  if (!name.empty() && name.compare(0, prefix.length(), prefix) != 0) { return false; }
  rest = name.substr(prefix.length());
  while (rest[0] == '/') { rest = rest.substr(1); }
  return true;
}

TestSet* TestSetUnion::Filter(TestNameFilter* filter)
{
  std::string rest;
  if (filter->NamePattern().empty()) { return this; }
  if (!CutTestNamePrefix(filter->NamePattern(), base, rest)) { return new EmptyTestSet(); }
  if (rest.empty()) { return this; }
  TestNameFilter* filter1 = new TestNameFilter(rest);
  TestSetUnion* ts = new TestSetUnion(base);
  for (unsigned i = 0; i < testSets.size(); ++i) {
    ts->Add(testSets[i]->Filter(filter1));
  }
  return ts;
}

TestSet* TestSetUnion::Filter(ExcludeListFilter* filter)
{
  std::string rest;
  ExcludeListFilter* filter1 = new ExcludeListFilter();
  for (std::string prefix : filter->ExcludePrefixes()) {
    if (CutTestNamePrefix(prefix, base, rest)) {
      if (rest.empty()) {
        delete filter1;
        return new EmptyTestSet();
      } else {
        filter1->AddPrefix(rest);
      }
    }
  }
  TestSetUnion* ts = new TestSetUnion(base);
  for (unsigned i = 0; i < testSets.size(); ++i) {
    ts->Add(testSets[i]->Filter(filter1));
  }
  return ts;
}

void TestSetUnion::Iterate(TestSpecIterator& it)
{
  AddBaseTestSpecIterator ait(base, it);
  for (unsigned i = 0; i < testSets.size(); ++i)
    testSets[i]->Iterate(ait);
}

}
