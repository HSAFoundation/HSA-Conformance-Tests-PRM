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

#ifndef HEXL_OPTIONS_HPP
#define HEXL_OPTIONS_HPP

#include <string>
#include <map>
#include <vector>

namespace hexl {

enum OptionKind {
  OPT_STRING,
  OPT_BOOLEAN,
  OPT_STRING_SET,
};

struct OptionDefinition {
  OptionKind kind;
  std::string name;
  std::string defaultValue;
};

class OptionRegistry {
public:
  void RegisterOption(const OptionDefinition& optDef) { definitions[optDef.name] = optDef; } /// \todo not used
  void RegisterOption(const std::string& name, const std::string& defaultValue = "");
  void RegisterMultiOption(const std::string& name); // may appear mutliple times in the command line
  void RegisterBooleanOption(const std::string& name);
  const OptionDefinition* GetOption(const std::string& name) const;
private:
  typedef std::map<std::string, OptionDefinition> Map;

  Map definitions;

  bool IsRegistered(const std::string& name) const;
};

class Options {
public:
  typedef std::vector<std::string> MultiString; // use vector because we want testing order 
                                                // to be the same as order of "testlist"
                                                // options in the command line
  bool IsSet(const std::string& name) const;
  std::string GetString(const std::string& name, const std::string& defaultValue = "") const;
  void SetString(const std::string& name, const std::string& value);
  const MultiString* GetMultiString(const std::string& name) const;
  void SetMultiString(const std::string& name, const std::string& value);
  bool GetBoolean(const std::string& name) const;
  void SetBoolean(const std::string& name, bool value = true);
  unsigned GetUnsigned(const std::string& name, unsigned defaultValue) const;
private:
  typedef std::map<std::string, MultiString> Map;

  Map values;
};

int ParseOptions(int argc, char **argv, const OptionRegistry& optDefs, Options& opts);

}

#endif // HEXL_OPTIONS_HPP
