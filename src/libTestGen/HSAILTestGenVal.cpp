#include "HSAILTestGenVal.h"
#include "HSAILUtilities.h"

#include <iomanip>

using std::setprecision;

using HSAIL_ASM::isSignedType;
using HSAIL_ASM::isUnsignedType;
using HSAIL_ASM::isPackedType;
using HSAIL_ASM::isUnrPacking;
using HSAIL_ASM::packedType2baseType;
using HSAIL_ASM::packedType2elementType;
using HSAIL_ASM::getPackingControl;
using HSAIL_ASM::pack2str;

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================
// ValVector - a container for values stored in vector operands

class ValVector
{
private:
    Val val[4];         // Up to 4 values
    unsigned dim;       // Number of stores values
    unsigned refCount;  // Number of owners; used for deallocation

private:
    ValVector(const ValVector&); // non-copyable
    ValVector &operator=(const ValVector &); // not assignable

public:
    ValVector(unsigned dim, Val v0, Val v1, Val v2, Val v3)
    {
        assert(2 <= dim && dim <= 4);
        assert(!v0.empty() && !v1.empty());
        assert(!v0.isVector() && !v1.isVector() && !v2.isVector() && !v3.isVector());

        this->dim = dim;
        refCount = 0;

        val[0] = v0;
        val[1] = v1;
        val[2] = v2;
        val[3] = v3;
    }

public:
    unsigned getDim()          const { return dim; }
    unsigned getType()         const { return val[0].getType(); }
    Val operator[](unsigned i) const { assert(i < dim); return val[i]; }

public:
    unsigned lock()   {                       return ++refCount; }
    unsigned unlock() { assert(refCount > 0); return --refCount; }
};

//=============================================================================
//=============================================================================
//=============================================================================
// Val initialization, destruction and related operations

void Val::clean()
{
    if (isVector())
    {
        if (vector->unlock() == 0) delete vector;
        vector = 0;
    }
}

void Val::copy(const Val& val)
{
    setProps(val.type);

    if (val.isVector()) {
        vector = val.vector;
        vector->lock();
    } else {
        num = val.num;
    }
}

Val::~Val()
{
    clean();
}

Val::Val(unsigned dim, Val v0, Val v1, Val v2, Val v3)
{
    assert(2 <= dim && dim <= 4);

    setProps(Brig::BRIG_TYPE_NONE);
    num.clear();

    vector = new ValVector(dim, v0, v1, v2, v3);
    assert(vector);
    vector->lock();

    for (unsigned i = 1; i < dim; ++i)
    {
        assert((*vector)[0].getType() == (*vector)[i].getType());
    }
}

Val::Val(const Val& val)
{
    copy(val);
}

Val& Val::operator=(const Val& val)
{
    if (this != &val)
    {
        clean();
        copy(val);
    }
    return *this;
}

unsigned Val::getDim() const
{
    return isVector()? vector->getDim() : 1;
}

unsigned Val::getVecType() const
{
    return isVector()? vector->getType() : Brig::BRIG_TYPE_NONE;
}

Val Val::operator[](unsigned i) const
{
    if (isVector()) {
        assert(i < getDim());
        return (*vector)[i];
    } else {
        assert(i == 0);
        return *this;
    }
}

//=============================================================================
//=============================================================================
//=============================================================================
// Operations with packed values

u64_t Val::getElement(unsigned idx) const
{
    assert(isPacked());
    assert(idx < getPackedTypeDim(getType()));

    return num.getElement(packedType2elementType(getType()), idx);
}

void  Val::setElement(unsigned idx, u64_t val)
{
    assert(isPacked());
    assert(idx < getPackedTypeDim(getType()));

    num.setElement(val, packedType2elementType(getType()), idx);
}

