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

#ifndef HEXL_TEST_HPP
#define HEXL_TEST_HPP

#include "MObject.hpp"
#include "Options.hpp"
#include "Stats.hpp"

#include <string>
#include <vector>
#include <stdint.h>
#include <cstdarg>
#include <cassert>
#include <memory>
#include <vector>

namespace hexl {

class Context;

enum TestStatus {
  PASSED = 0,
  FAILED,
  ERROR,
  NA,
};

const char *TestStatusString(TestStatus status);

class TestResult {
private:
  TestStatus status;
  std::string output;

public:
  TestResult()
    : status(PASSED) { }
  TestResult(TestStatus status_, const std::string& output_ = "")
    : status(status_), output(output_) { }
  TestStatus Status() const { return status; }
  const char *StatusString() const { return TestStatusString(status); }
  void SetStatus(TestStatus status) { this->status = status; }
  void SetFailed() { status = FAILED; }
  bool IsFailed() const { return status == FAILED; }
  void SetError() { status = ERROR; }
  bool IsError() const { return status == ERROR; }
  bool IsPassed() const { return status == PASSED; }
  void IncStats(AllStats& allStats) const;
  std::string Output() const { return output; }
  void SetOutput(const std::string& output) { this->output = output; }
  void Serialize(std::ostream& out) const;
  void Deserialize(std::istream& in);
};

ENUM_SERIALIZER(TestStatus);
MEMBER_SERIALIZER(TestResult);

class Test {
public:
  virtual ~Test() { }
  virtual std::string Type() const = 0;
  virtual void Name(std::ostream& out) const = 0;
  std::string TestName() const;
  virtual void Description(std::ostream& out) const = 0;
  virtual void InitContext(Context* context) = 0;
  virtual Context* GetContext() = 0;
  virtual void Serialize(std::ostream& out) const = 0;
  virtual void Run() = 0;
  virtual TestResult Result() const = 0;
  virtual void DumpIfEnabled() { };
};

class TestImpl : public Test {
private:
  TestResult result;

protected:
  std::unique_ptr<Context> context;

  void SetFailed() { result.SetFailed(); }
  void SetError() { result.SetError(); }
  void Fail(const char *format, ...);
  void Error(const char *format, ...);
  virtual void SerializeData(std::ostream& out) const { assert(false); }
  std::string GetOutputName(const std::string& what);
  bool DumpTextIfEnabled(const std::string& what, const std::string& text);
  bool DumpBinaryIfEnabled(const std::string& what, const void *buffer, size_t bufferSize);

public:
  TestImpl() : context(new Context()) { }
  TestImpl(Context* context_) : context(context_) { }
  void InitContext(Context* context) { this->context->SetParent(context); }
  Context* GetContext() { return context.get(); }
  //virtual std::string Type() const = 0;
  virtual void Name(std::ostream& out) const = 0;
  //virtual void Description(std::ostream& out) = 0;
  //virtual void SetContext(Context* context) = 0;
  virtual void Serialize(std::ostream& out) const;
  //virtual void Run() = 0;
  virtual TestResult Result() const { return result; }

  virtual void PrintExtraInfo(std::ostream &s) const { } // no extra info by default
};

class TestSpec : public Test {
private:
  TestResult result;

public:
  virtual void Name(std::ostream& out) const = 0;
  virtual Test* Create() = 0;
  virtual bool IsValid() const = 0;
  virtual void Run();
  virtual TestResult Result() const { return result; }
};

class TestHolder : public TestSpec {
private:
  Test* test;

public:
  TestHolder(Test* test_) : test(test_) { }
  virtual std::string Type() const { return test->Type(); }
  virtual void Name(std::ostream& out) const { test->Name(out); }
  virtual void Description(std::ostream& out) const { test->Description(out); }
  virtual void InitContext(Context* context) { test->InitContext(context); }
  virtual Context* GetContext() { return test->GetContext(); }
  virtual void Serialize(std::ostream& out) const { test->Serialize(out); }
  virtual Test* Create() { return test; }
  virtual bool IsValid() const { return true; }
};

class TestSpecIterator {
public:
  virtual void operator()(const std::string& path, TestSpec* spec) = 0;
};

class TestSpecList : public TestSpecIterator {
public:
  ~TestSpecList();

  size_t Count() const { return specs.size(); }

