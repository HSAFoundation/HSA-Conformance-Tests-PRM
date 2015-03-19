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

#include "PrmCoreTests.hpp"
#include "MiscOperationsTests.hpp"
#include "BranchTests.hpp"
#include "CrossLaneTests.hpp"
#include "BarrierTests.hpp"
#include "AtomicTests.hpp"
#include "AddressTests.hpp"
#include "FunctionsTests.hpp"
#include "UserModeQueueTests.hpp"
#include "DispatchPacketTests.hpp"
#include "SignalTests.hpp"
#include "MemoryFenceTests.hpp"
#include "InitializerTests.hpp"
#include "DirectiveTests.hpp"
#include "LimitsTests.hpp"
#include "ExceptionsTests.hpp"
#include "LibrariesTests.hpp"
#ifdef ENABLE_HEXL_HSAILTESTGEN
#include "HexlTestGen.hpp"
#endif // ENABLE_HEXL_HSAILTESTGEN
#include "CoreConfig.hpp"

using namespace hexl;
#ifdef ENABLE_HEXL_HSAILTESTGEN
using namespace hexl::TestGen;
#endif // ENABLE_HEXL_HSAILTESTGEN

namespace hsail_conformance { 

class ArithmeticOperationsTests  : public hexl::TestSetUnion {
public:
  ArithmeticOperationsTests () : TestSetUnion("arithmetic") {
#ifdef ENABLE_HEXL_HSAILTESTGEN
    Add(new TestGenTestSet("intfp", "abs", BRIG_OPCODE_ABS));
    Add(new TestGenTestSet("intfp", "add", BRIG_OPCODE_ADD));
    Add(new TestGenTestSet("intfp", "borrow", BRIG_OPCODE_BORROW));
    Add(new TestGenTestSet("intfp", "carry", BRIG_OPCODE_CARRY));
    Add(new TestGenTestSet("intfp", "div", BRIG_OPCODE_DIV));
    Add(new TestGenTestSet("intfp", "max", BRIG_OPCODE_MAX));
    Add(new TestGenTestSet("intfp", "min", BRIG_OPCODE_MIN));
    Add(new TestGenTestSet("intfp", "mul", BRIG_OPCODE_MUL));
    Add(new TestGenTestSet("intfp", "mulhi", BRIG_OPCODE_MULHI));
    Add(new TestGenTestSet("intfp", "neg", BRIG_OPCODE_NEG));
    Add(new TestGenTestSet("intfp", "rem", BRIG_OPCODE_REM));
    Add(new TestGenTestSet("intfp", "sub", BRIG_OPCODE_SUB));
    Add(new TestGenTestSet("intfp", "ceil", BRIG_OPCODE_CEIL));
    Add(new TestGenTestSet("intfp", "floor", BRIG_OPCODE_FLOOR));
    Add(new TestGenTestSet("intfp", "fma", BRIG_OPCODE_FMA));
    Add(new TestGenTestSet("intfp", "fract", BRIG_OPCODE_FRACT));
    Add(new TestGenTestSet("intfp", "rint", BRIG_OPCODE_RINT));
    Add(new TestGenTestSet("intfp", "sqrt", BRIG_OPCODE_SQRT));
    Add(new TestGenTestSet("intfp", "trunc", BRIG_OPCODE_TRUNC));

    Add(new TestGenTestSet("intopt", "mad", BRIG_OPCODE_MAD));

    Add(new TestGenTestSet("24int", "mad24", BRIG_OPCODE_MAD24));
    Add(new TestGenTestSet("24int", "mad24hi", BRIG_OPCODE_MAD24HI));
    Add(new TestGenTestSet("24int", "mul24", BRIG_OPCODE_MUL24));
    Add(new TestGenTestSet("24int", "mul24hi", BRIG_OPCODE_MUL24HI));

    Add(new TestGenTestSet("intshift", "shl", BRIG_OPCODE_SHL));
    Add(new TestGenTestSet("intshift", "shr", BRIG_OPCODE_SHR));

    Add(new TestGenTestSet("indbit", "and", BRIG_OPCODE_AND));
    Add(new TestGenTestSet("indbit", "or", BRIG_OPCODE_OR));
    Add(new TestGenTestSet("indbit", "xor", BRIG_OPCODE_XOR));
    Add(new TestGenTestSet("indbit", "not", BRIG_OPCODE_NOT));
    Add(new TestGenTestSet("indbit", "popcount", BRIG_OPCODE_POPCOUNT));

    Add(new TestGenTestSet("bitstr", "bitextract", BRIG_OPCODE_BITEXTRACT));
    Add(new TestGenTestSet("bitstr", "bitinsert", BRIG_OPCODE_BITINSERT));
    Add(new TestGenTestSet("bitstr", "bitmask", BRIG_OPCODE_BITMASK));
    Add(new TestGenTestSet("bitstr", "bitrev", BRIG_OPCODE_BITREV));
    Add(new TestGenTestSet("bitstr", "bitselect", BRIG_OPCODE_BITSELECT));
    Add(new TestGenTestSet("bitstr", "firstbit", BRIG_OPCODE_FIRSTBIT));
    Add(new TestGenTestSet("bitstr", "lastbit", BRIG_OPCODE_LASTBIT));

    Add(new TestGenTestSet("copymove", "combine", BRIG_OPCODE_COMBINE));
    Add(new TestGenTestSet("copymove", "expand", BRIG_OPCODE_EXPAND));
    Add(new TestGenTestSet("copymove", "mov", BRIG_OPCODE_MOV));

    Add(new TestGenTestSet("packed", "shuffle", BRIG_OPCODE_SHUFFLE));
    Add(new TestGenTestSet("packed", "unpacklo", BRIG_OPCODE_UNPACKLO));
    Add(new TestGenTestSet("packed", "unpackhi", BRIG_OPCODE_UNPACKHI));
    Add(new TestGenTestSet("packed", "pack", BRIG_OPCODE_PACK));
    Add(new TestGenTestSet("packed", "unpack", BRIG_OPCODE_UNPACK));

    Add(new TestGenTestSet("bitcmov", "cmov", BRIG_OPCODE_CMOV));

    Add(new TestGenTestSet("fpbit", "class", BRIG_OPCODE_CLASS));
    Add(new TestGenTestSet("fpbit", "copysign", BRIG_OPCODE_COPYSIGN));
  
    Add(new TestGenTestSet("nativefp", "nsin",  BRIG_OPCODE_NSIN));
    Add(new TestGenTestSet("nativefp", "ncos",  BRIG_OPCODE_NCOS));
    Add(new TestGenTestSet("nativefp", "nlog2", BRIG_OPCODE_NLOG2));
    Add(new TestGenTestSet("nativefp", "nexp2", BRIG_OPCODE_NEXP2));
    Add(new TestGenTestSet("nativefp", "nsqrt", BRIG_OPCODE_NSQRT));
    Add(new TestGenTestSet("nativefp", "nrsqrt",BRIG_OPCODE_NRSQRT));
    Add(new TestGenTestSet("nativefp", "nrcp",  BRIG_OPCODE_NRCP));
    Add(new TestGenTestSet("nativefp", "nfma",  BRIG_OPCODE_NFMA));

    Add(new TestGenTestSet("multimedia", "bitalign", BRIG_OPCODE_BITALIGN));
    Add(new TestGenTestSet("multimedia", "bytealign", BRIG_OPCODE_BYTEALIGN));
    Add(new TestGenTestSet("multimedia", "lerp", BRIG_OPCODE_LERP));
    Add(new TestGenTestSet("multimedia", "packcvt", BRIG_OPCODE_PACKCVT));
    Add(new TestGenTestSet("multimedia", "unpackcvt", BRIG_OPCODE_UNPACKCVT));
    Add(new TestGenTestSet("multimedia", "sad", BRIG_OPCODE_SAD));
    Add(new TestGenTestSet("multimedia", "sadhi", BRIG_OPCODE_SADHI));

    Add(new TestGenTestSet("compare", "cmp", BRIG_OPCODE_CMP));
    Add(new TestGenTestSet("conversion", "cvt", BRIG_OPCODE_CVT));
#endif // ENABLE_HEXL_HSAILTESTGEN

    Add(new AddressArithmeticTests());
  }