Val Val::getPackedElement(unsigned elementIdx, unsigned packing /*=BRIG_PACK_P*/, unsigned srcOperandIdx /*=0*/) const
{
    assert(srcOperandIdx == 0 || srcOperandIdx == 1);
    assert(pack2str(packing));

    if (empty())
    {
        assert(srcOperandIdx == 1 && isUnrPacking(packing));
        return *this;
    }
    else if (isPacked())
    {
        assert(elementIdx < getPackedTypeDim(getType()));

        unsigned idx = (getPackingControl(srcOperandIdx, packing) == 'p')? elementIdx : 0;
        u64_t element = getElement(idx);
        return Val(packedType2baseType(getType()), element);
    }
    else // Special case for SHL/SHR
    {
        assert(getType() == Brig::BRIG_TYPE_U32);
        assert(packing == Brig::BRIG_PACK_PP);
        return *this; // shift all elements by the same amount
    }
}

void Val::setPackedElement(unsigned elementIdx, Val dst)
{
    assert(isPacked());
    assert(!dst.isPacked());
    assert(dst.getType() == packedType2baseType(getType()));
    assert(elementIdx < getPackedTypeDim(getType()));

    // It is assumed that dst does not need sign-extension
    setElement(elementIdx, dst.num.get<u64_t>());
}

//=============================================================================
//=============================================================================
//=============================================================================
// Operations on scalar floating-point values

#define GET_FLOAT_PROP(prop) \
    bool Val::prop() const   \
    {                        \
        return isFloat() && (isF16()? FloatProp16(num.get<u16_t>()).prop() : \
                             isF32()? FloatProp32(num.get<u32_t>()).prop() : \
                                      FloatProp64(num.get<u64_t>()).prop()); \
    }

GET_FLOAT_PROP(isPositive)
GET_FLOAT_PROP(isNegative)
GET_FLOAT_PROP(isZero)
GET_FLOAT_PROP(isPositiveZero)
GET_FLOAT_PROP(isNegativeZero)
GET_FLOAT_PROP(isInf)
GET_FLOAT_PROP(isPositiveInf)
GET_FLOAT_PROP(isNegativeInf)
GET_FLOAT_PROP(isNan)
GET_FLOAT_PROP(isQuietNan)
GET_FLOAT_PROP(isSignalingNan)
GET_FLOAT_PROP(isSubnormal)
GET_FLOAT_PROP(isPositiveSubnormal)
GET_FLOAT_PROP(isNegativeSubnormal)
GET_FLOAT_PROP(isRegularPositive)
GET_FLOAT_PROP(isRegularNegative)
GET_FLOAT_PROP(isNatural)

#define GET_FLOAT_NUMBER(prop) \
    Val Val::prop() const      \
    {                          \
        assert(isFloat());     \
        return isF16()? Val(getType(), FloatProp16::prop()) : \
               isF32()? Val(getType(), FloatProp32::prop()) : \
                        Val(getType(), FloatProp64::prop());  \
    }

GET_FLOAT_NUMBER(getQuietNan)
GET_FLOAT_NUMBER(getNegativeZero)
GET_FLOAT_NUMBER(getPositiveZero)
GET_FLOAT_NUMBER(getNegativeInf)
GET_FLOAT_NUMBER(getPositiveInf)

u64_t Val::getNormalizedFract(int delta /*=0*/) const
{
    assert(isFloat());

    return isF16()? FloatProp16(num.get<u16_t>()).getNormalizedFract(delta) :
           isF32()? FloatProp32(num.get<u32_t>()).getNormalizedFract(delta) :
                    FloatProp64(num.get<u64_t>()).getNormalizedFract(delta);
}

Val Val::copySign(Val v) const
{
    assert(isFloat());
    assert(getType() == v.getType());

    return isF16()? Val(getType(), FloatProp16(num.get<u16_t>()).copySign(v.num.get<u16_t>())) :
           isF32()? Val(getType(), FloatProp32(num.get<u32_t>()).copySign(v.num.get<u32_t>())) :
                    Val(getType(), FloatProp64(num.get<u64_t>()).copySign(v.num.get<u64_t>()));
}

