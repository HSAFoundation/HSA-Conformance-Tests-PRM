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

#include "ImageCoreTests.hpp"
#include "ImageRdTests.hpp"
#ifdef ENABLE_HEXL_HSAILTESTGEN
#include "HexlTestGen.hpp"
#endif // ENABLE_HEXL_HSAILTESTGEN
#include "CoreConfig.hpp"

using namespace Brig;
using namespace hexl;
#ifdef ENABLE_HEXL_HSAILTESTGEN
using namespace hexl::TestGen;
#endif // ENABLE_HEXL_HSAILTESTGEN

namespace hsail_conformance { 

DECLARE_TESTSET_UNION(ImagesOperationsTests);

ImagesOperationsTests::ImagesOperationsTests()
  : TestSetUnion("images")
{
  Add(new ImageRdTestSet());
}

hexl::TestSet* NewImagesCoreTests()
{
  return new ImagesOperationsTests();
}

}
