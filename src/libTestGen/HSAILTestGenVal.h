//===-- HSAILTestGenSample.h - HSAIL Test Generator Val Container ---------===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef INCLUDED_HSAIL_TESTGEN_VAL_H
#define INCLUDED_HSAIL_TESTGEN_VAL_H

#include "HSAILTestGenEmulatorTypes.h"
#include "HSAILTestGenFpEmulator.h"
#include "Brig.h"
#include "HSAILUtilities.h"
#include "HSAILItems.h"

#include <assert.h>
#include <string>
#include <sstream>
#include <iomanip>

using std::string;
using std::ostringstream;
using std::setbase;
using std::setw;
using std::setfill;

using HSAIL_ASM::Directive;
using HSAIL_ASM::Inst;
using HSAIL_ASM::Operand;

using HSAIL_ASM::isPackedType;
using HSAIL_ASM::getBrigTypeNumBits;
using HSAIL_ASM::getPackedTypeDim;
using HSAIL_ASM::packedType2elementType;

#include <stdint.h>

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================
// Container for test values (used by LUA backend)

class ValVector;

class Val
{
    //==========================================================================
private:
    union
    {
        b128_t     num;    // Values for scalar operands. Unused bits must be zero!
        ValVector* vector; // values for vector operands
    };

    uint16_t type;          // set to BRIG_TYPE_NONE for empty values and for vector operands

    //==========================================================================
private:
    void setProps(unsigned t)
    {
        type = static_cast<uint16_t>(t);
        assert(t == type);
    }

private:
    void assign(u8_t  val) { setProps(BRIG_TYPE_U8);   num.set(val); }
    void assign(u16_t val) { setProps(BRIG_TYPE_U16);  num.set(val); }
    void assign(u32_t val) { setProps(BRIG_TYPE_U32);  num.set(val); }
    void assign(u64_t val) { setProps(BRIG_TYPE_U64);  num.set(val); }

    void assign(s8_t  val) { setProps(BRIG_TYPE_S8);   num.set(val); }
    void assign(s16_t val) { setProps(BRIG_TYPE_S16);  num.set(val); }
    void assign(s32_t val) { setProps(BRIG_TYPE_S32);  num.set(val); }
    void assign(s64_t val) { setProps(BRIG_TYPE_S64);  num.set(val); }

    void assign(f16_t val) { setProps(BRIG_TYPE_F16);  num.set(val); }
    void assign(f32_t val) { setProps(BRIG_TYPE_F32);  num.set(val); }
    void assign(f64_t val) { setProps(BRIG_TYPE_F64);  num.set(val); }

    void assign(b1_t   val){ setProps(BRIG_TYPE_B1);   num.set<b1_t>(val & 0x1); }
    void assign(b128_t val){ setProps(BRIG_TYPE_B128); num = val; }

    template<typename T> void assign(T val){ assign(val.get()); setProps(T::typeId); }

private:
    u8_t   get_u8()   const { assert(getType() == BRIG_TYPE_U8);   return num.get<u8_t>();  }
    u16_t  get_u16()  const { assert(getType() == BRIG_TYPE_U16);  return num.get<u16_t>(); }
    u32_t  get_u32()  const { assert(getType() == BRIG_TYPE_U32);  return num.get<u32_t>(); }
    u64_t  get_u64()  const { assert(getType() == BRIG_TYPE_U64);  return num.get<u64_t>(); }

    s8_t   get_s8()   const { assert(getType() == BRIG_TYPE_S8);   return num.get<s8_t>();  }
    s16_t  get_s16()  const { assert(getType() == BRIG_TYPE_S16);  return num.get<s16_t>(); }
    s32_t  get_s32()  const { assert(getType() == BRIG_TYPE_S32);  return num.get<s32_t>(); }
    s64_t  get_s64()  const { assert(getType() == BRIG_TYPE_S64);  return num.get<s64_t>(); }

    f16_t  get_f16()  const { assert(getType() == BRIG_TYPE_F16);  return num.get<f16_t>(); }
    f32_t  get_f32()  const { assert(getType() == BRIG_TYPE_F32);  return num.get<f32_t>(); }
    f64_t  get_f64()  const { assert(getType() == BRIG_TYPE_F64);  return num.get<f64_t>(); }