Val Val::ulp(int64_t delta) const
{
    assert(isFloat());

    return isF16()? Val(getType(), FloatProp16(num.get<u16_t>()).ulp(delta)) :
           isF32()? Val(getType(), FloatProp32(num.get<u32_t>()).ulp(delta)) :
                    Val(getType(), FloatProp64(num.get<u64_t>()).ulp(delta));
}

//=============================================================================
//=============================================================================
//=============================================================================
// Operations on scalar/packed floating-point values

class op_normalize  // Clear NaN payload and sign
{
private:
    bool discardNanSign;

public:
    op_normalize(bool discardSign) : discardNanSign(discardSign) {}

    Val operator()(Val v)
    {
        if (!v.isFloat()) return v;
        if (v.isF16()) return Val(v.getType(), FloatProp16(v.getAsB16()).normalize(discardNanSign));
        if (v.isF32()) return Val(v.getType(), FloatProp32(v.getAsB32()).normalize(discardNanSign));
        if (v.isF64()) return Val(v.getType(), FloatProp64(v.getAsB64()).normalize(discardNanSign));
        assert(false); return Val();
    }
};

struct op_ftz   // Force subnormals to 0
{
    Val operator()(Val v)
    {
        if (v.isNegativeSubnormal()) return v.getNegativeZero();
        if (v.isPositiveSubnormal()) return v.getPositiveZero();
        return v;
    }
};

Val Val::normalize(bool discardNanSign) const 
{
    if (isVector()) return *this;
    return transform(op_normalize(discardNanSign)); 
}

Val Val::ftz() const { return transform(op_ftz()); }

//=============================================================================
//=============================================================================
//=============================================================================
// Randomization

struct op_s2q  // Replace Signaling NaNs with Quiet ones
{
    Val operator()(Val v)
    {
        if (v.isSignalingNan()) return v.getQuietNan();
        return v;
    }
};

Val Val::randomize() const
{
    assert(!empty() && !isVector());

    Val res = *this;
    unsigned dim = getSize() / 8; // NB: 0 for b1; it is ok

    for (unsigned i = 0; i < dim; ++i) res.num.setElement(rand(), Brig::BRIG_TYPE_U8, i);

    res = res.transform(op_s2q());  // Signaling NaNs are not supported, replace with quiet
    res = res.normalize(false);     // Clear NaN payload
    return res;
}

bool Val::eq(Val v) const
{
    assert(!empty());
    assert(!v.empty());

    if (isVector())
    {
        if (getDim() != v.getDim()) return false;
        for (unsigned i = 0; i < getDim(); ++i) if (!(*this)[i].eq(v[i])) return false;
        return true;
    }

    if (getType() != v.getType()) return false;
    if (isNan()) return v.isNan();
    
    return getAsB64(0) == v.getAsB64(0) && 
           getAsB64(1) == v.getAsB64(1);
}


//=============================================================================
//=============================================================================
//=============================================================================
// Val dumping

static unsigned getTextWidth(unsigned type)
{
    if (type == Brig::BRIG_TYPE_F16) return 10;
    if (type == Brig::BRIG_TYPE_F32) return 16;
    if (type == Brig::BRIG_TYPE_F64) return 24;

    switch (getBrigTypeNumBits(type))
    {
    case 8:  return 4;
    case 16: return 6;
    case 32: return 11;
    case 64: return 20;
    default: return 0;  // handled separately
    }
}

string Val::luaStr(unsigned idx /*=0*/) const
{
    using namespace Brig;

    assert(!isPackedFloat());
    assert(0 <= idx && idx <= 3);
    assert(!empty() && !isVector());

    ostringstream s;

    unsigned w = getTextWidth(getType());
    if (isFloat()) w += 2;
    s << setw(w);

    if (isSpecialFloat())
    {
        s << nan2str();
    }
    else
    {
        char buffer[32];

        switch (getType())
        {        
        case BRIG_TYPE_F16: s << "0x" << setbase(16) << setfill('0') << setw(4) << getAsB16(); break;

        case BRIG_TYPE_F32: sprintf(buffer,  "\"%.6A\"", f32()); s << string(buffer); break;
        case BRIG_TYPE_F64: sprintf(buffer, "\"%.13A\"", f64()); s << string(buffer); break;

        case BRIG_TYPE_S8:  s << static_cast<s32_t>(s8());     break;
        case BRIG_TYPE_S16: s << s16();                        break;
        case BRIG_TYPE_S32: s << s32();                        break;

        default: s << setw(getTextWidth(BRIG_TYPE_U32)) << getAsB32(idx); break;
        }
    }

    return s.str();
}

