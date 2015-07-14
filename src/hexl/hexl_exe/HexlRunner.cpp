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

#include "BasicHexlTests.hpp"
#include "Stats.hpp"
#include "HexlTestRunner.hpp"
#include "HexlTestList.hpp"
#include "HexlTestFactory.hpp"
#ifdef ENABLE_HEXL_LUA
#include "LuaHexlTest.hpp"
#include "LuaTestFactory.hpp"
#endif // ENABLE_HEXL_LUA
#include "Options.hpp"
#include "HexlResource.hpp"
#include "HexlLib.hpp"
#ifdef ENABLE_HEXL_AGENT
#include "HexlAgent.hpp"
#endif // ENABLE_HEXL_AGENT

#include <fstream>
#include <iostream>

namespace hexl {

class HexlRunner {
public:
  HexlRunner(int argc, char **argv);
  void Run();

private:
  int argc;
  char **argv;
  Options options;
  std::unique_ptr<Context> context;
  std::unique_ptr<TestFactory> testFactory;
  TestRunner* testRunner;

  int ParseOptions();
  TestSet* CreateTestSet(const size_t i = 0);
};

HexlRunner::HexlRunner(int argc_, char **argv_)
  : argc(argc_),
    argv(argv_),
    context(new Context()),
#ifdef ENABLE_HEXL_LUA
    testFactory(new LuaTestFactory())
#else // ENABLE_HEXL_LUA
    testFactory(new DefaultTestFactory())
#endif // ENABLE_HEXL_LUA
{
}

int HexlRunner::ParseOptions()
{
  OptionRegistry optReg;

  optReg.RegisterOption("rt");
#ifdef ENABLE_HEXL_AGENT
  optReg.RegisterOption("agent");
  optReg.RegisterOption("remote");
#endif // ENABLE_HEXL_AGENT
  optReg.RegisterOption("hxl");
  optReg.RegisterOption("test");
  optReg.RegisterMultiOption("testlist");
  optReg.RegisterOption("tests");
  optReg.RegisterOption("hsail");
  optReg.RegisterOption("brig");
  optReg.RegisterOption("lua");
  optReg.RegisterOption("kernel");
  optReg.RegisterOption("testbase");
  optReg.RegisterOption("results");
  optReg.RegisterOption("key");
  optReg.RegisterBooleanOption("dummy");
  optReg.RegisterBooleanOption("verbose");
  optReg.RegisterBooleanOption("dump");
  optReg.RegisterOption("match");
  optReg.RegisterOption("testlog");
  optReg.RegisterOption("rtlib");
  optReg.RegisterOption("timeout");
  int n;
  if ((n = hexl::ParseOptions(argc, argv, optReg, options)) != 0) {
    std::cout << "Invalid option: " << argv[n] << std::endl;
    return 4;
  }
  if (!options.IsSet("test") && !options.IsSet("testlist") && !options.IsSet("tests") && !options.IsSet("hxl") && !options.IsSet("agent")) {
    std::cout << ("test/testlist/tests/hxl/agent option is not set") << std::endl;
    return 5;
  }
  {
    std::string match = options.GetString("match", "");
    if (match.length() > 0 && match[0] == '!' && match.length() <= 1) {
      std::cout << "Bad -match: '" << match << "'" << std::endl;
      return 6;
    }
  }
  {
    std::string key = options.GetString("key", "");
    if (key.length() > 0 && key[0] == '!' && key.length() <= 1) {
      std::cout << "Bad -key: '" << key << "'" << std::endl;
      return 7;
    }
  }
  return 0;
}

TestSet* HexlRunner::CreateTestSet(const size_t i)
{
  if (options.IsSet("testlist")) {
    if (!options.IsSet("test")) { context->Error() << "test is not set" << std::endl; return 0; }
    std::string testType = options.GetString("test");
    const Options::MultiString* t = options.GetMultiString("testlist"); assert(t);
    SimpleTestList* testList = new SimpleTestList(t->at(i), testFactory.get(), testType, options.GetString("key", ""));
    if (!testList->ReadFrom(context->RM(), t->at(i))) { delete testList; return 0; }
    return testList;
  } else if (options.IsSet("tests")) {
    std::string tests = options.GetString("tests"); /// \todo Is it sensible to support multiple testlists here?
    return testFactory->CreateTestSet(tests);
  } else if (options.IsSet("test")) {
    std::string testType = options.GetString("test");
    Test* test = testFactory->CreateTest(testType, "test", options);
    return new OneTest(test);
  } else if (options.IsSet("hxl")) {
    std::string hxl = options.GetString("hxl");
    std::istream* in = context->RM()->Get(hxl);
    if (!in) { return 0; }
    Test* test = testFactory->CreateTest(*in);
    delete in;
    return new OneTest(test);
  } else {
    assert(false); // Should have returned error earlier.
    return 0;
  }
}

void HexlRunner::Run()
{
  int result = 4;
  result = ParseOptions();
  context->Put("hexl.options", &options);
  context->Put("hexl.stats", new AllStats());
  ResourceManager* rm = new DirectoryResourceManager(options.GetString("testbase", "."), options.GetString("results", "."));
  context->Put("hexl.rm", rm);
  runtime::RuntimeContext* runtime = CreateRuntimeContext(context.get());
  if (runtime) {
    std::cout << "Runtime: " << runtime->Description() << std::endl;
  } else {
    result = 17;
  }
  context->Put("hexl.runtime", runtime);
  context->Put("hexl.testFactory", testFactory.get());
  if (result == 0) {
#ifdef ENABLE_HEXL_AGENT
    if (options.IsSet("remote")) {
      RemoteTestRunner* remoteTestRunner = new RemoteTestRunner(context, options.GetString("remote"));
      if (!remoteTestRunner->Connect()) {
        result = 19;
      } else {
        testRunner = remoteTestRunner;
      }
    } else 
#endif // ENABLE_HEXL_AGENT
    {
      testRunner = new SimpleTestRunner(context.get());
    }
#ifdef ENABLE_HEXL_AGENT
    if (options.IsSet("agent")) {
      Agent agent(testRunner, testFactory, options.GetString("agent"));
      agent.Loop();
      return;
    }
#endif // ENABLE_HEXL_AGENT
    if (options.IsSet("testlist")) {
      /// \todo Technically, this is a patch (to support intermediate stats for multi-testlist case)
      const Options::MultiString* t = options.GetMultiString("testlist"); assert(t);
      AllStats total; size_t testSetIndex = 0;
      for (; testSetIndex < t->size(); ++testSetIndex) {
        TestSet* testSet = CreateTestSet(testSetIndex);
        if (!testSet) {
          context->Error() << "Failed to create testset from '" << t->at(testSetIndex).c_str() << "'" << std::endl;
          total.TestSet().IncError();
          break;
        }
        testRunner->RunTests(*testSet);
        total.Append(testRunner->Stats());
        testRunner->Stats().Clear();
      }
      if (t->size() > 1) {
        std::cout << "TOTAL STATISTICS (" << testSetIndex << " testlists out of " << t->size() << "):" << std::endl;
        total.PrintTestSet(std::cout);
        std::cout << std::endl;
      }
      if (result == 0 && !total.TestSet().AllPassed()) { result = 10; }
    } else {
      TestSet* testSet = CreateTestSet();
      if (!testSet) { context->Error() << "Failed to create testset" << std::endl; result = 5; }
      testRunner->RunTests(*testSet);
      if (result == 0 && !context->Stats().TestSet().AllPassed()) { result = 10; }
    }
  }
  if (runtime) { delete runtime; }
  if (rm) { delete rm; }
  exit(result);
}

}

int main(int argc, char **argv)
{
  hexl::HexlRunner(argc, argv).Run();
  return 0; /// \todo Never reached.
}
