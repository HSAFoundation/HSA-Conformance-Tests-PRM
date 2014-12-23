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

#ifndef HEXL_RESOURCE_HPP
#define HEXL_RESOURCE_HPP

#include <iostream>
#include <string>
#include <vector>

namespace hexl {

class ResourceManager {
public:
  virtual ~ResourceManager() { }
  virtual std::istream* Get(const std::string& name) = 0;
  virtual std::string GetBasedName(const std::string& name) const = 0;
  virtual std::string GetOutputFileName(const std::string& name) const = 0;
  virtual std::ostream* GetOutput(const std::string& name) = 0;
};

class DirectoryResourceManager : public ResourceManager {
public:
  DirectoryResourceManager(const std::string& testbase_, const std::string& results_)
    : testbase(testbase_), results(results_) { }
  virtual std::istream* Get(const std::string& name);
  virtual std::string GetBasedName(const std::string& name) const;
  virtual std::string GetOutputFileName(const std::string& name) const;
  virtual std::ostream* GetOutput(const std::string& name);
private:
  std::string testbase;
  std::string results;
  bool MkdirPath(const std::string& name) const;
};

std::string Basename(const std::string& name);
std::string LoadTextResource(ResourceManager* rm, const std::string& name);
bool SaveTextResource(ResourceManager* rm, const std::string& name, const std::string& text);
std::string LoadFile(const std::string& name);
std::string LoadBinaryFile(const std::string& name);
std::vector<char> LoadBinaryResource(ResourceManager* rm, const std::string& name);
bool SaveBinaryResource(ResourceManager* rm, const std::string& name, const void *buffer, size_t bufferSize);

}

#endif // HEXL_RESOURCE_HPP