string Val::decDump() const
{
    using namespace Brig;
    assert(!empty() && !isVector());
    assert(getSize() != 128);
    assert(!isPacked());

    ostringstream s;

    s << setw(getTextWidth(getType()));

    if (isSpecialFloat())
    {
        s << nan2str();
    }
    else if (isNegativeZero()) // NB: with some compilers, '-0' is printed as '0'
    {
        s << "-0";
    }
    else
    {
        switch (getType())
        {
        case BRIG_TYPE_F16: s << setprecision(9)  << f16().f64(); break;
        case BRIG_TYPE_F32: s << setprecision(9)  << f32(); break;
        case BRIG_TYPE_F64: s << setprecision(17) << f64(); break;

        case BRIG_TYPE_S8:  s << static_cast<s32_t>(s8());  break;
        case BRIG_TYPE_S16: s << s16();                     break;
        case BRIG_TYPE_S32: s << s32();                     break;
        case BRIG_TYPE_S64: s << s64();                     break;

        default:            s << getAsB64();                break;
        }
    }

    return s.str();
}

string Val::hexDump() const
{
    using namespace Brig;
    assert(!empty() && !isVector());
    assert(getSize() != 128);
    assert(!isPacked());

    ostringstream s;
    s << "0x" << setbase(16) << setfill('0') << setw(getSize() / 4);

    switch (getType())
    {
    case BRIG_TYPE_S8:   s << (s8() & 0xFF);            break;
    case BRIG_TYPE_S16:  s << s16();                    break;
    case BRIG_TYPE_S32:  s << s32();                    break;
    case BRIG_TYPE_S64:  s << s64();                    break;
    default:             s << getAsB64();               break;
    }

    return s.str();
}

string Val::dump() const
{
    assert(!empty());

    if (isVector())
    {
        string sval;
        string hval;
        for (unsigned i = 0; i < vector->getDim(); ++i)
        {
            const char* pref = (i > 0)? ", " : "";
            sval += pref + (*vector)[i].decDump();
            hval += pref + (*vector)[i].hexDump();
        }
        return "(" + sval + ") [" + hval + "]";
    }
    else if (getType() == Brig::BRIG_TYPE_B128)
    {
        return b128().hexDump();
    }
    else if (isPackedType(getType()))
    {
        return dumpPacked();
    }
    else
    {
        return decDump() + " [" + hexDump() + "]";
    }
}

string Val::dumpPacked() const
{
    assert(!empty());
    assert(!isVector());

    ostringstream s;
    ostringstream h;

    unsigned etype = getElementType();
    unsigned dim   = getPackedTypeDim(getType());
    unsigned width = getBrigTypeNumBits(getType()) / dim;

    if      (isSignedType(etype))   s << "_s";
    else if (isUnsignedType(etype)) s << "_u";
    else                            s << "_f";

    s << width << "x" << dim << "(";
    h << setbase(16) << setfill('0') << "[";

    for (unsigned i = 0; i < dim; ++i)
    {
        if (i > 0) { s << ", "; h << ", "; }

        Val val(etype, getElement(dim - i - 1));

        s << val.decDump();
        h << val.hexDump();
    }

    s << ")";
    h << "]";

    return s.str() + " " + h.str();
}

string Val::nan2str() const
{
    assert(isSpecialFloat());
    assert(!isSignalingNan());

    return isNan()? "NAN" : isPositiveInf()? "INF" : "-INF";
}

//=============================================================================
//=============================================================================
//=============================================================================

} // namespace TESTGEN

