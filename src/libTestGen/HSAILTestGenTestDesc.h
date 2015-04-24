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

#ifndef INCLUDED_HSAIL_TESTGEN_TEST_DESC_H
#define INCLUDED_HSAIL_TESTGEN_TEST_DESC_H

#include "HSAILBrigContainer.h"
#include "HSAILTestGenVal.h"
#include "HSAILTestGenUtilities.h"
#include "HSAILItems.h"

#include <vector>
#include <algorithm>

using HSAIL_ASM::BrigContainer;
using HSAIL_ASM::Inst;
using std::string;
using std::vector;

namespace TESTGEN {

class TestGroupArray;
class TestDataMap;

//==============================================================================
//==============================================================================
//==============================================================================

string dumpInst(Inst inst);
string getOperandKind(Inst inst, unsigned operandIdx);

//==============================================================================
//==============================================================================
//==============================================================================
// Description of a test group

class TestDesc
{
    //==========================================================================
private:
    BrigContainer*  container;  // Container with BRIG code
    TestGroupArray* testData;   // Test group data
    TestDataMap*    map;        // Mapping of test data to instruction arguments
    Inst            testInst;   // The instruction being tested
    string          script;     // LUA script for this test group

    //==========================================================================
public:
    TestDesc() { clean(); }

    //==========================================================================
private:
    TestDesc(const TestDesc&);                    // non-copyable
    const TestDesc &operator=(const TestDesc &);  // not assignable

    //==========================================================================
    //
public:
    void clean()
    {
        map       = 0;
        container = 0;
        testData  = 0;
        testInst  = Inst();
        script.clear();
    }

    void setContainer(BrigContainer* c) { container = c;   }
    void setData(TestGroupArray* data)  { testData = data; }
    void setMap(TestDataMap* data)      { map = data; }
    void setInst(Inst inst)             { testInst = inst; }
    void setScript(string s)            { script = s;      }

    BrigContainer*  getContainer()  const { return container; }
    TestGroupArray* getData()       const { return testData;  }
    TestDataMap*    getMap()        const { return map;  }
    Inst            getInst()       const { return testInst;  }
    string          getScript()     const { return script;    }

    unsigned        getOpcode()           { assert(testInst); return testInst.opcode(); }

    //==========================================================================
};

//==============================================================================
//==============================================================================
//==============================================================================
// Mapping of test data to instruction arguments

class TestDataMap
{
    //==========================================================================
private:
    static const unsigned MAX_SRC_OPRND_NUM = 5;
    static const unsigned MAX_DST_OPRND_NUM = 1;
    static const unsigned MAX_MEM_OPRND_NUM = 1;

    //==========================================================================
private:
    unsigned firstSrcArgIdx;
    unsigned srcArgsNum;
    unsigned dstArgsNum;
    unsigned memArgsNum;
    double   precision;

    //==========================================================================
public:
    void setupTestArgs(unsigned first, unsigned src, unsigned dst, unsigned mem, double prc)
    {
        assert(first <  MAX_SRC_OPRND_NUM);
        assert(src   <= MAX_SRC_OPRND_NUM);
        assert(dst   <= MAX_DST_OPRND_NUM);
        assert(mem   <= MAX_MEM_OPRND_NUM);
    
        firstSrcArgIdx = first;
        srcArgsNum = src;
        dstArgsNum = dst;
        memArgsNum = mem;
        precision = prc; 
    }
    
    unsigned getArgsNum()                const { return srcArgsNum + dstArgsNum + memArgsNum; }
    unsigned getSrcArgsNum()             const { return srcArgsNum; }
    unsigned getDstArgsNum()             const { return dstArgsNum; }
    unsigned getMemArgsNum()             const { return memArgsNum; }
    unsigned getDstArgIdx()              const { return 0; }
    unsigned getFirstSrcArgIdx()         const { return firstSrcArgIdx; }
    unsigned getLastSrcArgIdx()          const { return firstSrcArgIdx + srcArgsNum - 1; }
    double   getPrecision()              const { return precision; }
};

//==============================================================================
//==============================================================================
//==============================================================================
// Container for storing test values

class TestData
{
    //==========================================================================
public:
    static const unsigned MAX_SRC_OPRND_NUM = 5;        // Max number of source operands

