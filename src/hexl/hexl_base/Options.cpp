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

#include "Options.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

namespace hexl {

bool OptionRegistry::IsRegistered(const std::string& name) const
{
  Map::const_iterator i = definitions.find(name);
  return i != definitions.end();
}

const OptionDefinition* OptionRegistry::GetOption(const std::string& name) const
{
  Map::const_iterator i = definitions.find(name);
  return (i != definitions.end()) ? &i->second : 0;
}

void OptionRegistry::RegisterOption(const std::string& name, const std::string& defaultValue)
{
  assert(!IsRegistered(name));
  OptionDefinition optDef;
  optDef.kind = OPT_STRING;
  optDef.name = name;
  optDef.defaultValue = defaultValue;
  definitions[name] = optDef;
}

void OptionRegistry::RegisterBooleanOption(const std::string& name)
{
  assert(!IsRegistered(name));
  OptionDefinition optDef;
  optDef.kind = OPT_BOOLEAN;
  optDef.name = name;
  definitions[name] = optDef;
}

void OptionRegistry::RegisterMultiOption(const std::string& name)
{
  assert(!IsRegistered(name));
  OptionDefinition optDef;
  optDef.kind = OPT_STRING_SET;
  optDef.name = name;
  definitions[name] = optDef;
}

bool Options::IsSet(const std::string& name) const
{
  return values.find(name) != values.end();
}

std::string Options::GetString(const std::string& name, const std::string& defaultValue) const
{
  Map::const_iterator i = values.find(name);
  assert(i == values.end() || i->second.size() == 1);
  return (i != values.end()) ? i->second[0] : defaultValue;
}

void Options::SetString(const std::string& name, const std::string& value)
{
#ifndef NDEBUG
  Map::const_iterator i = values.find(name);
  assert(i == values.end() || i->second.size() == 0);
#endif
  values[name].push_back(value);
}

const Options::MultiString* Options::GetMultiString(const std::string& name) const
{
  Map::const_iterator i = values.find(name);
  return (i != values.end()) ? &(i->second) : NULL;
}

void Options::SetMultiString(const std::string& name, const std::string& value)
{
  values[name].push_back(value);
}

bool Options::GetBoolean(const std::string& name) const
{
  return !GetString(name).empty();
}

void Options::SetBoolean(const std::string& name, bool value)
{
  SetString(name, value ? "1" : "");
}

unsigned Options::GetUnsigned(const std::string& name, unsigned defaultValue) const
{
  std::string s = GetString(name);
  if (s.empty()) { return defaultValue; }
  unsigned result;
  std::stringstream(s) >> result;
  return result;
}

int ParseOptions(const std::vector<std::string>& args, const OptionRegistry& registry, Options& opts) {
  unsigned i = 1;
  while (i < args.size()) {
    std::string arg = args[i++];
    if (arg[0] == '-') {
      arg = arg.substr(1);
      if (!arg.compare(0, 2, "o:")) {
        // -o:name=value
        size_t pos = arg.find("=");
        if (pos != std::string::npos) {
          std::string name = arg.substr(2, pos - 2);
          std::string value = arg.substr(pos + 1);
          opts.SetString(name, value);
          continue;
        }
      } else if (const OptionDefinition* optDef = registry.GetOption(arg)) {
        switch (optDef->kind) {
        case OPT_STRING:
          if (i < args.size()) {
            opts.SetString(arg, args[i++]);
            continue;
          }
          break;
        case OPT_STRING_SET:
          if (i < args.size()) {
            opts.SetMultiString(arg, args[i++]);
            continue;
          }
          break;
        case OPT_BOOLEAN:
          opts.SetBoolean(arg);
          continue;
        default:
          assert(false);
          break;
        }
      }
    }
    // Invalid option
    return i;
  }
  return 0;
}

int ParseOptions(int argc, char **argv, const OptionRegistry& registry, Options& opts) {
  std::vector<std::string> args;
  for (int  i = 0; i < argc; ++i) {
    args.push_back(argv[i]);
  }
  if (const char *o = getenv("HEXL_OPTS")) {
    std::string opts(o);
    if (!opts.empty()) {
      std::cout << "Using HEXL_OPTS environment variable: '" << opts << "'" << std::endl;
      std::stringstream ss(opts);
      std::string arg;
      while (ss >> arg) {
        if (!arg.empty()) { args.push_back(arg); }
      }
    }
  }
  return ParseOptions(args, registry, opts);
}

}
