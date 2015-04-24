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

#ifndef STATS_HPP
#define STATS_HPP

#include <iostream>
#include <iomanip>

namespace hexl {

class TestSetStats {
public:
  TestSetStats() : passed(0), failed(0), error(0), na(0) { }

  unsigned Passed() const { return passed; }
  unsigned Failed() const { return failed; }
  unsigned Error() const { return error; }
  unsigned Na() const { return na; }
  unsigned Total() const { return passed + failed + error + na; }

  bool AllPassed() const { return passed == Total(); }

  void IncPassed() { passed++; }
  void IncFailed() { failed++; }
  void IncError() { error++; }
  void IncNa() { na++; }
  void Clear() { passed = 0; failed = 0; error = 0; na = 0; }

  void Append(const TestSetStats& other) {
    passed += other.passed;
    failed += other.failed;
    error += other.error;
    na += other.na;
  }

  void Print(std::ostream& out) const {
    out << "Passed: " << std::setw(6) << passed << std::endl;
    out << "Failed: " << std::setw(6) << failed << std::endl;
    out << "Error:  " << std::setw(6) << error << std::endl;
    out << "NA:     " << std::setw(6) << na << std::endl;
    out << "Total:  " << std::setw(6) << Total() << std::endl;
  }

  void PrintShort(std::ostream& out) const {
    out << "Passed: " << std::setw(6) << passed << " "
        << "  Failed: " << std::setw(6) << failed << " "
        << "  Error: " << std::setw(6) << error << " "
        << "  NA: " << std::setw(6) << na << " "
        << "  Total: " << std::setw(6) << Total();
  }

private:
  unsigned passed;
  unsigned failed;
  unsigned error;
  unsigned na;
};

class AssemblyStats {
public:
  AssemblyStats() : strings(0), directives(0), instructions(0), operands(0) { }

  unsigned Strings() const { return strings; }
  unsigned Directives() const { return directives; }
  unsigned Instructions() const { return instructions; }
  unsigned Operands() const { return operands; }

  void IncStrings(unsigned count = 1) { strings += count; }
  void IncDirectives(unsigned count = 1) { directives += count; }
  void IncInstructions(unsigned count = 1) { instructions += count; }
  void IncOperands(unsigned count = 1) { operands += count; }
  void Clear() { strings = 0; directives = 0; instructions = 0; operands = 0; }

  void Append(const AssemblyStats& other) {
    strings += other.strings;
    directives += other.directives;
    instructions += other.instructions;
    operands += other.operands;
  }

  void PrintTestInfo(std::ostream& out) const {
    out << "BRIG instructions: " << instructions << std::endl;
  }
private:
  unsigned strings;
  unsigned directives;
  unsigned instructions;
  unsigned operands;
};

class AllStats {
public:
  void Print(std::ostream& out) const { }
  TestSetStats& TestSet() { return testSetStats; }
  const TestSetStats& TestSet() const { return testSetStats; }
  AssemblyStats &Assembly() { return assemblyStats; }
  const AssemblyStats &Assembly() const { return assemblyStats; }

  void Clear() { testSetStats.Clear(); assemblyStats.Clear(); }
  void Append(const AllStats& other) { testSetStats.Append(other.testSetStats); assemblyStats.Append(other.assemblyStats); }
  void PrintTest(std::ostream& out) const { assemblyStats.PrintTestInfo(out); }
  void PrintTestSet(std::ostream& out) const { testSetStats.Print(out); assemblyStats.PrintTestInfo(out); }

private:
  TestSetStats testSetStats;
  AssemblyStats assemblyStats;
};

}

#endif // STATS_HPP