    //==========================================================================
public:
    Val src[MAX_SRC_OPRND_NUM];                      // Values of source operands
    Val dst;                                         // Expected dst value (empty value if none)
    Val mem;                                         // Expected value in memory (empty value if none)

    //==========================================================================
public:
    void clear()
    {
        for (unsigned i = 0; i < MAX_SRC_OPRND_NUM; ++i) src[i] = Val();
        dst = Val();
        mem = Val();
    }

    bool isEmpty() const 
    { 
        if (!src[1].empty()) return false; // optimization

        for (unsigned i = 0; i < MAX_SRC_OPRND_NUM; ++i) 
        {
            if (!src[i].empty()) return false;
        }
        return dst.empty() && mem.empty();
    }
};

//==============================================================================
//==============================================================================
//==============================================================================
// Container for one group of test data.
//
// Note that some values in the group may be empty. Empty values represent 
// combinations of test data not valid for the instruction being tested.
// These values are required for proper grouping (see TestGroupArray);
// they are removed after grouping.

class TestGroupSample
{
    friend class TestGroupArray;

protected:
    vector<TestData> data;

private:
    TestGroupSample(const TestGroupSample&); // non-copyable
    const TestGroupSample &operator=(const TestGroupSample &); // not assignable

public:
    TestGroupSample()                           {}
    void reset()                                { data.clear(); }
                                     
public:                              
    void append(TestData& td)                   { data.push_back(td); }
    TestData& getData(unsigned flatIdx)         { assert(flatIdx < getFlatSize()); return data.at(flatIdx); }
    unsigned  getFlatSize()               const { return static_cast<unsigned>(data.size()); }

private:
    bool isEmpty()                        const { return data.empty() || getActualSize() == 0; }

public:
    // size without empty values
    unsigned  getActualSize() const       
    { 
        unsigned sz = 0; 
        for (unsigned i = 0; i < data.size(); ++i)
        {
            if (!data[i].isEmpty()) ++sz;
        }
        return sz;
    }
};

//==============================================================================
//==============================================================================
//==============================================================================
// Container for test data in one test group
//
// Test group may include several groups of test data but these groups must
// be compatible with each other, i.e. must have empty elements in the same 
// positions.

class TestGroupArray : public TestGroupSample
{
private:
    vector<TestData> signature;                                 // A group of test data indicating empty elements
                                                                // This is not a part of test data, it is used only
                                                                // to check group elements for compatibility.

    unsigned groupSize;                                         // Group size (not including rejected elements)
    unsigned maxGroupsNum;                                      // Max number of groups in the bundle
    unsigned maxTestsNum;                                       // Max number of tests in the bundle

public:
    TestGroupArray(unsigned maxGroups, unsigned maxTests)
    { 
        assert(maxGroups > 0); 
        assert(maxTests > 0); 

        groupSize = 0;
        maxGroupsNum = maxGroups;
        maxTestsNum = maxTests;
    }

private:
    TestGroupArray(const TestGroupArray&); // non-copyable
    const TestGroupArray &operator=(const TestGroupArray &); // not assignable

public:
    TestData& getData(unsigned flatIdx)         { return TestGroupSample::getData(flatIdx); }
    TestData& getData(unsigned grpIdx, 
                      unsigned tstIdx)          { return TestGroupSample::getData(grpIdx * groupSize + tstIdx); }

    unsigned getGroupsNum()               const { assert(getFlatSize() % groupSize == 0); return getFlatSize() / groupSize; }
    unsigned getGroupSize()               const { assert(groupSize != 0); return groupSize; }

