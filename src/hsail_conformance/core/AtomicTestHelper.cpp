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

#include "AtomicTestHelper.hpp"

namespace hsail_conformance {

//=====================================================================================

unsigned AtomicTestHelper::wavesize;

//=====================================================================================

TypedReg TestProp::Mov(uint64_t val)                                 const { return test->Mov(type, val); }
TypedReg TestProp::Min(TypedReg val, uint64_t max)                   const { return test->Min(val, max); }
TypedReg TestProp::Cond(unsigned cond, TypedReg val1, uint64_t val2) const { return test->Cond(cond, val1, val2); }
TypedReg TestProp::Cond(unsigned cond, TypedReg val1, TypedReg val2) const { return test->Cond(cond, val1, val2->Reg()); }
TypedReg TestProp::And(TypedReg x, TypedReg y)                       const { return test->And(x, y); }
TypedReg TestProp::And(TypedReg x, uint64_t y)                       const { return test->And(x, y); }
TypedReg TestProp::Or(TypedReg x, TypedReg y)                        const { return test->Or(x, y); }
TypedReg TestProp::Or(TypedReg x, uint64_t y)                        const { return test->Or(x, y); }
TypedReg TestProp::Add(TypedReg x, uint64_t y)                       const { return test->Add(x, y); }
TypedReg TestProp::Sub(TypedReg x, uint64_t y)                       const { return test->Sub(x, y); }
TypedReg TestProp::Mul(TypedReg x, uint64_t y)                       const { return test->Mul(x, y); }
TypedReg TestProp::Shl(uint64_t x, TypedReg y)                       const { return test->Shl(type, x, y); }
TypedReg TestProp::Not(TypedReg x)                                   const { return test->Not(x); }
TypedReg TestProp::PopCount(TypedReg x)                              const { return test->Popcount(x); }
TypedReg TestProp::WgId()                                            const { return test->TestWgId(false); }
uint64_t TestProp::MaxWgId()                                         const { return test->Groups() - 1; }
TypedReg TestProp::Id()                                              const { return test->TestAbsId(getBrigTypeNumBits(type) == 64); }
TypedReg TestProp::Id32()                                            const { return test->TestAbsId(false); }
TypedReg TestProp::Idx()                                             const { return test->Index(); }
TypedReg TestProp::Idx(unsigned arrayId, unsigned access)            const { return test->Index(arrayId, access); }

//=====================================================================================

} // namespace hsail_conformance
