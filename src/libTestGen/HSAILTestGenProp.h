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

#ifndef INCLUDED_HSAIL_TESTGEN_PROP_H
#define INCLUDED_HSAIL_TESTGEN_PROP_H

#include "HSAILInstProps.h"
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>
#include <map>

using std::string;
using std::vector;
using std::map;

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================
// Brig operands created for testing

// NB: The order of operands in this list affects generated tests in optimal search mode.
// Operands which are low in this list will less likely appear in generated tests.
enum BrigOperandId
{
    O_MINID = 0,

    O_CREG,
    O_SREG,
    O_DREG,
    O_QREG,

    O_VEC2_R32_SRC,
    O_VEC3_R32_SRC,
    O_VEC4_R32_SRC,
    O_VEC2_R64_SRC,
    O_VEC3_R64_SRC,
    O_VEC4_R64_SRC,
    O_VEC2_R128_SRC,
    O_VEC3_R128_SRC,
    O_VEC4_R128_SRC,

    O_VEC2_I_U8_SRC,
    O_VEC3_I_U8_SRC,
    O_VEC4_I_U8_SRC,
    O_VEC2_M_U8_SRC,
    O_VEC3_M_U8_SRC,
    O_VEC4_M_U8_SRC,

    O_VEC2_I_S8_SRC,
    O_VEC3_I_S8_SRC,
    O_VEC4_I_S8_SRC,
    O_VEC2_M_S8_SRC,
    O_VEC3_M_S8_SRC,
    O_VEC4_M_S8_SRC,

    O_VEC2_I_U16_SRC,
    O_VEC3_I_U16_SRC,
    O_VEC4_I_U16_SRC,
    O_VEC2_M_U16_SRC,
    O_VEC3_M_U16_SRC,
    O_VEC4_M_U16_SRC,

    O_VEC2_I_S16_SRC,
    O_VEC3_I_S16_SRC,
    O_VEC4_I_S16_SRC,
    O_VEC2_M_S16_SRC,
    O_VEC3_M_S16_SRC,
    O_VEC4_M_S16_SRC,

    O_VEC2_I_F16_SRC,
    O_VEC3_I_F16_SRC,
    O_VEC4_I_F16_SRC,
    O_VEC2_M_F16_SRC,
    O_VEC3_M_F16_SRC,
    O_VEC4_M_F16_SRC,

    O_VEC2_I_U32_SRC,
    O_VEC3_I_U32_SRC,
    O_VEC4_I_U32_SRC,
    O_VEC2_M_U32_SRC,
    O_VEC3_M_U32_SRC,
    O_VEC4_M_U32_SRC,

    O_VEC2_I_S32_SRC,
    O_VEC3_I_S32_SRC,
    O_VEC4_I_S32_SRC,
    O_VEC2_M_S32_SRC,
    O_VEC3_M_S32_SRC,
    O_VEC4_M_S32_SRC,

    O_VEC2_I_F32_SRC,
    O_VEC3_I_F32_SRC,
    O_VEC4_I_F32_SRC,
    O_VEC2_M_F32_SRC,
    O_VEC3_M_F32_SRC,
    O_VEC4_M_F32_SRC,

    O_VEC2_I_U64_SRC,
    O_VEC3_I_U64_SRC,
    O_VEC4_I_U64_SRC,
    O_VEC2_M_U64_SRC,
    O_VEC3_M_U64_SRC,
    O_VEC4_M_U64_SRC,

    O_VEC2_I_S64_SRC,
    O_VEC3_I_S64_SRC,
    O_VEC4_I_S64_SRC,
    O_VEC2_M_S64_SRC,
    O_VEC3_M_S64_SRC,
    O_VEC4_M_S64_SRC,

    O_VEC2_I_F64_SRC,
    O_VEC3_I_F64_SRC,
    O_VEC4_I_F64_SRC,
    O_VEC2_M_F64_SRC,
    O_VEC3_M_F64_SRC,
    O_VEC4_M_F64_SRC,

