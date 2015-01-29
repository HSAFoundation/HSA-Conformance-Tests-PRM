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

#ifndef HC_COMMON_UTIL_TESTS_HPP
#define HC_COMMON_UTIL_TESTS_HPP

#include "HCTests.hpp"

namespace hsail_conformance {

namespace utils {

class BoundaryTest : public Test {
private:
  uint64_t numBoundaryValues;

public:
  explicit BoundaryTest(
    uint64_t numBoundaryValues_, 
    hexl::emitter::Location codeLocation_ = hexl::emitter::Location::KERNEL, 
    hexl::Grid geometry_ = 0
  ): Test(codeLocation_, geometry_), numBoundaryValues(numBoundaryValues_) { }

  const uint64_t Boundary() const {
    return geometry->GridSize() - numBoundaryValues;
  }

  uint64_t NumBoundaryValues() const { return numBoundaryValues; }

  size_t OutputBufferSize() const override {
    return numBoundaryValues;
  }

  void ExpectedResults(hexl::Values* result) const override;
  void KernelCode() override;
};


class SkipTest: public BoundaryTest {
private:
  static const int RESULT_VALUE = 1;
  static const Brig::BrigTypeX RESULT_TYPE = Brig::BrigTypeX::BRIG_TYPE_U32;

public:
  explicit SkipTest(hexl::emitter::Location codeLocation_ = hexl::emitter::Location::KERNEL, hexl::Grid geometry_ = 0)
    : BoundaryTest(1, codeLocation_, geometry_) {}

  Brig::BrigTypeX ResultType() const override { return RESULT_TYPE; }
  hexl::Value ExpectedResult() const override { return hexl::Value(hexl::Brig2ValueType(RESULT_TYPE), RESULT_VALUE); }

  hexl::emitter::TypedReg Result() override;
};

}
}
#endif // HC_COMMON_UTIL_TESTS_HPP