    b1_t   get_b1()   const { assert(getType() == BRIG_TYPE_B1);   return num.get<b1_t>(); }
    b8_t   get_b8()   const { assert(getType() == BRIG_TYPE_B8);   return num.get<b8_t>(); }
    b16_t  get_b16()  const { assert(getType() == BRIG_TYPE_B16);  return num.get<b16_t>(); }
    b32_t  get_b32()  const { assert(getType() == BRIG_TYPE_B32);  return num.get<b32_t>(); }
    b64_t  get_b64()  const { assert(getType() == BRIG_TYPE_B64);  return num.get<b64_t>(); }
    b128_t get_b128() const { assert(getType() == BRIG_TYPE_B128); return num; }

    //==========================================================================
private:
    void clean();
    void copy(const Val& val);
    u64_t getMask() const { assert(getType() != BRIG_TYPE_NONE); return (getSize() < 64)? (1ULL << getSize()) - 1 : -1; }

    //==========================================================================
public:
    Val()                       { setProps(BRIG_TYPE_NONE); num.clear(); }
    Val(unsigned t, u64_t val)  { setProps(t); assert(getType() != BRIG_TYPE_NONE); num.init(val & getMask()); }
    Val(unsigned t, b128_t val) { setProps(t); assert(getType() != BRIG_TYPE_NONE); num = val; }
    Val(unsigned dim, Val v0, Val v1, Val v2, Val v3);
    ~Val();

public:
    Val(const Val&);
    template<typename T> Val(T val) { num.clear(); assign(val); }

    Val& operator=(const Val& val);

    //==========================================================================
public:
    u8_t   u8()   const { return get_u8();  }
    u16_t  u16()  const { return get_u16(); }
    u32_t  u32()  const { return get_u32(); }
    u64_t  u64()  const { return get_u64(); }

    s8_t   s8()   const { return get_s8();  }
    s16_t  s16()  const { return get_s16(); }
    s32_t  s32()  const { return get_s32(); }
    s64_t  s64()  const { return get_s64(); }

    f16_t  f16()  const { return get_f16(); }
    f32_t  f32()  const { return get_f32(); }
    f64_t  f64()  const { return get_f64(); }

    b1_t   b1()   const { return get_b1();  }
    b8_t   b8()   const { return get_b8();  }
    b16_t  b16()  const { return get_b16(); }
    b32_t  b32()  const { return get_b32(); }
    b64_t  b64()  const { return get_b64(); }
    b128_t b128() const { return get_b128(); }

public:
    operator u8_t()   const { return get_u8();  }
    operator u16_t()  const { return get_u16(); }
    operator u32_t()  const { return get_u32(); }
    operator u64_t()  const { return get_u64(); }

    operator s8_t()   const { return get_s8();  }
    operator s16_t()  const { return get_s16(); }
    operator s32_t()  const { return get_s32(); }
    operator s64_t()  const { return get_s64(); }

    operator f16_t()  const { return get_f16(); }
    operator f32_t()  const { return get_f32(); }
    operator f64_t()  const { return get_f64(); }

    operator b128_t() const { return get_b128(); }

    template<typename T> operator T() const { assert(getType() == T::typeId); return num.get<T>(); }

    //==========================================================================
public:
    bool empty()               const { return getType() == BRIG_TYPE_NONE && !vector; }
    bool isVector()            const { return getType() == BRIG_TYPE_NONE && vector; }
    unsigned getDim()          const;
    unsigned getVecType()      const;

    Val operator[](unsigned i) const;

