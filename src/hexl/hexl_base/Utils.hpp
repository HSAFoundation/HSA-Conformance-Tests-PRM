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

#ifndef HEXL_UTILS_HPP
#define HEXL_UTILS_HPP

#include "Brig.h"
#include "hsail_c.h"
#include <string>
#include "MObject.hpp"

namespace HSAIL_ASM { class BrigContainer; }

namespace hexl {
Brig::BrigMachineModel8_t GetBrigMachineModel(brig_container_t brig);
Brig::BrigCodeOffset32_t GetBrigUniqueKernelOffset(brig_container_t brig);
std::string GetBrigKernelName(brig_container_t brig, Brig::BrigCodeOffset32_t kernelOffset);
unsigned GetBrigKernelInArgCount(brig_container_t brig, Brig::BrigCodeOffset32_t kernelOffset);
brig_container_t CreateBrigFromContainer(HSAIL_ASM::BrigContainer* container);
std::string ExtractTestPath(const std::string& name, unsigned level);
hexl::ValueType Brig2ValueType(Brig::BrigTypeX type);
Brig::BrigTypeX Value2BrigType(hexl::ValueType type);
bool Is128Bit(Brig::BrigTypeX type);

template <>
struct Serializer<brig_container_t> {
  static void Write(std::ostream& out, const brig_container_t& brig) {
    unsigned sectionCount = brig_container_get_section_count(brig);
    size_t totalSize = 0;
    for (unsigned i = 0; i < sectionCount; ++i) {
      totalSize += brig_container_get_section_size(brig, i);
    }
    WriteData(out, sectionCount);
    WriteData(out, totalSize);
    for (unsigned i = 0; i < sectionCount; ++i) {
      size_t sectionSize = brig_container_get_section_size(brig, i);
      WriteData(out, sectionSize);
      out.write(brig_container_get_section_bytes(brig, i), sectionSize);
    }
  }
  static void Read(std::istream& in, brig_container_t& brig) {
    unsigned sectionCount;
    size_t totalSize;
    ReadData(in, sectionCount);
    ReadData(in, totalSize);
    assert(sectionCount == 3);
    char *buffer = new char[totalSize + sectionCount * sizeof(size_t)];
    in.read(buffer, totalSize + sectionCount * sizeof(size_t));
    const char *data = buffer + sizeof(size_t);
    const char *code = data + *((size_t *) data - 1) + sizeof(size_t);
    const char *operand = code + *((size_t *) code - 1) + sizeof(size_t);
    brig = brig_container_create_copy(data, code, operand, 0);
  }
};

}

#endif // HEXL_UTILS_HPP
