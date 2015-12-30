/*
   Copyright 2013-2015 Heterogeneous System Architecture (HSA) Foundation

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

#ifndef INCLUDED_HSAIL_TESTGEN_EMULATOR_H
#define INCLUDED_HSAIL_TESTGEN_EMULATOR_H

#include "HSAILTestGenVal.h"

namespace TESTGEN {

// ============================================================================
// HSAIL Instructions Emulator

// Instructions Emulator computes result of HSAIL instruction execution
// based on input values arg0..arg4.
// This result depends on instruction and may include:
// - value placed into destination register
// - value placed into memory

// Initial value used for initialization of dst before
// packed instructions which only modify part of dst
extern uint64_t initialPackedVal;

// Emulate execution of instruction 'inst' using provided input values.
// Return value stored into destination register or an empty value
// if there is no destination or if emulation failed.
Val emulateDstVal(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4);

// Emulate execution of instruction 'inst' using provided input values.
// Return value stored into memory or an empty value if this
// instruction does not modify memory or if emulation failed.
Val emulateMemVal(Inst inst, Val arg0, Val arg1, Val arg2, Val arg3, Val arg4);

// Returns expected accuracy for an HSAIL instruction.
// The values in the (0,1) range specify relative precision.
// Values >= 1 denote precision in ULPS, calculated as (value - 0.5), i.e. 1.0 means 0.5 ULPS.
// Values <= 0 denote absolute precision (CM_DECIMAL in lua). Specifically,
// when value == 0, the precision is infinite (no deviation is allowed).
double getPrecision(Inst inst);

// ============================================================================

} // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_EMULATOR_H
