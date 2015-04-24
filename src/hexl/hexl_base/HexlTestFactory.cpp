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

#include "HexlTestFactory.hpp"
#include "BasicHexlTests.hpp"

namespace hexl {

Test* DefaultTestFactory::CreateTest(const std::string& type, const std::string& name, const Options& options)
{
  /*
  if (type == "finalize") {
    std::string hsail = options.GetString("hsail", name + ".hsail");
    return new FinalizeHsailResourceTest(hsail);
  } else */ {
    return 0;
  }
}

Test* DefaultTestFactory::CreateTestDeserialize(const std::string& type, std::istream& in)
{
  /*
  if (type == "finalize") {
    return new FinalizeHsailResourceTest(in);
  } else if (type == "validate_brig_container") {
    return new ValidateBrigContainerTest(in);
  } else */ {
    return 0;
  }
}

Test* TestFactory::CreateTest(std::istream& in)
{
  std::string type;
  ReadData(in, type);
  return CreateTestDeserialize(type, in);
}

void TestFactory::Serialize(std::ostream& out, const Test* test)
{
  test->Serialize(out);
}

/*
void TestFactory::Deserialize(std::istream& in, Test*& test)
{
  std::string type;
  ReadData(in, type);
  test = CreateTestDeserialize(type, in);
}
*/

}
