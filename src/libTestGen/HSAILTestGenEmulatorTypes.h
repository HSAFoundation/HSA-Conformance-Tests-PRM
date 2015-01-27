//===-- HSAILTestGenEmulatorTypes.h - HSAIL Test Generator  ===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef INCLUDED_HSAIL_TESTGEN_EMULATOR_TYPES_H
#define INCLUDED_HSAIL_TESTGEN_EMULATOR_TYPES_H

#include "Brig.h"
#include "HSAILUtilities.h"
#include "HSAILTestGenUtilities.h"
#include "HSAILItems.h"

#include <assert.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdint.h>

using std::string;
using std::ostringstream;
using std::setbase;
using std::setw;
using std::setfill;

using HSAIL_ASM::getBrigTypeNumBits;

namespace TESTGEN {

// ============================================================================
// ============================================================================
// ============================================================================
// HSAIL s/u/f Types

typedef int8_t   s8_t;
typedef int16_t  s16_t;
typedef int32_t  s32_t;
typedef int64_t  s64_t;

typedef uint8_t  u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

typedef float    f32_t;
typedef double   f64_t;

class f16_t;

// ============================================================================
// ============================================================================
// ============================================================================
// HSAIL Type Helpers

class b128_t
{
private:
    u64_t  data[2];

public:
    void clear() { set<u64_t>(0, 0); set<u64_t>(0, 1); }
    template<typename T> void init(T val) { clear(); set(val); }

    template<typename T>
    T get(unsigned idx = 0) const
    {
        assert(idx < 16 / sizeof(T));
        return reinterpret_cast<const T*>(this)[idx];
    }

    template<typename T>
    void set(T val, unsigned idx = 0)
    {
        assert(idx < 16 / sizeof(T));
        reinterpret_cast<T*>(this)[idx] = val;
    }

public:
    // Get value with sign-extension
    u64_t getElement(unsigned type, unsigned idx = 0) const
    {
        using namespace Brig;

        assert(idx < 128 / static_cast<unsigned>(getBrigTypeNumBits(type)));

        switch(type)
        {
        case BRIG_TYPE_S8:  return static_cast<s64_t>(get<s8_t> (idx));
        case BRIG_TYPE_S16: return static_cast<s64_t>(get<s16_t>(idx));
        case BRIG_TYPE_S32: return static_cast<s64_t>(get<s32_t>(idx));
        case BRIG_TYPE_S64: return static_cast<s64_t>(get<s64_t>(idx));

        case BRIG_TYPE_U8:  return get<u8_t>(idx);
        case BRIG_TYPE_U16: return get<u16_t>(idx);
        case BRIG_TYPE_U32: return get<u32_t>(idx);
        case BRIG_TYPE_U64: return get<u64_t>(idx);

        case BRIG_TYPE_F16: return get<u16_t>(idx);
        case BRIG_TYPE_F32: return get<u32_t>(idx);
        case BRIG_TYPE_F64: return get<u64_t>(idx);

        default:
            assert(false);
            return 0;
        }
    }

    void setElement(u64_t val, unsigned type, unsigned idx = 0)
    {
        using namespace Brig;

        assert(idx < 128 / static_cast<unsigned>(getBrigTypeNumBits(type)));

        switch(type)
        {
        case BRIG_TYPE_S8:  set(static_cast<s8_t> (val), idx); return;
        case BRIG_TYPE_S16: set(static_cast<s16_t>(val), idx); return;
        case BRIG_TYPE_S32: set(static_cast<s32_t>(val), idx); return;
        case BRIG_TYPE_S64: set(static_cast<s64_t>(val), idx); return;

        case BRIG_TYPE_U8:  set(static_cast<u8_t> (val), idx); return;
        case BRIG_TYPE_U16: set(static_cast<u16_t>(val), idx); return;
        case BRIG_TYPE_U32: set(static_cast<u32_t>(val), idx); return;
        case BRIG_TYPE_U64: set(static_cast<u64_t>(val), idx); return;

        case BRIG_TYPE_F16: set(static_cast<u16_t>(val), idx); return;
        case BRIG_TYPE_F32: set(static_cast<u32_t>(val), idx); return;
        case BRIG_TYPE_F64: set(static_cast<u64_t>(val), idx); return;

        default:
            assert(false);
            return;
        }
    }

public:
    bool operator==(const b128_t& rhs) const { return (get<u64_t>(0) == rhs.get<u64_t>(0)) && (get<u64_t>(1) == rhs.get<u64_t>(1)); }

