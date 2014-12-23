//===-- HSAILTestGenLuaBackend.h - LUA Backend ----------------------------===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef INCLUDED_HSAIL_TESTGEN_LUA_BACKEND_H
#define INCLUDED_HSAIL_TESTGEN_LUA_BACKEND_H

#include "HSAILTestGenBackendEml.h"

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================
// LUA script generation
//
// Generated script looks like this (details may vary):
// ----------------------------------------------------------
//      require "helpers"
//
//      local threads = 1
//      thread_group = T{0, 0, threads, 1}
//
//      src1 = new_global_array(UINT32, 2)                      // Declaration of an array for storing test results
//      array_set_all(src1, { 1 })                              // Initialization of array with test data
//      array_print(src1, "Array with test values for src1")    // Request test data dump
//      new_arg(REF, src1)                                      // Declaration of this array as kernel argument
//
//      dst = new_global_array(UINT32, 72)                      // Declaration of an array for storing test results
//      new_arg(REF, dst)                                       // Declaration of this array as kernel argument
//      dst_check = new_result_array_check(dst)                 // Request Emulator to compare data in this array with expected data
//      result_array_check_set(dst_check, 0, 0)                 // Declaration of expected data
//      result_array_check_print(dst_check, "Array with expected dst values")
//
// ----------------------------------------------------------
//
//==============================================================================
//==============================================================================
//==============================================================================

#define LUA_SEPARATOR ("--------------------------------------------------\n")
#define LUA_COMMENT   ("--- ")

//==============================================================================
//==============================================================================
//==============================================================================

class LuaBackend : public EmlBackend
{
public:
    LuaBackend()
    {
    }

    // Update test description with backend-specific data
    void registerTest(TestDesc& desc)
    {
        EmlBackend::registerTest(desc);
        desc.setScript(getLuaScript());
    }

private:
    //==========================================================================
    //

    struct CommentLua
    {
        string res;
        void operator()(string s) { res += LUA_COMMENT + s + "\n"; }
        string str() { return res; }
    };

    void genLuaDesc(ostringstream& os)
    {
        if (BrigSettings::commentsEnabled())
        {
            CommentLua commenter;
            emitTestDescriptionHeader(commenter, testName, testSample, testGroup->getGroupSize());
            emitTestDescriptionBody(commenter, testSample, *testGroup, testDataMap);
            os << LUA_SEPARATOR << commenter.str() << LUA_SEPARATOR << "\n";
        }
    }

    void genLuaHeader(ostringstream& os)
    {
        os << "require \"helpers\"\n\n"
           << "local threads = " << testGroup->getGroupsNum() << "\n"
           << "thread_group = T{0, 0, threads, 1}\n\n";
    }

    bool isSignedLuaType(unsigned type) { return isSignedType(type) && getBrigTypeNumBits(type) <= 32; }

    void defLuaArray(ostringstream& os, string name, unsigned type, unsigned dim)
    {
        assert(type != BRIG_TYPE_F16);

        // Subword values are represented as 32-bit values
        // s64/u64 values are represented as 2 32-bit values because of LUA limitations
        unsigned    typeSize  = getBrigTypeNumBits(type);
        const char* arrayType = isFloatType(type)? (typeSize == 64? "DOUBLE" : "FLOAT")
                              : isSignedLuaType(type) ?             "INT32"  : "UINT32";
        unsigned    arraySize = isFloatType(type)? 1 : (typeSize == 128? 4 : typeSize == 64? 2 : 1);

        os << name << " = new_global_array(" << arrayType  << ", " << arraySize * testGroup->getFlatSize() * dim << ")\n";
    }

    void initLuaArray(ostringstream& os, string name, unsigned operandIdx)
    {
        assert(testGroup);

        LuaSrcPrinter printer("                  ");

        os << "array_set_all(" << name << ", ";

        if (testGroup->getFlatSize() == 1)
        {
            TestData& data = testGroup->getData(0);
            Val val = data.src[operandIdx];
            val2lua(printer, val);
            os << "{ " << printer() << " }) -- " << val.dump() << "\n";
        }
        else
        {
            os << "\n              {\n";
            for (unsigned flatIdx = 0; flatIdx < testGroup->getFlatSize(); ++flatIdx)
            {
                TestData& data = testGroup->getData(flatIdx);
                Val val = data.src[operandIdx];
                printer.nextValue(val);
                val2lua(printer, val);
            }
            os << printer()
               << "              }\n"
               << ")\n";
        }
    }

    void defLuaKernelArg(ostringstream& os, string name)
    {
        os << "new_arg(REF, " << name << ")\n";
    }

    void defLuaCheckRules(ostringstream& os, string checkName, string arrayName, unsigned type)
    {
        assert(type != BRIG_TYPE_F16);
        double precision = getPrecision(testSample);

        os << checkName << " = new_result_array_check(" << arrayName;

        if (isFloatType(type))
        {
            os << ", " << precision << ", ";
            os << ((precision < 1)? "CM_RELATIVE" : "CM_ULPS");
        }

        os << ")\n";
    }

    void printSrcLuaArray(ostringstream& os, string name)
    {
        os << "array_print(" << name << ", \"Array with test values for " << name << "\")\n";
    }

    void printResLuaArray(ostringstream& os, string checkName, string valKind)
    {
        os << "result_array_check_print(" << checkName << ", \"Array with expected " << valKind << " values\")\n";
    }