  const std::string& GetPath(size_t i) const { return paths[i]; }
  const TestSpec* GetSpec(size_t i) const { return specs[i]; }
  TestSpec* GetSpec(size_t i) { return specs[i]; }
  void Add(const std::string& path, TestSpec* spec) { paths.push_back(path); specs.push_back(spec); }
  void operator()(const std::string& path, TestSpec* spec) { if (spec->IsValid()) { Add(path, spec); } else { delete spec; } }

private:
  std::vector<std::string> paths;
  std::vector<TestSpec*> specs;
};

class TestFilter;
class TestNameFilter;
class ExcludeListFilter;

class TestSet {
public:
  virtual ~TestSet() { }
  virtual void InitContext(Context* context) = 0;
  virtual void Name(std::ostream& out) const = 0;
  virtual void Description(std::ostream& out) const = 0;
  virtual void Iterate(TestSpecIterator& it) = 0;
  virtual TestSet* Filter(TestNameFilter* filter) = 0;
  virtual TestSet* Filter(ExcludeListFilter* filter) = 0;
};

class EmptyTestSet : public TestSet {
public:
  virtual void InitContext(Context* context) { }
  virtual void Name(std::ostream& out) const { out << "<empty>"; }
  virtual void Description(std::ostream& out) const { out << "<empty>"; }
  virtual void Iterate(TestSpecIterator& it) { }
  virtual TestSet* Filter(TestNameFilter* filter) { return new EmptyTestSet(); }
  virtual TestSet* Filter(ExcludeListFilter* filter) { return new EmptyTestSet(); }
};

class BasicTestSet : public TestSet {
private:
  std::string name;

protected:
  Context* context;

public:
  BasicTestSet(const std::string& name_)
    : name(name_), context(0) { }
  virtual void InitContext(Context* context) { this->context = context; }
  std::string Path() const { return name; }
  virtual void Name(std::ostream& out) const { out << name; }
  virtual void Description(std::ostream& out) const { out << name; }
  virtual TestSet* Filter(TestNameFilter* filter);
  virtual TestSet* Filter(ExcludeListFilter* filter);
};

#define DECLARE_TESTSET(TS, NAME) \
class TS : public hexl::BasicTestSet { \
public: \
  TS() : BasicTestSet(NAME) { } \
  virtual void Iterate(hexl::TestSpecIterator& it); \
}

class FilteredTestSet : public TestSet {
private:
  TestSet* parent;
  TestFilter* filter;

public:
  FilteredTestSet(TestSet* parent_, TestFilter* filter_)
    : parent(parent_), filter(filter_) { }
  virtual void InitContext(Context* context) { if (parent) { parent->InitContext(context); } }
  virtual void Name(std::ostream& out) const { parent->Name(out); }
  virtual void Description(std::ostream& out) const { parent->Description(out); }
  virtual void Iterate(TestSpecIterator& it);
  virtual TestSet* Filter(TestNameFilter* filter);
  virtual TestSet* Filter(ExcludeListFilter* filter);
};

class OneTest : public TestSet {
public:
  OneTest(Test* test_) : test(test_) { assert(test); }
  virtual void InitContext(Context* context) { }
  virtual void Iterate(TestSpecIterator& it) { it("", new TestHolder(test)); }
  virtual void Name(std::ostream& out) const { test->Name(out); }
  virtual void Description(std::ostream& out) const { test->Description(out); }
  virtual void Name(TestSpec* test, std::ostream& out) const { test->Name(out); }
//  virtual TestSet* Filter(TestFilter* filter) { return filter->Filter(this); }
  virtual TestSet* Filter(TestNameFilter* filter);
  virtual TestSet* Filter(ExcludeListFilter* filter);

private:
  Test* test;
};

class TestSetUnion : public TestSet {
private:
  std::string base;
  std::vector<TestSet*> testSets;

  bool Cut(const std::string& name, std::string& rest) const;

protected:
  Context* context;

public:
  TestSetUnion(const std::string& base_, Context* context_ = 0)
    : base(base_), context(context_) { }
  void InitContext(Context* context);
  void Add(TestSet* testSet) { testSets.push_back(testSet); }
  void Name(std::ostream& out) const { out << base; }
  void Description(std::ostream& out) const { out << base; }
  virtual void Iterate(TestSpecIterator& it);
//  virtual TestSet* Filter(TestFilter* filter) { return filter->Filter(this); }
  virtual TestSet* Filter(TestNameFilter* filter);
  virtual TestSet* Filter(ExcludeListFilter* filter);
};

#define DECLARE_TESTSET_UNION(TS) \
class TS : public hexl::TestSetUnion { \
public: \
  TS(); \
}

class TestFilter {
public:
  virtual TestSet* Filter(TestSet* ts) = 0;
  virtual bool Matches(const std::string& path, Test* test) = 0;
};

class TestNameFilter : public TestFilter {
private:
  std::string namePattern;

public:
  TestNameFilter(const std::string& namePattern_)
    : namePattern(namePattern_) { }
  virtual TestSet* Filter(TestSet* ts) { return ts->Filter(this); }
  bool Matches(const std::string& path, Test* test);
  bool Matches(const std::string& name);
  const std::string& NamePattern() const { return namePattern; }
};

class ExcludeListFilter : public TestFilter {
private:
  std::vector<std::string> excludePrefixes;

public:
  virtual TestSet* Filter(TestSet* ts) { return ts->Filter(this); }
  bool Matches(const std::string& path, Test* test);
  bool Matches(const std::string& name);
  const std::vector<std::string>& ExcludePrefixes() const { return excludePrefixes; }
  void AddPrefix(const std::string& prefix) { excludePrefixes.push_back(prefix); }
  bool Load(ResourceManager* rm, const std::string& name);
};

class AndFilter : public TestFilter {
private:
  TestFilter* filter1;
  TestFilter *filter2;

public:
  AndFilter(TestFilter* filter1_, TestFilter* filter2_)
    : filter1(filter1_), filter2(filter2_) { }
  virtual TestSet* Filter(TestSet* ts) { return filter2->Filter(filter1->Filter(ts)); }
  bool Matches(const std::string& path, Test* test) { return filter1->Matches(path, test) && filter2->Matches(path, test); }
};

class AssemblyStats;

class AllStats;

class ResourceManager;
class TestFactory;

class EnvContext {
public:
  void Error(const char *format, ...);
  void vError(const char *format, va_list ap);
  AllStats& Stats() { return stats; }
  const AllStats& Stats() const { return stats; }
private:
  AllStats stats;
};


};

#endif // HEXL_TEST_HPP
