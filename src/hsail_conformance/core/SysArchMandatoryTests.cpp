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

#include "SysArchMandatoryTests.hpp"
#include "MModelTests.hpp"
#include "AtomicTests.hpp"

using namespace hexl;

namespace hsail_conformance {

DECLARE_TESTSET_UNION(MemoryConsistencyModelTests);

MemoryConsistencyModelTests::MemoryConsistencyModelTests()
  : TestSetUnion("consistency")
{
  Add(new AtomicTests());
  Add(new MModelTests());
}

DECLARE_TESTSET_UNION(SysArchMandatoryTests);

SysArchMandatoryTests::SysArchMandatoryTests()
  : TestSetUnion("mandatory")
{
  Add(new MemoryConsistencyModelTests());
}

hexl::TestSet* NewSysArchMandatoryTests()
{
  return new SysArchMandatoryTests();
}

}