    unsigned getType()         const { return type; }
    unsigned getValType()      const { return isVector()? getVecType() : getType(); }
    unsigned getElementType()  const { assert(isPackedType(type)); return packedType2elementType(type); }
    unsigned getElementSize()  const { assert(isPackedType(type)); return getBrigTypeNumBits(getElementType()); }
    unsigned getSize()         const { return getBrigTypeNumBits(getType()); }
    bool     isF64()           const { return isFloat() && getSize() == 64; }
    bool     isF32()           const { return isFloat() && getSize() == 32; }
    bool     isF16()           const { return isFloat() && getSize() == 16; }

public:
    bool isInt()               const { return HSAIL_ASM::isIntType(getType()); }
    bool isSignedInt()         const { return HSAIL_ASM::isSignedType(getType()); }
    bool isUnsignedInt()       const { return HSAIL_ASM::isUnsignedType(getType()); }

    bool isFloat()             const { return HSAIL_ASM::isFloatType(getType()); }
    bool isRegularFloat()      const { return isFloat() && !isInf() && !isNan(); }
    bool isSpecialFloat()      const { return isFloat() && (isInf() || isNan()); }

    bool isPacked()            const { return HSAIL_ASM::isPackedType(getType()); }
    bool isPackedFloat()       const { return HSAIL_ASM::isFloatPackedType(getType()); }

public:
    u64_t getElement(unsigned idx) const;
    void setElement(unsigned idx, u64_t val);
    Val  getPackedElement(unsigned elementIdx, unsigned packing = BRIG_PACK_P, unsigned srcOperandIdx = 0) const;
    void setPackedElement(unsigned elementIdx, Val dst);

    //==========================================================================
private:
    template<class T>
    Val transform(T op) const // apply operation to regular/packed data
    {
        assert(!isVector());

        Val res = *this;
        unsigned dim = getPackedTypeDim(getType());

        if (dim == 0) return op(res);

        for (unsigned i = 0; i < dim; ++i)
        {
            res.setPackedElement(i, op(res.getPackedElement(i)));
        }

        return res;
    }

    //==========================================================================
    // Operations on scalar floating-point values
public:
    bool isPositive() const;
    bool isNegative() const;
    bool isZero() const;
    bool isPositiveZero() const;
    bool isNegativeZero() const;
    bool isInf() const;
    bool isPositiveInf() const;
    bool isNegativeInf() const;
    bool isNan() const;
    bool isQuietNan() const;
    bool isSignalingNan() const;
    bool isSubnormal() const;
    bool isPositiveSubnormal() const;
    bool isNegativeSubnormal() const;
    bool isRegularPositive() const;
    bool isRegularNegative() const;
    bool isNatural() const;

    u64_t getNormalizedFract(int delta = 0) const;
    Val copySign(Val v) const;
    Val ulp(int64_t delta) const;

public:
    Val getQuietNan()     const;
    Val getNegativeZero() const;
    Val getPositiveZero() const;
    Val getNegativeInf()  const;
    Val getPositiveInf()  const;

    //==========================================================================
    // Operations on scalar/packed floating-point values
public:
    Val normalize(bool discardNanSign) const;  // Clear NaN sign and payload
    Val ftz() const;                           // Force subnormals to 0

    //==========================================================================
public:
    u16_t getAsB16(unsigned idx = 0) const { return num.get<u16_t>(idx); }                              // Get value with zero-extension

    s32_t getAsS32()                 const { return static_cast<s32_t>(num.getElement(getType())); }    // Get value with zero/sign-extension
    u32_t getAsB32(unsigned idx = 0) const { return num.get<u32_t>(idx); }                              // Get value with zero-extension

    s64_t getAsS64()                 const { return num.getElement(getType()); }                        // Get value with zero/sign extension
    u64_t getAsB64(unsigned idx = 0) const { return num.get<u64_t>(idx); }                              // Get value with zero-extension

    Val randomize() const;
    bool eq(Val v) const;

    //==========================================================================
public:
    string dump() const;
    string luaStr(unsigned idx = 0) const;

private:
    string decDump() const;
    string hexDump() const;
    string dumpPacked() const;
    string nan2str() const;

    //==========================================================================
private:
    //static unsigned getTypeSize(unsigned t) { return HSAIL_ASM::getBrigTypeNumBits(t); }

};

//=============================================================================
//=============================================================================
//=============================================================================

} // namespace TESTGEN

// ============================================================================

#endif // INCLUDED_HSAIL_TESTGEN_VAL_H