    O_VEC2_I_B128_SRC,
    O_VEC3_I_B128_SRC,
    O_VEC4_I_B128_SRC,
    O_VEC2_M_B128_SRC,
    O_VEC3_M_B128_SRC,
    O_VEC4_M_B128_SRC,

    O_VEC2_R32_DST,
    O_VEC3_R32_DST,
    O_VEC4_R32_DST,
    O_VEC2_R64_DST,
    O_VEC3_R64_DST,
    O_VEC4_R64_DST,
    O_VEC2_R128_DST,
    O_VEC3_R128_DST,
    O_VEC4_R128_DST,

    O_VEC2_SIG32_SRC,
    O_VEC3_SIG32_SRC,
    O_VEC4_SIG32_SRC,

    O_VEC2_SIG64_SRC,
    O_VEC3_SIG64_SRC,
    O_VEC4_SIG64_SRC,

    O_IMM_U8,
    O_IMM_S8,

    O_IMM_U16,
    O_IMM_S16,
    O_IMM_F16,

    O_IMM_U32,
    O_IMM_S32,
    O_IMM_F32,

    O_IMM_U64,
    O_IMM_S64,
    O_IMM_F64,

    O_IMM_U8X4,
    O_IMM_S8X4,
    O_IMM_U16X2,
    O_IMM_S16X2,
    O_IMM_F16X2,

    O_IMM_U8X8,
    O_IMM_S8X8,
    O_IMM_U16X4,
    O_IMM_S16X4,
    O_IMM_F16X4,
    O_IMM_U32X2,
    O_IMM_S32X2,
    O_IMM_F32X2,

    O_IMM_U8X16,
    O_IMM_S8X16,
    O_IMM_U16X8,
    O_IMM_S16X8,
    O_IMM_F16X8,
    O_IMM_U32X4,
    O_IMM_S32X4,
    O_IMM_F32X4,
    O_IMM_U64X2,
    O_IMM_S64X2,
    O_IMM_F64X2,

    O_IMM_U32_0,
    O_IMM_U32_1,
    O_IMM_U32_2,
    O_IMM_U32_3,

    O_IMM_SIG32,
    O_IMM_SIG64,

    O_WAVESIZE,

    O_LABELREF,
    O_FUNCTIONREF,
    O_IFUNCTIONREF,
    O_KERNELREF,
    O_SIGNATUREREF,
    O_FBARRIERREF,

    O_ADDRESS_GLOBAL_VAR,
    O_ADDRESS_READONLY_VAR,

    O_ADDRESS_GROUP_VAR,        // 32-bit segment
    O_ADDRESS_PRIVATE_VAR,      // 32-bit segment

    O_ADDRESS_GLOBAL_ROIMG,
    O_ADDRESS_GLOBAL_WOIMG,
    O_ADDRESS_GLOBAL_RWIMG,

    O_ADDRESS_READONLY_ROIMG,
    O_ADDRESS_READONLY_RWIMG,

    O_ADDRESS_GLOBAL_SAMP,
    O_ADDRESS_READONLY_SAMP,

    O_ADDRESS_GLOBAL_SIG32,
    O_ADDRESS_READONLY_SIG32,

    O_ADDRESS_GLOBAL_SIG64,
    O_ADDRESS_READONLY_SIG64,

    O_ADDRESS_FLAT_DREG,
    O_ADDRESS_FLAT_SREG,
    O_ADDRESS_FLAT_OFF,

    O_JUMPTAB,          // currently not used
    O_CALLTAB,          // currently not used

    O_NULL,

    O_MAXID
};

