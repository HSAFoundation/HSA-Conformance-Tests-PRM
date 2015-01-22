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

#include "HexlTestRunner.hpp"
#include "Stats.hpp"
#include "HexlTest.hpp"
#include "HexlResource.hpp"
#include <sstream>
#include "Utils.hpp"

namespace hexl {

TestRunnerBase::TestRunnerBase(Context* context_)
  : context(context_), testContext(0)
{
}

void TestRunnerBase::Init()
{
  if (!context->Has("hexl.log.stream.debug")) {
    context->Put("hexl.log.stream.debug", TestOut());
    context->Put("hexl.log.stream.info", TestOut());
    context->Put("hexl.log.stream.error", TestOut());
  }
}

void TestRunnerBase::RunTest(const std::string& path, Test* test)
{
  assert(test);
  Init();
  BeforeTest(path, test);
  if (testContext->Opts()->GetString("rt") != "none") {
    clock_t t_begin, t_end;
    t_begin = clock();
    TestResult result = ExecuteTest(test);
    t_end = clock();
    result.SetTime(t_begin, t_end);
    AfterTest(path, test, result);
  }
}

void TestRunnerBase::BeforeTest(const std::string& path, Test* test)
{
  testContext = new Context(context);
  std::string fullPath = path + "/" + test->TestName();
  testContext->Put("hexl.outputPath", fullPath);
  test->InitContext(testContext);
  test->DumpIfEnabled();
}

void TestRunnerBase::AfterTest(const std::string& path, Test* test, const TestResult& result)
{
  result.IncStats(stats);
  delete testContext; testContext = 0;
}

bool TestRunnerBase::RunTests(TestSet& tests)
{
  Init();
  if (!BeforeTestSet(tests)) { return false; }
  TestSpecList specs;
  tests.Iterate(specs);
  for (size_t i = 0; i < specs.Count(); ++i) {
    std::string path = specs.GetPath(i);
    TestSpec* spec = specs.GetSpec(i);
    spec->InitContext(context);
    Test *test = spec->Create();
    RunTest(path, test);
    if (test) { delete test; }
  }
  if (!AfterTestSet(tests)) { return false; }
  return true;
}

TestResult TestRunnerBase::ExecuteTest(Test* test)
{
  test->Run();
  return test->Result();
}

bool SimpleTestRunner::AfterTestSet(TestSet& testSet)
{
  std::cout << "Testrun statistics:" << std::endl;
  IndentStream indent(std::cout);
  stats.PrintTestSet(std::cout);
  return true;
}

void SimpleTestRunner::BeforeTest(const std::string& path, Test* test)
{
  TestRunnerBase::BeforeTest(path, test);
  std::string fullTestName = path + "/" + test->TestName();
  std::cout << "START:  " << fullTestName << std::endl;
  if (testContext->IsVerbose("description")) {
    std::cout << "Test description:" << std::endl;
    IndentStream indent(std::cout);
    test->Description(std::cout);
  }
  if (testContext->IsVerbose("context")) {
    std::cout << "Test context:" << std::endl;
    IndentStream indent(std::cout);
    test->GetContext()->Print(std::cout);
  }
  if (testContext->IsDumpEnabled("hxl", false)) {
    std::ostream* out = testContext->RM()->GetOutput(testContext->GetOutputName(fullTestName, "hxl"));
    if (out) {
      test->Serialize(*out);
      delete out;
    }
  }
  testContext->Env()->Stats().Clear();
}

void SimpleTestRunner::AfterTest(const std::string& path, Test* test, const TestResult& result)
{
  std::string fullTestName = path + "/" + test->TestName();
  std::cout <<
    result.StatusString() << ": " <<
    fullTestName << std::endl;
  std::cout << std::endl;
  TestRunnerBase::AfterTest(path, test, result);
}

HTestRunner::HTestRunner(Context* context_)
  : TestRunnerBase(context_)
{
  testLogLevel = context->Opts()->GetUnsigned("testloglevel", 4);
}

void HTestRunner::BeforeTest(const std::string& path, Test* test)
{
  TestRunnerBase::BeforeTest(path, test);
  testOut.clear();
  std::string fullName = path + "/" + test->TestName();
  std::string cpath = ExtractTestPath(fullName, testLogLevel);
  if (cpath != pathPrev) {
    if (!pathPrev.empty()) {
      RunnerLog() << "  ";
      pathStats.TestSet().PrintShort(RunnerLog());
      RunnerLog() << std::endl;
    }
    pathStats.Clear();
    RunnerLog() << cpath << "  " << std::endl;
    pathPrev = cpath;
  }
}

bool HTestRunner::BeforeTestSet(TestSet& testSet)
{
  std::string testLogName = context->Opts()->GetString("testlog", "test.log");
  testLog.open(testLogName.c_str(), std::ofstream::out);
  if (!testLog.is_open()) {
    context->Env()->Error("Failed to open test log %s", testLogName.c_str());
    return false;
  }
  return true;
}

bool HTestRunner::AfterTestSet(TestSet& testSet)
{
  testLog.close();
  // Process first path.
  RunnerLog() << "  ";
  pathStats.TestSet().PrintShort(RunnerLog());
  RunnerLog() << std::endl;

  // Totals
  RunnerLog() << std::endl << "Testrun" << std::endl << "  ";
  Stats().TestSet().PrintShort(RunnerLog()); RunnerLog() << std::endl;
  return true;
}

void HTestRunner::AfterTest(const std::string& path, Test* test, const TestResult& result)
{
  if (!result.IsPassed() || context->Opts()->IsSet("hexl.log.test.all")) {
    testLog << testOut.str();
  }
  std::string fullTestName = path + "/" + test->TestName();
  testLog <<
    result.StatusString() << ": " <<
    fullTestName << " " << std::setprecision(2) <<
    result.ExecutionTime() << "s" << std::endl;
  testLog << std::endl;
  result.IncStats(pathStats);
  testOut.str(std::string());
  TestRunnerBase::AfterTest(path, test, result);
}

}
