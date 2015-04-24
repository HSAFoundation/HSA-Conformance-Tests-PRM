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

#ifndef HC_TESTS_HPP
#define HC_TESTS_HPP

#include "Arena.hpp"
#include "Emitter.hpp"
#include "BrigEmitter.hpp"
#include "Scenario.hpp"
#include "Grid.hpp"

namespace hsail_conformance {

class Test : public hexl::EmittedTest {
protected:
  hexl::emitter::BrigEmitter& be;
  hexl::emitter::EmittableContainer specList;

public:
  Test(hexl::emitter::Location codeLocation = hexl::emitter::KERNEL, hexl::Grid geometry = 0)
    : hexl::EmittedTest(codeLocation, geometry),
      be(*te->Brig()),
      specList(te.get()) { }

  void Init();

  void StartProgram() { hexl::EmittedTest::StartProgram(); specList.StartProgram(); }
  void EndProgram() { hexl::EmittedTest::EndProgram(); specList.EndProgram(); }

  void StartModule() { hexl::EmittedTest::StartModule(); specList.StartModule(); }
  void ModuleDirectives() { hexl::EmittedTest::ModuleDirectives(); specList.ModuleDirectives(); }
  void ModuleVariables() { hexl::EmittedTest::ModuleVariables(); specList.ModuleVariables(); }
  void EndModule() { hexl::EmittedTest::EndModule(); specList.EndModule(); }

  void StartFunction() { hexl::EmittedTest::StartFunction(); specList.StartFunction(); }
  void FunctionFormalOutputArguments() { hexl::EmittedTest::FunctionFormalOutputArguments(); specList.FunctionFormalOutputArguments(); }
  void FunctionFormalInputArguments() { hexl::EmittedTest::FunctionFormalInputArguments(); specList.FunctionFormalInputArguments(); }
  void StartFunctionBody() { hexl::EmittedTest::StartFunctionBody(); specList.StartFunctionBody(); }
  void FunctionDirectives() { hexl::EmittedTest::FunctionDirectives(); specList.FunctionDirectives(); }
  void FunctionVariables() { hexl::EmittedTest::FunctionVariables(); specList.FunctionVariables(); }
  void FunctionInit() { hexl::EmittedTest::FunctionInit(); specList.FunctionInit(); }
  void EndFunction() { hexl::EmittedTest::EndFunction(); specList.EndFunction(); }

  void ActualCallArguments(hexl::emitter::TypedRegList inputs, hexl::emitter::TypedRegList outputs) { hexl::EmittedTest::ActualCallArguments(inputs, outputs); specList.ActualCallArguments(inputs, outputs); }

  void StartKernel() { hexl::EmittedTest::StartKernel(); specList.StartKernel(); }
  void KernelArguments() { hexl::EmittedTest::KernelArguments(); specList.KernelArguments(); }
  void StartKernelBody() { hexl::EmittedTest::StartKernelBody(); specList.StartKernelBody(); }
  void KernelDirectives() { hexl::EmittedTest::KernelDirectives(); specList.KernelDirectives(); }
  void KernelVariables() { hexl::EmittedTest::KernelVariables(); specList.KernelVariables(); }
  void KernelInit() { hexl::EmittedTest::KernelInit(); specList.KernelInit(); }
  void EndKernel() { hexl::EmittedTest::EndKernel(); specList.EndKernel(); }

  void SetupDispatch(const std::string& dispatchId) { hexl::EmittedTest::SetupDispatch(dispatchId); specList.SetupDispatch(dispatchId); }

  void ScenarioInit() { hexl::EmittedTest::ScenarioInit(); specList.ScenarioInit(); }
  void ScenarioCodes() { hexl::EmittedTest::ScenarioCodes(); specList.ScenarioCodes(); }
  void ScenarioDispatch() { hexl::EmittedTest::ScenarioDispatch(); specList.ScenarioDispatch();  }
  void ScenarioValidation() { hexl::EmittedTest::ScenarioValidation(); specList.ScenarioValidation();  }
  void ScenarioEnd() { hexl::EmittedTest::ScenarioEnd(); specList.ScenarioEnd();  }

  std::string Type() const { return "hsail_conformance_brig"; }
  void Description(std::ostream& out) const { Name(out); }
  void Serialize(std::ostream& out) const { assert(false); }

  bool IsValid() const { return true; }

  //CoreConfig* CoreCfg() const { return context->Get<CoreConfig*>(CoreConfig::CONTEXT_KEY); }
};

template <typename T, typename P1>
class TestAction1 : public hexl::Action<P1> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  explicit TestAction1(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const P1& p1) { it(base, new T(p1)); }
};

template <typename T, typename P1, typename P2>
class TestAction2 : public hexl::Action<hexl::Pair<P1, P2>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction2(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, P2>& p) { it(base, new T(p.First(), p.Second())); }
};

template <typename T, typename P1, typename P2, typename P3>
class TestAction3 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, P3>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction3(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, P3>>& p) { it(base, new T(p.First(), p.Second().First(), p.Second().Second())); }
};

