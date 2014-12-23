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

#include "InitializerTests.hpp"
#include "BrigEmitter.hpp"
#include "BasicHexlTests.hpp"
#include "HCTests.hpp"
#include <sstream>

using namespace Brig;
using namespace HSAIL_ASM;
using namespace hexl;
using namespace hexl::emitter;

namespace hsail_conformance {

  Endianness PlatformEndianness(void) {
    union {
      uint32_t i;
      char c[4];
    } bin = {0x01020304};

    if (bin.c[0] == 1) { 
      return ENDIANNESS_BIG;
    } else {
      return ENDIANNESS_LITTLE;
    }
  }

  void SwapEndian(void* ptr, size_t typeSize) {
    auto chPtr = reinterpret_cast<char *>(ptr);
    std::reverse(chPtr, chPtr + typeSize);
  }


class ValueGenerator {
private:
  static const unsigned MAX_TYPE_SIZE = 8;
  
  unsigned currentByte;
  std::vector<char> charVector;

  void generateVector(size_t size) {
    charVector.clear();
    for (size_t i = 0; i < size; ++i) {
      charVector.push_back(++currentByte % 256);  
    }
  } 

  ValueGenerator(const ValueGenerator&);
  ValueGenerator(const ValueGenerator&&);
  ValueGenerator operator=(const ValueGenerator&);
  ValueGenerator operator=(const ValueGenerator&&);

public:
  ValueGenerator() : currentByte(0), charVector() { 
    charVector.reserve(MAX_TYPE_SIZE);
  };

  Value S8()  { generateVector(1); return hexl::Value(MV_INT8,   *reinterpret_cast<char *>(charVector.data())); }
  Value U8()  { generateVector(1); return hexl::Value(MV_UINT8,  *reinterpret_cast<char *>(charVector.data())); }
  Value S16() { generateVector(2); return hexl::Value(MV_INT16,  *reinterpret_cast<int16_t *>(charVector.data())); }
  Value U16() { generateVector(2); return hexl::Value(MV_UINT16, *reinterpret_cast<uint16_t *>(charVector.data())); }
  Value S32() { generateVector(4); return hexl::Value(MV_INT32,  *reinterpret_cast<int32_t *>(charVector.data())); }
  Value U32() { generateVector(4); return hexl::Value(MV_UINT32, *reinterpret_cast<uint32_t *>(charVector.data())); }
  Value S64() { generateVector(8); return hexl::Value(MV_INT64,  *reinterpret_cast<int64_t *>(charVector.data())); }
  Value U64() { generateVector(8); return hexl::Value(MV_UINT64, *reinterpret_cast<uint64_t *>(charVector.data())); }
  Value F()   { generateVector(4); return hexl::Value(*reinterpret_cast<float *>(charVector.data())); }
  Value D()   { generateVector(8); return hexl::Value(*reinterpret_cast<double *>(charVector.data())); }

  Value Generate(ValueType type) {
   switch (type)
    {
    case MV_INT8:   return S8();
    case MV_UINT8:  return U8();
    case MV_INT16:  return S16();
    case MV_UINT16: return U16();
    case MV_INT32:  return S32();
    case MV_UINT32: return U32();
    case MV_INT64:  return S64();
    case MV_UINT64: return U64();
    case MV_FLOAT:  return F();
    case MV_DOUBLE: return D();
    default: assert(false); return hexl::Value();
    }
  }
};


class InitializerTest : public Test {
private:
  Variable var;
  BrigTypeX type;
  BrigSegment segment;
  uint64_t dim;
  Values data;
  bool isConst;
  ValueGenerator generator;

  uint64_t DataSize() const { return std::max((uint32_t) dim, (uint32_t) 1); }
  uint64_t TypeSize() const {return getBrigTypeNumBytes(type); }
  hexl::ValueType ValueType() const { return Brig2ValueType(type); }
  BrigTypeX SegmentAddrType() const {
    return getSegAddrSize(segment, te->CoreCfg()->IsLarge()) == 32 ? BRIG_TYPE_U32 : BRIG_TYPE_U64;
  }

  void PushResult(Values* result, void *data, hexl::ValueType type) const {
    auto typeSize = ValueTypeSize(type);
    if (te->CoreCfg()->Endianness() != PlatformEndianness()) {
      SwapEndian(data, typeSize);
    }
    auto chPtr = reinterpret_cast<char *>(data);
    for (size_t i = 0; i < typeSize; ++i) {
      result->push_back(Value(MV_UINT8, chPtr[i]));
    }
  }

public:
  InitializerTest(Grid geometry, BrigTypeX type_, BrigSegment segment_, uint64_t dim_, bool isConst_)
    : Test(Location::KERNEL, geometry), 
      type(type_), segment(segment_), dim(dim_), data(), isConst(isConst_),
      generator()
  {
  }

  bool IsValid() const override {
    return (segment == BRIG_SEGMENT_GLOBAL || segment == BRIG_SEGMENT_READONLY) &&
           data.size() < DataSize();
  }