inline bool isImmOperandId(unsigned val)
{
    switch(val)
    {
    case O_IMM_U8:
    case O_IMM_S8:

    case O_IMM_U16:
    case O_IMM_S16:
    case O_IMM_F16:

    case O_IMM_U32:
    case O_IMM_S32:
    case O_IMM_F32:

    case O_IMM_U64:
    case O_IMM_S64:
    case O_IMM_F64:

    case O_IMM_U8X4: 
    case O_IMM_S8X4: 
    case O_IMM_U16X2:
    case O_IMM_S16X2:
    case O_IMM_F16X2:

    case O_IMM_U8X8:
    case O_IMM_S8X8:
    case O_IMM_U16X4:
    case O_IMM_S16X4:
    case O_IMM_F16X4:
    case O_IMM_U32X2:
    case O_IMM_S32X2:
    case O_IMM_F32X2:

    case O_IMM_U8X16:
    case O_IMM_S8X16:
    case O_IMM_U16X8:
    case O_IMM_S16X8:
    case O_IMM_F16X8:
    case O_IMM_U32X4:
    case O_IMM_S32X4:
    case O_IMM_F32X4:
    case O_IMM_U64X2:
    case O_IMM_S64X2:
    case O_IMM_F64X2:
        return true;

    case O_IMM_U32_0:
    case O_IMM_U32_1:
    case O_IMM_U32_2:
    case O_IMM_U32_3:
        return true;

    case O_IMM_SIG32:
    case O_IMM_SIG64:
        return true;

    case O_WAVESIZE:
        return true;

    default:
        return false;
    }
}

unsigned operandId2SymId(unsigned oprId);
bool     isSupportedOperand(unsigned oprId);

//=============================================================================
//=============================================================================
//=============================================================================
// Equivalence class values

enum BrigEqClassId
{
    EQCLASS_MINID = 0,

    EQCLASS_0,
    EQCLASS_1,
    EQCLASS_2,
    EQCLASS_255,

    EQCLASS_MAXID
};

//=============================================================================
//=============================================================================
//=============================================================================
// Symbols

enum SymId
{
    SYM_NONE = 0,
    SYM_MINID = 0,

    SYM_FUNC,
    SYM_IFUNC,
    SYM_KERNEL,
    SYM_SIGNATURE,
    SYM_GLOBAL_VAR,
    SYM_GROUP_VAR,
    SYM_PRIVATE_VAR,
    SYM_READONLY_VAR,
    SYM_GLOBAL_ROIMG,
    SYM_GLOBAL_WOIMG,
    SYM_GLOBAL_RWIMG,
    SYM_READONLY_ROIMG,
    SYM_READONLY_RWIMG,
    SYM_GLOBAL_SAMP,
    SYM_READONLY_SAMP,
    SYM_GLOBAL_SIG32,
    SYM_READONLY_SIG32,
    SYM_GLOBAL_SIG64,
    SYM_READONLY_SIG64,
    SYM_FBARRIER,
    SYM_LABEL,

    SYM_MAXID
};

struct SymDesc
{
    unsigned    id;
    const char* name;
    unsigned    type;
    unsigned    dim;
    unsigned    segment;
};

const char* getSymName(unsigned symId);
unsigned    getSymType(unsigned symId);
unsigned    getSymDim(unsigned symId);
unsigned    getSymSegment(unsigned symId);
bool        isSupportedSym(unsigned symId);

//=============================================================================
//=============================================================================
//=============================================================================

string prop2str(unsigned id);
string operand2str(unsigned id);
string val2str(unsigned id, unsigned val);

const unsigned* getValMapDesc(unsigned* size);

//=============================================================================

using namespace HSAIL_PROPS;

struct PropDeleter;

//=============================================================================
//=============================================================================
//=============================================================================
// Description of instruction property

class Prop
{
protected:
    const unsigned propId;

    vector<unsigned> pValues;   // Positive (valid) values this property may take
    vector<unsigned> nValues;   // All possible values for this property (both valid and invalid)

    unsigned pPos;              // Current position in pValues
    unsigned nPos;              // Current position in nvalues

    friend struct PropDeleter;

    //==========================================================================
private:
    Prop(const Prop&); // non-copyable
    const Prop &operator=(const Prop &);  // not assignable

protected:
    Prop(unsigned id) : propId(id)
    {
        using namespace HSAIL_PROPS;
        assert(PROP_MINID <= id && id < PROP_MAXID);
        reset();
    }

public:
    virtual ~Prop() {}

    //==========================================================================
public:
    static Prop* create(unsigned propId, const unsigned* pVals, unsigned pValsNum, const unsigned* nVals, unsigned nValsNum);