    string hexDump() const
    {
        ostringstream s;
        s << "_b128(";
        s << setbase(16) << setfill('0');
        s << "0x" << setw(8) << get<u32_t>(3) << ",";
        s << "0x" << setw(8) << get<u32_t>(2) << ",";
        s << "0x" << setw(8) << get<u32_t>(1) << ",";
        s << "0x" << setw(8) << get<u32_t>(0) << ")";
        return s.str();
    }
};

template <typename OS> inline OS& operator <<(OS& os, const b128_t& v) { os << v.hexDump(); return os; }

// ============================================================================
// ============================================================================
// ============================================================================
// HSAIL Type Class

template<typename T, typename E, unsigned HsailTypeId>
struct HsailType
{
public:
    typedef T BaseType;
    typedef E ElemType;
    static const unsigned typeId = HsailTypeId;

private:
    BaseType val;

public:
    HsailType() {}
    HsailType(const BaseType v) : val(v) {}
    bool operator==(const HsailType& rhs) const { return val == rhs.val; }
    operator BaseType() const { return val; }
    BaseType get() const { return val; }
};

// ============================================================================
// ============================================================================
// ============================================================================
// HSAIL Bit Types

typedef HsailType<u8_t,  u8_t,  Brig::BRIG_TYPE_B1>  b1_t;
typedef HsailType<u8_t,  u8_t,  Brig::BRIG_TYPE_B8>  b8_t;
typedef HsailType<u16_t, u16_t, Brig::BRIG_TYPE_B16> b16_t;
typedef HsailType<u32_t, u32_t, Brig::BRIG_TYPE_B32> b32_t;
typedef HsailType<u64_t, u64_t, Brig::BRIG_TYPE_B64> b64_t;

//=============================================================================
//=============================================================================
//=============================================================================
// HSAIL Packed Types

typedef HsailType<u32_t,  u8_t,  Brig::BRIG_TYPE_U8X4 > u8x4_t;
typedef HsailType<u32_t,  u16_t, Brig::BRIG_TYPE_U16X2> u16x2_t;
typedef HsailType<u64_t,  u8_t,  Brig::BRIG_TYPE_U8X8 > u8x8_t;
typedef HsailType<u64_t,  u16_t, Brig::BRIG_TYPE_U16X4> u16x4_t;
typedef HsailType<u64_t,  u32_t, Brig::BRIG_TYPE_U32X2> u32x2_t;
typedef HsailType<b128_t, u8_t,  Brig::BRIG_TYPE_U8X16> u8x16_t;
typedef HsailType<b128_t, u16_t, Brig::BRIG_TYPE_U16X8> u16x8_t;
typedef HsailType<b128_t, u32_t, Brig::BRIG_TYPE_U32X4> u32x4_t;
typedef HsailType<b128_t, u64_t, Brig::BRIG_TYPE_U64X2> u64x2_t;

typedef HsailType<u32_t,  s8_t,  Brig::BRIG_TYPE_S8X4 > s8x4_t;
typedef HsailType<u32_t,  s16_t, Brig::BRIG_TYPE_S16X2> s16x2_t;
typedef HsailType<u64_t,  s8_t,  Brig::BRIG_TYPE_S8X8 > s8x8_t;
typedef HsailType<u64_t,  s16_t, Brig::BRIG_TYPE_S16X4> s16x4_t;
typedef HsailType<u64_t,  s32_t, Brig::BRIG_TYPE_S32X2> s32x2_t;
typedef HsailType<b128_t, s8_t,  Brig::BRIG_TYPE_S8X16> s8x16_t;
typedef HsailType<b128_t, s16_t, Brig::BRIG_TYPE_S16X8> s16x8_t;
typedef HsailType<b128_t, s32_t, Brig::BRIG_TYPE_S32X4> s32x4_t;
typedef HsailType<b128_t, s64_t, Brig::BRIG_TYPE_S64X2> s64x2_t;

typedef HsailType<u32_t,  f16_t, Brig::BRIG_TYPE_F16X2> f16x2_t;
typedef HsailType<u64_t,  f16_t, Brig::BRIG_TYPE_F16X4> f16x4_t;
typedef HsailType<u64_t,  f32_t, Brig::BRIG_TYPE_F32X2> f32x2_t;
typedef HsailType<b128_t, f16_t, Brig::BRIG_TYPE_F16X8> f16x8_t;
typedef HsailType<b128_t, f32_t, Brig::BRIG_TYPE_F32X4> f32x4_t;
typedef HsailType<b128_t, f64_t, Brig::BRIG_TYPE_F64X2> f64x2_t;

// ============================================================================
// ============================================================================
// ============================================================================
// Packing routines for packed data

inline b128_t b128(u64_t lo, u64_t hi) { b128_t res; res.set(lo, 0); res.set(hi, 1); return res; }

template<typename T, typename E>
inline T pack(E x0,        E x1,        E x2 = E(0),  E x3 = E(0),  E x4 = E(0),  E x5 = E(0),  E x6 = E(0),  E x7 = E(0),
              E x8 = E(0), E x9 = E(0), E x10 = E(0), E x11 = E(0), E x12 = E(0), E x13 = E(0), E x14 = E(0), E x15 = E(0))
{
    b128_t res;
    unsigned dim = sizeof(T) / sizeof(E);
    E data[16] = {x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15};

    for (unsigned i = 0; i < dim; ++i) res.set(data[i], dim - i - 1);
    for (unsigned i = dim; i < 16; ++i) { assert(data[i] == (E)0); }

    return res.get<T>();
}

template<typename T, typename E>
inline T fillBits(E x, unsigned mask = 0xFFFFFFFF)
{
    b128_t res;
    unsigned dim = sizeof(T) / sizeof(E);

    for (unsigned i = 0; i < dim; ++i, mask >>= 1) res.set(static_cast<E>(x * (i + 1) * (mask & 0x1)), i);

    return res.get<T>();
}

template<typename T>
inline T fillBits_f16(f16_t x, unsigned mask = 0xFFFFFFFF)
{
    b128_t res;
    unsigned dim = sizeof(T) / sizeof(f16_t);

    for (unsigned i = 0; i < dim; ++i, mask >>= 1) res.set(f16_t(x.f64() * (i + 1) * (mask & 0x1)), i);

    return res.get<T>();
}

#define u8x4  pack<u8x4_t,  u8_t>
#define u8x8  pack<u8x8_t,  u8_t>
#define u8x16 pack<u8x16_t, u8_t>
#define u16x2 pack<u16x2_t, u16_t>
#define u16x4 pack<u16x4_t, u16_t>
#define u16x8 pack<u16x8_t, u16_t>
#define u32x2 pack<u32x2_t, u32_t>
#define u32x4 pack<u32x4_t, u32_t>
#define u64x2 pack<u64x2_t, u64_t>

#define s8x4  pack<s8x4_t,  s8_t>
#define s8x8  pack<s8x8_t,  s8_t>
#define s8x16 pack<s8x16_t, s8_t>
#define s16x2 pack<s16x2_t, s16_t>
#define s16x4 pack<s16x4_t, s16_t>
#define s16x8 pack<s16x8_t, s16_t>
#define s32x2 pack<s32x2_t, s32_t>
#define s32x4 pack<s32x4_t, s32_t>
#define s64x2 pack<s64x2_t, s64_t>

#define f16x2 pack<f16x2_t, f16_t>
#define f16x4 pack<f16x4_t, f16_t>
#define f16x8 pack<f16x8_t, f16_t>
#define f32x2 pack<f32x2_t, f32_t>
#define f32x4 pack<f32x4_t, f32_t>
#define f64x2 pack<f64x2_t, f64_t>


#define fill_u8x4  fillBits<u8x4_t,  u8_t>
#define fill_u8x8  fillBits<u8x8_t,  u8_t>
#define fill_u8x16 fillBits<u8x16_t, u8_t>
#define fill_u16x2 fillBits<u16x2_t, u16_t>
#define fill_u16x4 fillBits<u16x4_t, u16_t>
#define fill_u16x8 fillBits<u16x8_t, u16_t>
#define fill_u32x2 fillBits<u32x2_t, u32_t>
#define fill_u32x4 fillBits<u32x4_t, u32_t>
#define fill_u64x2 fillBits<u64x2_t, u64_t>

#define fill_s8x4  fillBits<s8x4_t,  s8_t>
#define fill_s8x8  fillBits<s8x8_t,  s8_t>
#define fill_s8x16 fillBits<s8x16_t, s8_t>
#define fill_s16x2 fillBits<s16x2_t, s16_t>
#define fill_s16x4 fillBits<s16x4_t, s16_t>
#define fill_s16x8 fillBits<s16x8_t, s16_t>
#define fill_s32x2 fillBits<s32x2_t, s32_t>
#define fill_s32x4 fillBits<s32x4_t, s32_t>
#define fill_s64x2 fillBits<s64x2_t, s64_t>

#define fill_f16x2 fillBits_f16<f16x2_t>
#define fill_f16x4 fillBits_f16<f16x4_t>
#define fill_f16x8 fillBits_f16<f16x8_t>
#define fill_f32x2 fillBits<f32x2_t, f32_t>
#define fill_f32x4 fillBits<f32x4_t, f32_t>
#define fill_f64x2 fillBits<f64x2_t, f64_t>

//=============================================================================
//=============================================================================
//=============================================================================
//

#define HEX2F32(x) (*reinterpret_cast<const f32_t*>(&x))
#define HEX2F64(x) (*reinterpret_cast<const f64_t*>(&x))

#define F32_2U(x) (*reinterpret_cast<const u32_t*>(&x))
#define F64_2U(x) (*reinterpret_cast<const u64_t*>(&x))

//=============================================================================
//=============================================================================
//=============================================================================
// Properties of integer types

template<typename T>
struct NumProps
{
    static unsigned width()     { return sizeof(T) * 8; }
    static unsigned shiftMask() { return width() - 1; }
};

template<typename T> bool isSigned(T val)   { return ((T)-1 < (T)0); }

// Compute number of bits required to represent 'range' values
inline u32_t range2width(unsigned range) // log2
{
    switch(range)
    {
    case 2:     return 1;
    case 4:     return 2;
    case 8:     return 3;
    case 16:    return 4;
    case 32:    return 5;
    case 64:    return 6;
    default:
        assert(false);
        return 0;
    }
}

inline u64_t getSignMask(unsigned width)  { return 1ULL << (width - 1); }
inline u64_t getWidthMask(unsigned width) { return ((width == 64)? 0 : (1ULL << width)) - 1ULL; }
inline u64_t getRangeMask(unsigned range) { return getWidthMask(range2width(range)); }

//==============================================================================
//==============================================================================
//==============================================================================
// Type Boundaries

f16_t getTypeBoundary_f16(unsigned type, bool isLo);
f32_t getTypeBoundary_f32(unsigned type, bool isLo);
f64_t getTypeBoundary_f64(unsigned type, bool isLo);

template<typename T>
static T getTypeBoundary(unsigned type, bool isLo)
{
    assert(isIntType(type));

    if (sizeof(T) == 2) return (T)getTypeBoundary_f16(type, isLo);
    if (sizeof(T) == 4) return (T)getTypeBoundary_f32(type, isLo);
    if (sizeof(T) == 8) return (T)getTypeBoundary_f64(type, isLo);

    assert(false);
    return (T)0.0f;
}

u64_t getIntBoundary(unsigned type, bool low);

//=============================================================================
//=============================================================================
//=============================================================================
// Helpers for generation of tests for rounding modes

unsigned getRoundingTestsNum(unsigned dstType);

const f16_t*  getF16RoundingTestsData(unsigned dstType, AluMod aluMod);
const float*  getF32RoundingTestsData(unsigned dstType, AluMod aluMod);
const double* getF64RoundingTestsData(unsigned dstType, AluMod aluMod);

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_EMULATOR_TYPES_H