    unsigned getTestIdx(unsigned flatIdx) const { assert(flatIdx < getFlatSize()); return flatIdx % getGroupSize(); }
    unsigned getGroupIdx(unsigned flatIdx)const { assert(flatIdx < getFlatSize()); return flatIdx / getGroupSize(); }

private:
    bool isEmpty()                        const { return signature.empty() && data.empty(); }
    bool isFull()                         const { return getGroupsNum() == maxGroupsNum; }
    void append(TestData& td)                   { assert(false); TestGroupSample::append(td); }

public:
    bool append(TestGroupSample& sample)
    {
        assert(!sample.isEmpty());

        if (isEmpty()) return initGroup(sample);
        if (equalSignatures(sample)) return addGroup(sample);

        return false;
    }

private:
    bool initGroup(TestGroupSample& sample)
    {
        assert(isEmpty());
        assert(!sample.isEmpty());
        assert(groupSize == 0);

        for (unsigned i = 0; i < sample.data.size(); ++i)
        {
            const TestData& td = sample.getData(i);
            if (!td.isEmpty()) data.push_back(td);
            signature.push_back(td);
        }

        groupSize = static_cast<unsigned>(data.size());
        assert(groupSize > 0);
        assert(groupSize <= maxTestsNum);

        maxGroupsNum = std::min(maxGroupsNum, maxTestsNum / groupSize);
        assert(maxGroupsNum > 0);

        return true;
    }

    bool addGroup(TestGroupSample& sample)
    {
        assert(!isEmpty());
        assert(!sample.isEmpty());
        assert(groupSize != 0);

        if (isFull()) return false;

        for (unsigned i = 0; i < sample.data.size(); ++i)
        {
            const TestData& td = sample.getData(i);
            if (!td.isEmpty()) data.push_back(td);
        }
        assert(data.size() % groupSize == 0);

        return true;
    }

    bool equalSignatures(TestGroupSample& sample)
    {
        assert(!isEmpty());
        assert(!sample.isEmpty());
        assert(signature.size() == sample.data.size());

        for (unsigned i = 0; i < signature.size(); ++i)
        {
            if (signature[i].isEmpty() != sample.data[i].isEmpty()) return false;
        }

        return true;
    }
};

//==============================================================================
//==============================================================================
//==============================================================================
// Factory of test data which bundles individual tests into test groups

class TestDataFactory
{
    //==========================================================================
private:
    vector<TestGroupArray*> data;                               // Array for storing test groups
    TestGroupSample groupSample;                                // Array for data in one group
    unsigned pos;                                               // Current position

    unsigned maxGroupSize;                                      // Group size (including rejected elements)
    unsigned maxGroupsNum;                                      // Max number of groups in the bundle
    unsigned maxTestsNum;                                       // Max number of tests in the bundle

    //==========================================================================
public:
    TestDataFactory() { reset(); }

    //==========================================================================
    // PUBLIC INTERFACE
    //
    // 1. ADDING DATA:
    //      reset(...) -> 
    //                    append(...); -> append(...);  ... -> append(...); -> finishGroup(); ->
    //                    append(...); -> append(...);  ... -> append(...); -> finishGroup(); ->
    //                    ...
    //                    append(...); -> append(...);  ... -> append(...); -> finishGroup(); ->
    //                    seal();
    //
    // 2. READING DATA:
    //      getNextGroup(); getNextGroup(); ... -> getNextGroup();
    //
    //==========================================================================
public:
    void reset() 
    {
        pos = 0;
        maxGroupSize = 0;
        maxGroupsNum = 0;
        maxTestsNum  = 0;
        groupSample.reset();
        for (unsigned i = 0; i < data.size(); ++i) delete data[i];
        data.clear();
    }

    void reset(unsigned maxGroupSz, unsigned maxGroups, unsigned maxTests) 
    {
        assert(maxGroupSz != 0);
        assert(maxGroups != 0);
        assert(maxTests != 0);

        reset();
        maxGroupSize = maxGroupSz;
        maxGroupsNum = maxGroups;
        maxTestsNum  = maxTests;
    }