    //==========================================================================
public:
    unsigned getPropId() const { return propId; }

    void reset() { resetPositive(); resetNegative(); }
    void resetPositive() { pPos = 0; }
    void resetNegative() { nPos = 0; }

    unsigned getCurrentPositive() const
    {
        assert(0 < pPos && pPos <= pValues.size());
        return pValues[pPos - 1];
    }

    unsigned getCurrentNegative() const
    {
        assert(0 < nPos && nPos <= nValues.size());
        return nValues[nPos - 1];
    }

    bool findNextPositive() { return pPos++ < pValues.size(); }
    bool findNextNegative() { return nPos++ < nValues.size(); }

    //==========================================================================
private:
    void init(const unsigned* pVals, unsigned pValsNum, const unsigned* nVals, unsigned nValsNum);
    void tryRemoveImmOperands();

    //==========================================================================
protected:
    bool isPositive(unsigned val) const         { return std::find(pValues.begin(), pValues.end(), val) != pValues.end(); }
    bool isNegative(unsigned val) const         { return std::find(nValues.begin(), nValues.end(), val) != nValues.end(); }

    virtual void appendPositive(unsigned val)   { if (!isPositive(val)) pValues.push_back(val); }
    virtual void appendNegative(unsigned val)   { if (!isNegative(val)) nValues.push_back(val); }

};

//=============================================================================
//=============================================================================
//=============================================================================

// Special container for non-brig properties
// Each HDL value is translated to a set of TestGen values (see valMapDesc)
class ExtProp : public Prop
{
private:
    static map<unsigned, const unsigned*> valMap; // HDL to TG translation

private:
    ExtProp(const ExtProp&); // non-copyable
    const ExtProp &operator=(const ExtProp &);  // not assignable

    //==========================================================================
public:
    ExtProp(unsigned id) : Prop(id) { initValMap(); }
    virtual ~ExtProp() {}

    //==========================================================================
protected:
    virtual void appendPositive(unsigned val)
    {
        assert(VAL_MINID < val && val < VAL_MAXID);
        for (const unsigned* vals = translateVal(val); *vals != 0; ++vals)
        {
            if (isOperandProp(propId) && !isSupportedOperand(*vals)) continue;

            Prop::appendPositive(*vals);
        }
    }

    virtual void appendNegative(unsigned val)
    {
        assert(VAL_MINID < val && val < VAL_MAXID);
        for (const unsigned* vals = translateVal(val); *vals != 0; ++vals)
        {
            if (isOperandProp(propId) && !isSupportedOperand(*vals)) continue;

            //F: This is to avoid problems with disassembler (it fails with assert if some operands are 0)
            if (isOperandProp(propId) && *vals == O_NULL && !isPositive(*vals)) continue;
            Prop::appendNegative(*vals); // replace "0" with INVALID_VAL_XXX
        }
    }

    //==========================================================================
    // HDL to TG mapping

private:
    static const unsigned* translateVal(unsigned hdlVal) //F need more abstract interface
    {
        assert(hdlVal != 0);
        assert(VAL_MINID < hdlVal && hdlVal < VAL_MAXID);
        assert(valMap.count(hdlVal) != 0);

        for (const unsigned* vals = valMap[hdlVal]; *vals != 0; ++vals) assert(*vals < O_MAXID); // this is for debugging only

        return valMap[hdlVal];
    }

    static void initValMap()
    {
        if (valMap.empty())
        {
            unsigned valMapSize;
            const unsigned* valMapDesc = getValMapDesc(&valMapSize);
            for (unsigned i = 0; i < valMapSize; ++i)
            {
                if (i == 0 || valMapDesc[i - 1] == 0)
                {
                    assert(valMapDesc[i] != 0);
                    assert(VAL_MINID < valMapDesc[i] && valMapDesc[i] < VAL_MAXID);
                    assert(valMap.count(valMapDesc[i]) == 0);

                    valMap[valMapDesc[i]] = valMapDesc + i + 1;
                }
            }
        }
    }
};

//=============================================================================
//=============================================================================
//=============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_PROP_H