  void Init() override {
    Test::Init();
    var = kernel->NewVariable("var", segment, type, Location::MODULE, BRIG_ALIGNMENT_NONE, dim, isConst);
    for (uint32_t i = 0; i < DataSize(); ++i) {
      auto val = generator.Generate(ValueType());
      data.push_back(val);
      switch (type) {
      case BRIG_TYPE_S8:  var->PushBack(val.Data().s8);  break;
      case BRIG_TYPE_U8:  var->PushBack(val.Data().u8);  break;
      case BRIG_TYPE_S16: var->PushBack(val.Data().s16); break;
      case BRIG_TYPE_U16: var->PushBack(val.Data().u16); break;
      case BRIG_TYPE_S32: var->PushBack(val.Data().s32); break;
      case BRIG_TYPE_U32: var->PushBack(val.Data().u32); break;
      case BRIG_TYPE_S64: var->PushBack(val.Data().s64); break;
      case BRIG_TYPE_U64: var->PushBack(val.Data().u64); break;
      case BRIG_TYPE_F16: var->PushBack(val.Data().f);   break;
      case BRIG_TYPE_F32: var->PushBack(val.Data().f);   break;
      case BRIG_TYPE_F64: var->PushBack(val.Data().d);   break;
      default: assert(false);
      }
    }
  }

  void Name(std::ostream& out) const override {
    if (isConst) {
      out << "const_";
    }
    out << segment2str(segment) << "_" << typeX2str(type);
    if (dim != 0) { 
      out << "[" << dim << "]"; 
    }
  }

  uint64_t ResultDim() const override {
    return DataSize() * TypeSize();
  }

  BrigTypeX ResultType() const override { return BRIG_TYPE_U8; }

  void ExpectedResults(Values* result) const override {
    for (auto val: data) {
      switch (val.Type()) {
      case MV_INT8:    { auto data = val.Data().s8;  PushResult(result, &data, val.Type()); break; }
      case MV_UINT8:   { auto data = val.Data().u8;  PushResult(result, &data, val.Type()); break; }
      case MV_INT16:   { auto data = val.Data().s16; PushResult(result, &data, val.Type()); break; }
      case MV_UINT16:  { auto data = val.Data().u16; PushResult(result, &data, val.Type()); break; }
      case MV_INT32:   { auto data = val.Data().s32; PushResult(result, &data, val.Type()); break; }
      case MV_UINT32:  { auto data = val.Data().u32; PushResult(result, &data, val.Type()); break; }
      case MV_INT64:   { auto data = val.Data().s64; PushResult(result, &data, val.Type()); break; }
      case MV_UINT64:  { auto data = val.Data().u64; PushResult(result, &data, val.Type()); break; }
      case MV_FLOAT16: { auto data = val.Data().f;   PushResult(result, &data, val.Type()); break; }
      case MV_FLOAT:   { auto data = val.Data().f;   PushResult(result, &data, val.Type()); break; }
      case MV_DOUBLE:  { auto data = val.Data().d;   PushResult(result, &data, val.Type()); break; }
      default: assert(false);
      }
    }
  }

  void KernelCode() override {
    assert(codeLocation == KERNEL);
    
    auto forEach = "@for_each";
    auto forByte = "@for_byte";
    
    // for-each loop counter
    auto forEachCount = be.AddTReg(BRIG_TYPE_U32);
    be.EmitMov(forEachCount, be.Immed(forEachCount->Type(), 0));

    // for-each loop inside var
    be.EmitLabel(forEach);

    // calculate offset inside var
    auto offsetBase = be.AddAReg(BRIG_SEGMENT_GLOBAL);
    auto cvt = be.AddTReg(offsetBase->Type());
    if (cvt->Type() != forEachCount->Type()) {
      be.EmitCvt(cvt, forEachCount);
    } else {
      be.EmitMov(cvt, forEachCount);
    }
    be.EmitArith(BRIG_OPCODE_MUL, offsetBase, cvt, be.Immed(offsetBase->Type(), TypeSize()));  
    
    // generate code to read each byte from var
    auto result = be.AddTReg(BRIG_TYPE_U8);
    auto offset = be.AddTReg(offsetBase->Type());
    auto wiId = be.EmitWorkitemFlatAbsId(offsetBase->IsLarge());
    
    // read each byte in loop
    auto byteCount = be.AddTReg(offset->Type());
    be.EmitMov(byteCount, be.Immed(byteCount->Type(), 0));
    be.EmitLabel(forByte);
   
    // load one byte from element and store it in output register
    be.EmitArith(BRIG_OPCODE_ADD, offset, offsetBase, byteCount->Reg());
    be.EmitLoad(segment, BRIG_TYPE_U8, result->Reg(), be.Address(var->Variable(), offset->Reg(), 0)); 

    // store byte in output buffer
    be.EmitArith(BRIG_OPCODE_MAD, offset, wiId, be.Immed(wiId->Type(), ResultDim()), offset);
    output->EmitStoreData(result, offset);
    
    auto cmp = be.AddCTReg();
    be.EmitArith(BRIG_OPCODE_ADD, byteCount, byteCount, be.Immed(byteCount->Type(), 1));
    be.EmitCmp(cmp->Reg(), byteCount, be.Immed(byteCount->Type(), TypeSize()), BRIG_COMPARE_LT);
    be.EmitCbr(cmp, forByte);

    // iterate until the end of var
    be.EmitArith(BRIG_OPCODE_ADD, forEachCount, forEachCount, be.Immed(forEachCount->Type(), 1));
    be.EmitCmp(cmp->Reg(), forEachCount, be.Immed(forEachCount->Type(), DataSize()), BRIG_COMPARE_LT);
    be.EmitCbr(cmp, forEach);
  }

  void ModuleVariables() override {
    Test::ModuleVariables();
    var->ModuleVariables();
  }
};


void InitializerTests::Iterate(hexl::TestSpecIterator& it)
{
  CoreConfig* cc = CoreConfig::Get(context);
  TestForEach<InitializerTest>(cc->Ap(), it, "initializer", cc->Grids().DefaultGeometrySet(), cc->Types().Compound(), cc->Segments().InitializableSegments(), cc->Variables().InitializerDims(), Bools::All());
}

}