  void InitContext(hexl::Context* context) {
#ifdef ENABLE_HEXL_HSAILTESTGEN
    hexl::emitter::CoreConfig* coreConfig = hexl::emitter::CoreConfig::Get(context);
    if (!context->Has(TestGenConfig::ID)) {
      context->Put(TestGenConfig::ID, new TestGenConfig(coreConfig->Model(), coreConfig->Profile()));
    }
#endif // ENABLE_HEXL_HSAILTESTGEN
    TestSetUnion::InitContext(context);
  }
};

DECLARE_TESTSET_UNION(MemoryOperationsTests);

MemoryOperationsTests::MemoryOperationsTests()
  : TestSetUnion("memory")
{
#ifdef ENABLE_HEXL_HSAILTESTGEN
  Add(new TestGenTestSet("ordinary", "ld", BRIG_OPCODE_LD));
  Add(new TestGenTestSet("ordinary", "st", BRIG_OPCODE_ST));
  Add(new TestGenTestSet("atomic", "ret", BRIG_OPCODE_ATOMIC));
  Add(new TestGenTestSet("atomic", "noret", BRIG_OPCODE_ATOMICNORET));
#endif // ENABLE_HEXL_HSAILTESTGEN
  Add(new SignalTests());
  Add(new MemoryFenceTests());
  Add(new AtomicTests());
}

DECLARE_TESTSET_UNION(ParallelOperationsTests);

ParallelOperationsTests::ParallelOperationsTests()
  : TestSetUnion("parallel")
{
  Add(new CrossLaneOperationsTests());
  Add(new BarrierTests());
}

DECLARE_TESTSET_UNION(SpecialOperationsTests);

SpecialOperationsTests::SpecialOperationsTests()
  : TestSetUnion("special")
{
  Add(new DispatchPacketOperationsTests());
  Add(new ExceptionsTests());
  Add(new UserModeQueueTests());
  Add(new MiscOperationsTests());
}

DECLARE_TESTSET_UNION(VariablesTests);

VariablesTests::VariablesTests()
  : TestSetUnion("variables")
{
  Add(new InitializerTests());
}

DECLARE_TESTSET_UNION(DirectiveTestsUnion);

DirectiveTestsUnion::DirectiveTestsUnion()
  : TestSetUnion("directive")
{
  Add(new DirectiveTests());
}

DECLARE_TESTSET_UNION(LimitsTestsUnion);

LimitsTestsUnion::LimitsTestsUnion()
  : TestSetUnion("limits")
{
  Add(new LimitsTests());
}

DECLARE_TESTSET_UNION(LibrariesTestsUnion);

LibrariesTestsUnion::LibrariesTestsUnion()
  : TestSetUnion("libraries")
{
  Add(new LibrariesTests());
}


DECLARE_TESTSET_UNION(PrmCoreTests);

PrmCoreTests::PrmCoreTests()
  : TestSetUnion("core")
{
  Add(new ArithmeticOperationsTests());
  Add(new MemoryOperationsTests());
  Add(new BranchTests());
  Add(new ParallelOperationsTests());
  Add(new FunctionsTests());
  Add(new SpecialOperationsTests());
  Add(new VariablesTests());
  Add(new DirectiveTestsUnion());
  Add(new LimitsTestsUnion());
  Add(new LimitsTestsUnion());
  Add(new LibrariesTestsUnion());
}

hexl::TestSet* NewPrmCoreTests()
{
  return new PrmCoreTests();
}

}
