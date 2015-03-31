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

#ifndef HEXL_TEST_RUNNER_HPP
#define HEXL_TEST_RUNNER_HPP

#include "HexlTest.hpp"
#include <sstream>
#include <fstream>

namespace hexl {

class Context;

class TestRunner {
public:
  virtual ~TestRunner() { }
  virtual void RunTest(const std::string& path, Test* test) = 0;
  virtual bool RunTests(TestSet& tests) = 0;
  virtual const AllStats& Stats() const = 0;
  virtual AllStats& Stats() = 0;
};

class TestRunnerBase : public TestRunner {
private:
  clock_t t_begin, t_end;

protected:
  Context* context;
  Context* testContext;
  AllStats stats;

  virtual void Init();
  virtual bool BeforeTestSet(TestSet& testSet) { return true; }
  virtual bool AfterTestSet(TestSet& testSet) { return true; }
  virtual void BeforeTest(const std::string& path, Test* test);
  virtual void AfterTest(const std::string& path, Test* test, const TestResult& result);
  virtual std::ostream* TestOut() { return &std::cout; }
  virtual TestResult ExecuteTest(Test* test);
  const AllStats& Stats() const { return stats; }
  AllStats& Stats() { return stats; }

public:
  TestRunnerBase(Context* context_);
  virtual void RunTest(const std::string& path, Test* test);
  virtual void RunTestSpec(const std::string& path, TestSpec* spec);
  virtual bool RunTests(TestSet& tests);
};

class SimpleTestRunner : public TestRunnerBase {
protected:
  virtual bool AfterTestSet(TestSet& testSet);
  virtual void AfterTest(const std::string& path, Test* test, const TestResult& result);

public:
  SimpleTestRunner(Context* context_)
    : TestRunnerBase(context_) { }
};

class HTestRunner : public TestRunnerBase {
private:
  std::string pathPrev;
  std::ostringstream testOut;
  std::ofstream testLog;
  AllStats pathStats;
  unsigned testLogLevel;

protected:
  std::ostream& RunnerLog() { return std::cout; }
  std::ostream* TestOut() { return &testOut; }
  virtual bool BeforeTestSet(TestSet& testSet);
  virtual bool AfterTestSet(TestSet& testSet);
  virtual void BeforeTest(const std::string& path, Test* test);
  virtual void AfterTest(const std::string& path, Test* test, const TestResult& result);

public:
  HTestRunner(Context* context_);
};

}

#endif // HEXL_TEST_RUNNER_HPP