    void defLuaChecks(ostringstream& os, string checkName, bool isDst)
    {
        assert(testGroup);

        LuaDstPrinter printer(checkName);

        for (unsigned flatIdx = 0; flatIdx < testGroup->getFlatSize(); ++flatIdx)
        {
            TestData& data = testGroup->getData(flatIdx);
            Val val = isDst? data.dst : data.mem;
            printer.nextValue(val);
            val2lua(printer, val);
        }

        os << printer();
    }

    void defSrcLuaArray(ostringstream& os, unsigned operandIdx)
    {
        assert(testGroup);

        TestData& data = testGroup->getData(0);
        unsigned type = data.src[operandIdx].getValType();
        unsigned dim  = data.src[operandIdx].getDim();

        if (HSAIL_ASM::isFloatPackedType(type))
        {
            dim *= getPackedTypeDim(type);
            type = packedType2elementType(type);
        }

        string name = getSrcArrayName(operandIdx);
        defLuaArray(os, name, type, dim);
        initLuaArray(os, name, operandIdx);
        printSrcLuaArray(os, name);
        defLuaKernelArg(os, name);
        os << "\n";
    }

    void defDstLuaArray(ostringstream& os)
    {
        assert(testGroup);
        TestData& data = testGroup->getData(0);
        defResultLuaArray(os, "dst", "dst_check", getDstArrayName(), data.dst.getValType(), data.dst.getDim(), true);
    }

    void defMemLuaArray(ostringstream& os)
    {
        assert(testGroup);
        TestData& data = testGroup->getData(0);
        defResultLuaArray(os, "mem", "mem_check", getMemArrayName(), data.mem.getValType(), data.mem.getDim(), false);
    }

    void defResultLuaArray(ostringstream& os, string valKind, string checkName, string arrayName, unsigned type, unsigned dim, bool isDst)
    {
        if (HSAIL_ASM::isFloatPackedType(type))
        {
            dim *= getPackedTypeDim(type);
            type = packedType2elementType(type);
        }

        defLuaArray(os, arrayName, type, dim);
        defLuaKernelArg(os, arrayName);
        defLuaCheckRules(os, checkName, arrayName, type);
        defLuaChecks(os, checkName, isDst);
        printResLuaArray(os, checkName, valKind);
        os << "\n";
    }

    string getLuaScript()
    {
        assert(testGroup);

        ostringstream os;

        genLuaDesc(os);
        genLuaHeader(os);

        //F Set LUA wavesize to the value specified by command line option (currently hardcoded as 64)

        for (unsigned i = testDataMap.getFirstSrcArgIdx(); i <= testDataMap.getLastSrcArgIdx(); ++i)
        {
            defSrcLuaArray(os, i);
        }

        if (testDataMap.getDstArgsNum() == 1)
        {
            defDstLuaArray(os);
        }

        if (testDataMap.getMemArgsNum() == 1)
        {
            defMemLuaArray(os);
        }

        return os.str();
    }

    //==========================================================================
    // LUA script generation helpers
private:

    // ------------------------------------------------------------------------
    // Printer for src values

    struct LuaSrcPrinter
    {
    private:
        ostringstream s;
        string pref;
        Val value;

    public:
        LuaSrcPrinter(string p) : pref(p) {}

    public:
        void nextValue(Val v)  { flush(); value = v; s << pref; }
        template<typename T>
        void operator()(T val) { s << val << ", "; }
        void flush()           { if (!value.empty()) s << " -- " << value.dump() << "\n"; }
        string operator()()    { flush(); return s.str(); }
    };

    // ------------------------------------------------------------------------
    // Printer for dst values

    struct LuaDstPrinter
    {
    private:
        ostringstream s;
        string checkName;
        unsigned slot;
        Val value;

    public:
        LuaDstPrinter(string name, unsigned firstSlot = 0) : checkName(name), slot(firstSlot) {}

    public:
        void nextValue(Val v)  { value = v; }
        template<typename T>
        void operator()(T val) {
            s << "result_array_check_set(" << checkName << ", " << setw(3) << slot++ << ", " << val << ")";
            if (!value.empty()) s << " -- " << value.dump();
            value = Val();
            s << "\n";
        }
        string operator()()    { return s.str(); }
    };

    // ------------------------------------------------------------------------

    template<class T>
    void val2lua(T& printer, Val v)
    {
        assert(!v.empty());

        for (unsigned i = 0; i < v.getDim(); ++i)
        {
            assert(!v[i].isVector());

            Val val = v[i];

            if (val.isFloat())
            {
                printer(val.luaStr());
            }
            else if (val.isPackedFloat())
            {
                unsigned dim = getPackedTypeDim(val.getType());
                for (unsigned i = 0; i < dim; ++i) printer(val.getPackedElement(i).luaStr());
            }
            else if (val.getSize() <= 32)
            {
                printer(val.luaStr());
            }
            else if (val.getSize() == 64)
            {
                printer(val.luaStr(0));
                printer(val.luaStr(1));
            }
            else // 128-bit values
            {
                assert(val.getSize() == 128);

                printer(val.luaStr(0));
                printer(val.luaStr(1));
                printer(val.luaStr(2));
                printer(val.luaStr(3));
            }
        }
    }

    // ------------------------------------------------------------------------

}; // class LuaBackend

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_LUA_BACKEND_H
