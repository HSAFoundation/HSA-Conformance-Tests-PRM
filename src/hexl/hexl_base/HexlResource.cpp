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

#include "HexlResource.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <cassert>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#endif

namespace hexl {

std::istream* DirectoryResourceManager::Get(const std::string& name)
{
  std::ifstream* in = new std::ifstream(GetBasedName(name).c_str());
  if (!in->is_open()) { delete in; return 0; }
  return in;
}

std::ostream* DirectoryResourceManager::GetOutput(const std::string& name)
{
  std::ofstream* out = new std::ofstream(GetOutputFileName(name).c_str());
  if (!out->is_open()) { delete out; return 0; }
  return out;
}

std::string DirectoryResourceManager::GetBasedName(const std::string& name) const
{ 
  std::string filename = testbase;
  if (!filename.empty()) { filename += "/"; }
  filename += name;
  return filename;
}

std::string DirectoryResourceManager::GetOutputFileName(const std::string& name) const
{
  std::string filename = results;
  if (!filename.empty()) { filename += "/"; }
  filename += name;
  std::string dirname = Basename(filename);
  if (!dirname.empty()) { MkdirPath(dirname); }
  return filename;
}

bool DirectoryResourceManager::MkdirPath(const std::string& name) const
{
  size_t pos = name.find_first_of("/\\");
  while (true) {
    std::string pname = name.substr(0, pos);
#ifdef _WIN32
    if (!CreateDirectory(pname.c_str(), NULL) &&
        ERROR_ALREADY_EXISTS != GetLastError()) {
      std::cout << "Warning: failed to mkdir " << pname << std::endl;
      return false;
    }
#else // Assume Linux.
    if (0 != mkdir(pname.c_str(), 0777) && errno != EEXIST) {
      std::cout << "Warning: failed to mkdir " << pname << std::endl;
      return false;
    }
#endif
    if (pos == std::string::npos) {
      break;
    }
    pos = name.find_first_of("/\\", pos + 1);
  }
  return true;
}

std::string Basename(const std::string& name)
{
  size_t pos = name.find_last_of("/\\");
  if (pos != std::string::npos) {
    return name.substr(0, pos);
  } else {
    return "";
  }
}

std::string LoadTextResource(ResourceManager* rm, const std::string& name)
{
  assert(rm);
  std::istream* in = rm->Get(name);
  if (!in) { return ""; }
  std::string res;
  std::string line;
  unsigned linen = 0;
  while (getline(*in, line)) {
    res += line;
    res += "\n";
  }
  delete in;
  return res;
}

bool SaveTextResource(ResourceManager* rm, const std::string& name, const std::string& text)
{
  std::string fileName = rm->GetOutputFileName(name);
  std::ofstream out(fileName);
  if (!out.is_open()) { return false; }
  out << text;
  out.close();
  return true;
}

std::string LoadFile(const std::string& name)
{
  std::ifstream in(name);
  if (!in) { return ""; }
  std::string res;
  std::string line;
  unsigned linen = 0;
  while (getline(in, line)) {
    res += line;
    res += "\n";
  }
  return res;
}

std::string LoadBinaryFile(const std::string& name)
{
  std::ifstream in(name, std::ios::binary | std::ios::ate);
  if (!in) { return ""; }
  std::string res;
  res.reserve(std::string::size_type(in.tellg()));
  in.seekg(0, std::ios::beg);
  res.assign((std::istreambuf_iterator<char>(in)),
              std::istreambuf_iterator<char>());
  return res;
}

std::vector<char> LoadBinaryResource(ResourceManager* rm, const std::string& name)
{
  std::istream* in = rm->Get(name);
  std::vector<char> res = std::vector<char>(std::istreambuf_iterator<char>(*in),
                                            std::istreambuf_iterator<char>());
  delete in;
  return res;
}

bool SaveBinaryResource(ResourceManager* rm, const std::string& name, const void *buffer, size_t bufferSize)
{
  std::string fileName = rm->GetOutputFileName(name);
  std::ofstream out(fileName, std::ios_base::out | std::ios_base::binary);
  if (!out.is_open()) { return false; }
  out.write(static_cast<const char *>(buffer), bufferSize);
  out.close();
  return true;
}

}
