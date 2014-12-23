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

#include "HexlTestList.hpp"
#include "HexlResource.hpp"
#include "HexlTestFactory.hpp"
#include <set>
#include <cassert>

namespace hexl {

class SimpleTestSpec : public TestSpec {
public:
  SimpleTestSpec(const std::string& name_, SimpleTestList* testList_) : name(name_), testList(testList_) { }

  std::string Type() const { return "simple_test_spec"; }
  void Name(std::ostream& out) const { out << name; }
  void Description(std::ostream& out) const { out << name; }
  void InitContext(Context* context) { }
  Context* GetContext() { return 0; }
  void Serialize(std::ostream& out) const { assert(false); }
  Test* Create()
  {
    assert (testList && testList->testFactory);
    return testList->testFactory->CreateTest(testList->testType, name);
  }

  virtual bool IsValid() const { return true; }
private:
  std::string name;
  SimpleTestList* testList;
};


#define TEMP_DEBUG_KEYS_(text) //text

/// Parses a line from testlist and returns results.
/// Result could be testname with optional keywords OR
/// nestedTestlist OR nothing.
///
/// Line format:
///   " "*{testname[" "+keyword[","keyword]*]]|"@"nestedTestlist}" "*[#comment]<EOL>
///   One or more whitespaces or commas work as a separator between keywords.
///   Whitespaces and commas are not allowed in basename and keywords.
///
/// \param:out  testname        Name of specified test or empty,
///                             if line does not specify any test.
/// \param:out  keywords        Set of keywords associated with test.
///                             Undefined if testname is empty.
/// \param:out  nestedTestlist  Name of nested testlist or empty,
///                             if line does not specify nested testlist.
/// \return     true = OK, false = failed
///
bool SimpleTestList::Parse(
  std::string line, 
  const unsigned lineNumber, // for diagnostics
  const std::string& testlist, // for diagnostics
  std::string * const testname, 
  std::set<std::string> * const keywords,
  std::string * const nestedTestlist) const
{
  assert(testname && keywords && nestedTestlist);
  testname->clear();
  keywords->clear();
  nestedTestlist->clear();
  TEMP_DEBUG_KEYS_( std::string origLine = line; )
  // throw away everything which begins with [" "]*#
  std::size_t e = line.find("#");
  std::size_t s;
  if ( e != std::string::npos ) {
    if (e > 0) {
      s = line.find_last_not_of(" \t", e-1, line.length()-e);
      s = (s == std::string::npos) ? e : s+1;
    } else {
      s = e;
    }
    line.replace(s, line.length() - s, "");
  }
  // find testname or nestedTestlist
  s = line.find_first_not_of(" \t");
  if (s == std::string::npos) {
    TEMP_DEBUG_KEYS_( std::cout << "   SKIPPED line " << lineNumber << ": \""<< origLine << "\"" << std::endl; ) 
    return true;
  } // nothing
  e = line.find_first_of(" \t", s);
  std::string token = std::string(line, s, (e == std::string::npos) ? e : e-s);
  if (token.find(",") != std::string::npos) { // comma may get into testname in wrong lines
    std::cout << "Error: Misplaced comma. Line " << lineNumber << " in testlist '" << testlist <<"': \""<< line << "\"" << std::endl; /// \todo
    return false; 
  }

  std::string keywords_;
  if (token[0] == '@') {
    *nestedTestlist = std::string(token,1);
    // check that no extra characters in line
    s = line.find_first_not_of(" \t", e);
    if (s != std::string::npos) {
      std::cout << "Error: Extra characters after testlist name: '" << std::string(line, s) << "'. Line " << lineNumber << " in testlist " << testlist <<": \""<< line << "\"" << std::endl; /// \todo
      return false;
    }
  } else {
    *testname = token;
    // find keywords_ and check the rest
    s = line.find_first_not_of(" \t", e);
    if (s != std::string::npos) {
      e = line.find_first_of(" \t", s);
      keywords_ = std::string(line, s, (e == std::string::npos) ? e : e-s);
      s = line.find_first_not_of(" \t", e);
      if (s != std::string::npos) {
        std::cout << "Error: Extra characters after keywords: '" << std::string(line, s) << "'. Line " << lineNumber << " in testlist " << testlist <<": \""<< line << "\"" << std::endl; /// \todo
        return false;
      }
    }
  }
  // parse keywords
  for (e = 0, s = 0; !keywords_.empty() && e != std::string::npos; s = e+1) {
    e = keywords_.find(",", s);
    std::string k = std::string(keywords_, s, (e == std::string::npos) ? e : e-s);
    if (k.empty()) {
      std::cout << "Error: Empty keyword. Line " << lineNumber << " in testlist '" << testlist <<"': \""<< line << "\"" << std::endl; /// \todo
      return false;
    }
    keywords->insert(k);
  }
  TEMP_DEBUG_KEYS_(
  std::cout << "   NESTED TL: " << *nestedTestlist << std::endl;
  std::cout << "   TEST NAME: " << *testname << std::endl;
  std::cout << "   KEYWORDS : ";
  for (std::set<std::string>::const_iterator p = keywords->begin(); p != keywords->end(); ++p) {
    std::cout << "[" << *p << "] ";
  }
  std::cout << std::endl; )
  return true;
}

bool SimpleTestList::IsMatchKey(const std::set<std::string>& keywords) const
{
  if (!key.empty()) {
    if (key[0] == '!') { // there should be NONE matching keywords
      if (keywords.count(std::string(key,1)) != 0) { return false; }
    } else { // there should be at least ONE matching keyword
      if (keywords.count(key) == 0) { return false; }
    }
  }
  return true;
}

/// Fails if testlist (or a nested testlist, if any) is not found.
/// Bad syntax of testlists cause error messages.
bool SimpleTestList::ReadFrom(ResourceManager* rm, const std::string& testlist)
{
  readFromNestingDepth = 0;
  return ReadFromImpl(rm, testlist);
}

bool SimpleTestList::ReadFromImpl(ResourceManager* rm, const std::string& testlist)
{
  assert(rm);
  ++readFromNestingDepth;
  bool rc = true;
  std::istream* in = rm->Get(testlist); // new
  if (!in) {
    std::cout << "Error: Unable to open testlist '" << testlist <<"'" << std::endl; /// \todo
    rc = false;
  } else {
    std::string line;
    for (unsigned lineNumber = 1; getline(*in, line); ++lineNumber) {
      std::string testname;
      std::string nestedTestlist;
      std::set<std::string> keywords;
      if (!Parse(line, lineNumber, testlist, &testname, &keywords, &nestedTestlist)) {
        continue; // bad line, skip
      }
      if (!nestedTestlist.empty()) {
        if (readFromNestingDepth > 100) {
          std::cout << "Error: Testlist nesting depth > 100. Line " << lineNumber << " in testlist '" << testlist <<"': \""<< line << "\"" << std::endl; /// \todo
          rc = false;
          break;
        }
        if (! (rc = ReadFromImpl(rm, nestedTestlist))) {
          std::cout << "Info: See line " << lineNumber << " in testlist '" << testlist <<"': \""<< line << "\"" << std::endl; /// \todo
          break;
        }
      } else if (!testname.empty()) {
        if (IsMatchKey(keywords)) {
          testNames.push_back(testname);
        }
      } else {
        continue;  // nothing in line, skip
      }
    }
    delete in;
  }
  --readFromNestingDepth;
  return rc;
}

void SimpleTestList::Iterate(TestSpecIterator& it)
{
  for (std::vector<std::string>::const_iterator i = testNames.begin(), e = testNames.end(); i != e; ++i) {
    it("", new SimpleTestSpec(*i, this));
  }
}

}
