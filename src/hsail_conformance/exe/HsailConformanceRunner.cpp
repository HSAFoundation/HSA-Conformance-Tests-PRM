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

#include "Options.hpp"
#include "HexlTestFactory.hpp"
#include "HexlTestRunner.hpp"
#include <iostream>
#include <memory>
#include "HexlResource.hpp"

#include "PrmCoreTests.hpp"
#include "ImageCoreTests.hpp"
#include "HexlLib.hpp"
#ifdef ENABLE_HEXL_AGENT
#include "HexlAgent.hpp"
#endif // ENABLE_HEXL_AGENT
#include "CoreConfig.hpp"
#include "SignalTests.hpp"

using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

class HCTestFactory : public DefaultTestFactory {
private:
  Context* context;
  hexl::TestSet* prmCoreTests;
  hexl::TestSet* imageCoreTests;

public:
  HCTestFactory(Context* context_)
    : context(context_), prmCoreTests(NewPrmCoreTests()), imageCoreTests(NewImagesCoreTests())
  {
  }

  ~HCTestFactory()
  {
    delete prmCoreTests;
    delete imageCoreTests;
  }

  virtual Test* CreateTest(const std::string& type, const std::string& name, const Options& options = Options())
  {
    assert(false);
    return 0;
  }

  virtual TestSet* CreateTestSet(const std::string& type)
  {
    TestSet* ts;
    ts = prmCoreTests;
    ts->InitContext(context);
    if (type == "images") {
        ts = imageCoreTests;
        ts->InitContext(context);
    }
    else
    {
      if (type != "all") {
        TestNameFilter* filter = new TestNameFilter(type);
        TestSet* fts = prmCoreTests->Filter(filter);
        if (fts != ts) {
          fts->InitContext(context);
          ts = fts;
        }
      }
    }
    return ts;
  }
};

class HCRunner {
public:
  HCRunner(int argc_, char **argv_)
    : argc(argc_), argv(argv_), context(new Context()),
      testFactory(new HCTestFactory(context.get())), runner(0),
      coreConfig(new CoreConfig())
  {
    context->Put(CoreConfig::CONTEXT_KEY, coreConfig.get());
  }
  ~HCRunner() { delete testFactory; }

  void Run();

private:
  int argc;
  char **argv;
  std::unique_ptr<Context> context;
  Options options;
  TestFactory* testFactory;
  TestRunner* runner;
  std::unique_ptr<CoreConfig> coreConfig;
  TestRunner* CreateTestRunner();
  TestSet* CreateTestSet();
};

TestRunner* HCRunner::CreateTestRunner()
{
  std::string runner = options.GetString("runner");
#ifdef ENABLE_HEXL_AGENT
  if (options.IsSet("remote")) {
    if (options.IsSet("runner") && runner != "remote") {
      std::cout << "Runner should be set to remote for -remote" << std::endl;
      exit(20);
    }
    RemoteTestRunner* remoteTestRunner = new RemoteTestRunner(context, options.GetString("remote"));
    if (!remoteTestRunner->Connect()) {
      exit(19);
    }
    return remoteTestRunner;
  } else
#endif // ENABLE_HEXL_AGENT
  if (runner == "hrunner" || runner.empty()) {
    return new HTestRunner(context.get());
  } else if (runner == "simple") {
    return new SimpleTestRunner(context.get());
  } else {
    std::cout << "Unsupported runner: " << runner << std::endl;
    exit(20);
  }
}

TestSet* HCRunner::CreateTestSet()
{
  TestSet* ts = testFactory->CreateTestSet(options.GetString("tests"));
  ts->InitContext(context.get());
  if (options.IsSet("exclude")) {
    ExcludeListFilter* filter = new ExcludeListFilter();
    filter->Load(context->RM(), options.GetString("exclude"));
    TestSet* fts = filter->Filter(ts);
    if (fts != ts) {
      ts->InitContext(context.get());
      ts = fts;
    }
  }
  return ts;
}

void HCRunner::Run()
{
  OptionRegistry optReg;
  optReg.RegisterOption("rt");
  optReg.RegisterOption("runner");
  optReg.RegisterOption("remote");
  optReg.RegisterOption("testbase");
  optReg.RegisterOption("results");
  optReg.RegisterOption("tests");
  optReg.RegisterOption("testloglevel");
  optReg.RegisterOption("testlog");
  optReg.RegisterOption("exclude");
  optReg.RegisterBooleanOption("dummy");
  optReg.RegisterBooleanOption("verbose");
  optReg.RegisterBooleanOption("dump");
  optReg.RegisterOption("match");
  {
    int n = hexl::ParseOptions(argc, argv, optReg, options);
    if (n != 0) {
      std::cout << "Invalid option: " << argv[n] << std::endl;
      exit(4);
    }
    if (!options.IsSet("tests")) {
      std::cout << "tests option is not set" << std::endl;
      exit(5);
    }
    std::string match = options.GetString("match", "");
    if (match.length() > 0 && match[0] == '!' && match.length() <= 1) {
      std::cout << "Bad -match: '" << match << "'" << std::endl;
      exit(6);
    }
  }
  EnvContext* env = new EnvContext();
  context->Put("hexl.env", env);
  ResourceManager* rm = new DirectoryResourceManager(options.GetString("testbase", "."), options.GetString("results", "."));
  context->Put("hexl.rm", rm);
  context->Put("hexl.options", &options);
  RuntimeContext* runtime = 0;
  if (context->Opts()->GetString("rt") != "none") {
    runtime = CreateRuntimeContext(context.get());
    if (!runtime) {
      std::cout << "Failed to create runtime" << std::endl;
      exit(7);
    }
    context->Put("hexl.runtime", runtime);
  }
  context->Put("hexl.testFactory", testFactory);
  runner = CreateTestRunner();
  TestSet* tests = CreateTestSet();
  assert(tests);
  runner->RunTests(*tests);

  // cleanup in reverse order. new never fails.
  delete runner; runner = 0;
  if (runtime) { delete runtime; } 
  delete rm;  delete env;
}

}

int main(int argc, char **argv)
{
  hsail_conformance::HCRunner hcr(argc, argv);
  hcr.Run();
  return 0;
}
