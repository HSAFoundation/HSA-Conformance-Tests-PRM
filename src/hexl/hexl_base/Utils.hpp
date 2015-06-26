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

#ifndef HEXL_UTILS_HPP
#define HEXL_UTILS_HPP

#include "Brig.h"
#include "HSAILBrigContainer.h"
#include <string>
#include "MObject.hpp"
#include "Image.hpp"

namespace HSAIL_ASM { class BrigContainer; }

namespace hexl {

enum EndiannessConfig {
  ENDIANNESS_LITTLE,
  ENDIANNESS_BIG
};

BrigMachineModel8_t GetBrigMachineModel(HSAIL_ASM::BrigContainer* brig);
BrigCodeOffset32_t GetBrigUniqueKernelOffset(HSAIL_ASM::BrigContainer* brig);
std::string GetBrigKernelName(HSAIL_ASM::BrigContainer* brig, BrigCodeOffset32_t kernelOffset);
unsigned GetBrigKernelInArgCount(HSAIL_ASM::BrigContainer* brig, BrigCodeOffset32_t kernelOffset);
std::string ExtractTestPath(const std::string& name, unsigned level);
hexl::ValueType Brig2ValueType(BrigType type);
std::string ValueType2Str(hexl::ValueType vtype);
BrigType Value2BrigType(hexl::ValueType type);
bool Is128Bit(BrigType type);
std::string ExceptionsNumber2Str(uint32_t exceptionsNumber);
uint32_t ImageGeometryDims(BrigImageGeometry geometry);
bool IsImageGeometryArray(BrigImageGeometry geometry);
bool IsImageDepth(BrigImageGeometry geometry);
bool IsImageQueryGeometrySupport(BrigImageGeometry imageGeometryProp, BrigImageQuery imageQuery);
bool IsImageGeometrySupported(BrigImageGeometry imageGeometryProp, ImageGeometry imageGeometry);
bool IsImageLegal(BrigImageGeometry geometry, BrigImageChannelOrder channelOrder, BrigImageChannelType channelType);
bool IsImageOptional(BrigImageGeometry geometry, BrigImageChannelOrder channelOrder, BrigImageChannelType channelType, BrigType accessPermission);
bool IsSamplerLegal(BrigSamplerCoordNormalization coord, BrigSamplerFilter filter, BrigSamplerAddressing addressing);
BrigType ImageAccessType(BrigImageChannelType channelType);

EndiannessConfig PlatformEndianness(void);
void SwapEndian(void* ptr, size_t typeSize);

template <>
struct Serializer<HSAIL_ASM::BrigContainer*> {
  static void Write(std::ostream& out, const HSAIL_ASM::BrigContainer& brig) {
    /*
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
    */
    assert(false);
  }
  static void Read(std::istream& in, HSAIL_ASM::BrigContainer& brig) {
    /*
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
    brig = brig_container_create_copy(buffer, data, code, operand, 0);
    */
    assert(false);
  }
};

}

#endif // HEXL_UTILS_HPP