    void append(TestData& data)
    { 
        assert(maxGroupSize != 0);
        assert(groupSample.getFlatSize() < maxGroupSize);

        groupSample.append(data);
        if (groupSample.getFlatSize() == maxGroupSize)
        {
            if (groupSample.getActualSize() != 0) appendGroup();
            groupSample.reset();
        }
    }

    void finishGroup()
    {
        assert(groupSample.getFlatSize() == 0);

        pos = static_cast<unsigned>(data.size());
    }

    void seal()
    {
        assert(groupSample.getFlatSize() == 0);

        pos = 0;
    }

    TestGroupArray* getNextGroup()
    {
        assert(groupSample.getFlatSize() == 0);

        return (pos < data.size())? data[pos++] : 0;
    }

    bool empty() const { return data.size() == 0; }

    //==========================================================================
private:
    void appendGroup()
    {
        for (unsigned i = pos; i < data.size(); ++i)
        {
            if (data[i]->append(groupSample)) return;
        }

        TestGroupArray* group = new TestGroupArray(maxGroupsNum, maxTestsNum);
        bool ok = group->append(groupSample);
        data.push_back(group);
        assert(ok);
    }

    //==========================================================================
};

//==============================================================================
//==============================================================================
//==============================================================================

template<class T>
void emitTestDescriptionHeader(T& comment, string testName, Inst testInst, unsigned groupSize)
{
    if (groupSize == 1)
    {
        comment("Test name: " + testName);
    }
    else
    {
        comment("Test group name: " + testName);
        comment("Test group size: " + index2str(groupSize));
    }
    comment("");
    comment("Instruction: " + dumpInst(testInst));
}

template<class T>
void emitTestDescriptionBody(T& comment, Inst testInst, TestGroupArray& testGroup, TestDataMap& map, int flatTestIdx = -1)
{
    unsigned firstFlatTestIdx = (flatTestIdx >= 0)? flatTestIdx : 0;
    unsigned maxFlatTestIdx   = (flatTestIdx >= 0)? flatTestIdx : testGroup.getFlatSize() - 1;
    unsigned groupSize        = testGroup.getGroupSize();

    for (unsigned flatIdx = firstFlatTestIdx; flatIdx <= maxFlatTestIdx; ++flatIdx)
    {
        comment("");

        unsigned testIdx  = testGroup.getTestIdx(flatIdx);
        unsigned groupIdx = testGroup.getGroupIdx(flatIdx);
        TestData& data    = testGroup.getData(flatIdx);

        if (groupSize > 1)
        {
            comment("Test#" + index2str(groupIdx, 2) + "." + index2str(testIdx, 2) + "# arguments:");
        }
        else
        {
            if (flatTestIdx >= 0) {
                comment("Test arguments:");
            } else {
                comment("Test#" + index2str(groupIdx, 2) + "# arguments:");
            }
        }

        for (unsigned i = map.getFirstSrcArgIdx(); i <= map.getLastSrcArgIdx(); ++i)
        {
            assert(i < static_cast<unsigned>(testInst.operands().size()));
            assert(testInst.operand(i));
            comment("    Arg " + index2str(i) + " (" + getOperandKind(testInst, i) + "):           " + data.src[i].dump());
        }

        if (map.getDstArgsNum() == 1)
        {
            Val dstValue = data.dst;
            assert(!dstValue.empty());
            assert(testInst.type() == dstValue.getValType());

            comment("Expected result:           " + dstValue.dump());
        }

        if (map.getMemArgsNum() == 1)
        {
            Val memValue = data.mem;
            assert(!memValue.empty());

            comment("Expected result in memory: " + memValue.dump());
        }
    }
}

//==============================================================================
//==============================================================================
//==============================================================================
}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_TEST_DESC_H
