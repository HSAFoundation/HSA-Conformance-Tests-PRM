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

#include "HCTests.hpp"
#include "BasicHexlTests.hpp"
#include "Scenario.hpp"
#include "CoreConfig.hpp"

using namespace HSAIL_ASM;
using namespace hexl;
using namespace Brig;

namespace hsail_conformance {

  void Test::Init() {
    hexl::EmittedTest::Init();
    specList.Reset(te.get());
    specList.Init();
  }

}