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

#include "UtilTests.hpp"
#include "BrigEmitter.hpp"

using namespace hexl;
using namespace hexl::emitter;
using namespace Brig;
using namespace HSAIL_ASM;
using namespace hsail_conformance::utils;

namespace hsail_conformance {

void BoundaryTest::ExpectedResults(hexl::Values* result) const {
  for (uint64_t i = 0; i < numBoundaryValues; ++i) {
    result->push_back(ExpectedResult(i));
  }
}

void BoundaryTest::KernelCode() {
  TypedReg result64 = be.WorkitemFlatAbsId(true);
  //Store condition
  TypedReg reg_c = be.AddTReg(BRIG_TYPE_B1);
  //cmp_ge c0, s0, n
  Operand boundary = be.Immed(BRIG_TYPE_U64, Boundary());
  be.EmitCmp(reg_c->Reg(), result64, boundary, BRIG_COMPARE_GE);
  SRef then = "@then";
  //cbr c0, @then
  be.EmitCbr(reg_c, then);
  SRef endif = "@endif";
  //br @endif
  be.EmitBr(endif);
    //@then:
  be.EmitLabel(then);
  TypedReg index = be.AddTReg(BRIG_TYPE_U64);
  //sub s1, s0, n
  be.EmitArith(BRIG_OPCODE_SUB, index, result64, boundary);  
  //insert workitemXXXid instruction
  TypedReg result = KernelResult();
  //store result
  output->EmitStoreData(result, index);
  //@endif:
  be.EmitLabel(endif);
}


void SkipTest::Init() {
  Test::Init();
  outputVar = kernel->NewVariable("output_var", BRIG_SEGMENT_GLOBAL, ResultType(), Location::MODULE);
}

void SkipTest::KernelCode() {
  auto result = KernelResult();
  assert(result);
  outputVar->EmitStoreFrom(result);
}

TypedReg SkipTest::Result() {
  auto result = be.AddTReg(RESULT_TYPE);
  be.EmitMov(result, be.Immed(result->Type(), RESULT_VALUE));
  return result;
}

}