template <typename T, typename P1, typename P2, typename P3, typename P4>
class TestAction4 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, P4>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction4(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, P4>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second()));
  }
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
class TestAction5 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, P5>>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction5(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, P5>>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second().First(),
                   p.Second().Second().Second().Second()));
  }
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
class TestAction6 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, P6>>>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction6(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, P6>>>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second().First(),
                   p.Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second()));
  }
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
class TestAction7 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, P7>>>>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction7(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, P7>>>>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second().First(),
                   p.Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second()));
  }
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
class TestAction8 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, P8>>>>>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction8(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, P8>>>>>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second().First(),
                   p.Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second()));
  }
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
class TestAction9 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, hexl::Pair<P8, P9>>>>>>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction9(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, hexl::Pair<P8, P9>>>>>>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second().First(),
                   p.Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second()));
  }
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
class TestAction10 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, hexl::Pair<P8, hexl::Pair<P9, P10>>>>>>>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction10(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, hexl::Pair<P8, hexl::Pair<P9, P10>>>>>>>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second().First(),
                   p.Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().Second()));
  }
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11>
class TestAction11 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, hexl::Pair<P8, hexl::Pair<P9, hexl::Pair<P10, P11>>>>>>>>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction11(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, hexl::Pair<P8, hexl::Pair<P9, hexl::Pair<P10, P11>>>>>>>>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second().First(),
                   p.Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().Second().Second()));
  }
};

template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12>
class TestAction12 : public hexl::Action<hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, hexl::Pair<P8, hexl::Pair<P9, hexl::Pair<P10, hexl::Pair<P11, P12>>>>>>>>>>>> {
private:
  std::string base;
  hexl::TestSpecIterator& it;

public:
  TestAction12(const std::string& base_, hexl::TestSpecIterator& it_) : base(base_), it(it_) { }

  void operator()(const hexl::Pair<P1, hexl::Pair<P2, hexl::Pair<P3, hexl::Pair<P4, hexl::Pair<P5, hexl::Pair<P6, hexl::Pair<P7, hexl::Pair<P8, hexl::Pair<P9, hexl::Pair<P10, hexl::Pair<P11, P12>>>>>>>>>>>& p) {
    it(base, new T(p.First(),
                   p.Second().First(),
                   p.Second().Second().First(),
                   p.Second().Second().Second().First(),
                   p.Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().Second().Second().First(),
                   p.Second().Second().Second().Second().Second().Second().Second().Second().Second().Second().Second()));
  }
};

template <typename Test, typename P1>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s)
{
  TestAction1<Test, P1> a(base, it);
  p1s->Iterate(a);
}

template <typename Test, typename P1, typename P2>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s)
{
  TestAction2<Test, P1, P2> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s)
{
  TestAction3<Test, P1, P2, P3> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s)
{
  TestAction4<Test, P1, P2, P3, P4> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4, typename P5>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s, hexl::Sequence<P5>* p5s)
{
  TestAction5<Test, P1, P2, P3, P4, P5> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s, p5s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s, hexl::Sequence<P5>* p5s, hexl::Sequence<P6>* p6s)
{
  TestAction6<Test, P1, P2, P3, P4, P5, P6> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s, p5s, p6s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s, hexl::Sequence<P5>* p5s, hexl::Sequence<P6>* p6s, hexl::Sequence<P7>* p7s)
{
  TestAction7<Test, P1, P2, P3, P4, P5, P6, P7> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s, p5s, p6s, p7s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s, hexl::Sequence<P5>* p5s, hexl::Sequence<P6>* p6s, hexl::Sequence<P7>* p7s, hexl::Sequence<P8>* p8s)
{
  TestAction8<Test, P1, P2, P3, P4, P5, P6, P7, P8> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s, hexl::Sequence<P5>* p5s, hexl::Sequence<P6>* p6s, hexl::Sequence<P7>* p7s, hexl::Sequence<P8>* p8s, hexl::Sequence<P9>* p9s)
{
  TestAction9<Test, P1, P2, P3, P4, P5, P6, P7, P8, P9> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s, p9s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s, hexl::Sequence<P5>* p5s, hexl::Sequence<P6>* p6s, hexl::Sequence<P7>* p7s, hexl::Sequence<P8>* p8s, hexl::Sequence<P9>* p9s, hexl::Sequence<P10>* p10s)
{
  TestAction10<Test, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s, p9s, p10s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s, hexl::Sequence<P5>* p5s, hexl::Sequence<P6>* p6s, hexl::Sequence<P7>* p7s, hexl::Sequence<P8>* p8s, hexl::Sequence<P9>* p9s, hexl::Sequence<P10>* p10s, hexl::Sequence<P11>* p11s)
{
  TestAction11<Test, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s, p9s, p10s, p11s);
  ps->Iterate(a);
}

template <typename Test, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10, typename P11, typename P12>
void TestForEach(hexl::Arena* ap, hexl::TestSpecIterator& it, const std::string& base, hexl::Sequence<P1>* p1s, hexl::Sequence<P2>* p2s, hexl::Sequence<P3>* p3s, hexl::Sequence<P4>* p4s, hexl::Sequence<P5>* p5s, hexl::Sequence<P6>* p6s, hexl::Sequence<P7>* p7s, hexl::Sequence<P8>* p8s, hexl::Sequence<P9>* p9s, hexl::Sequence<P10>* p10s, hexl::Sequence<P11>* p11s, hexl::Sequence<P12>* p12s)
{
  TestAction12<Test, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12> a(base, it);
  auto ps = SequenceProduct(ap, p1s, p2s, p3s, p4s, p5s, p6s, p7s, p8s, p9s, p10s, p11s, p12s);
  ps->Iterate(a);
}

}

#endif // HC_TESTS_HPP
