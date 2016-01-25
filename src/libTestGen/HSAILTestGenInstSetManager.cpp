
#include "HSAILTestGenInstSetManager.h"
#include "HSAILTestGenInstSet.h"

#include "HSAILItems.h"
#include "Brig.h"

namespace TESTGEN {

//==============================================================================
//==============================================================================
//==============================================================================

class OpcodeComparator
{
private:
    unsigned opcode;

public:
    OpcodeComparator(unsigned opc) : opcode(opc) {}
    bool operator()(const OpcodeMap s) { return s.opcode == opcode; }
};

//==============================================================================
//==============================================================================
//==============================================================================

void InstSetManager::registerInstSet(const InstSet* is)
{
    assert(is);

    instSet.push_back(is);
    if (const Extension* e = is->getExtension())
    {
        extManager.registerExtension(e);
        extManager.disableAll();
    }
}

void InstSetManager::registerOpcodes(const InstSet* is)
{
    unsigned num;
    const unsigned* opcodes = is->getOpcodes(num);

    for (unsigned i = 0; i < num; ++i)
    {
        vector<OpcodeMap>::iterator result = find_if(opcodeMap.begin(), opcodeMap.end(), OpcodeComparator(opcodes[i]));
        if (result == opcodeMap.end() || result->is != is) //NB: the same opcode may be defined in several instruction sets
        {        
            const OpcodeMap od = {opcodes[i], is};
            opcodeMap.push_back(od);
        }
    }
}

bool InstSetManager::enable(const string& isName)
{
    if (const InstSet* is = getInstSet(isName))
    {
        registerOpcodes(is);
        imageExtEnabled |= is->isImageExt();
        if (strcmp(is->getName(), "CORE") != 0) extMgr().enable(is->getName());
        ///FG std::sort(opcodeMap.begin(), opcodeMap.end());
        return true;
    }
    else
    {
        return false;
    }
}

bool InstSetManager::isEnabled(const string& name)
{
    assert(name != "CORE");

    // This is a special case because there may be extensions of IMAGE extension
    return (name == "IMAGE" && imageExtEnabled) || extMgr().enabled(name);
}

void InstSetManager::getEnabledExtensions(vector<string>& extNames) 
{ 
    extMgr().getEnabled(extNames);
    if (!extMgr().enabled("IMAGE") && imageExtEnabled) extNames.push_back("IMAGE");
}

const char* InstSetManager::getExtension(unsigned opcode)
{
    const InstSet* is = getInstSet(opcode);
    assert(is);

    return (strcmp(is->getName(), "CORE") == 0)? "" : is->getName();
}

//==============================================================================

const OpcodeMap* InstSetManager::getOpcodeMap(unsigned opcode)
{
    vector<OpcodeMap>::iterator result = find_if(opcodeMap.begin(), opcodeMap.end(), OpcodeComparator(opcode));
    return (result == opcodeMap.end())? 0 : &*result;
}

const InstSet* InstSetManager::getInstSet(unsigned opcode) 
{ 
    const OpcodeMap* desc = getOpcodeMap(opcode); 
    assert(desc); 
    return desc->is; 
}

const InstSet* InstSetManager::getInstSet(Inst inst) { return getInstSet(inst.opcode()); }

const InstSet* InstSetManager::getInstSet(const string& instSetName)
{
    for (unsigned isIdx = 0; isIdx < (unsigned)instSet.size(); ++isIdx)
    {
        if (instSetName == instSet[isIdx]->getName()) return instSet[isIdx];
    }
    return 0;
}

ExtManager& InstSetManager::extMgr() { return extManager; }

//==============================================================================

unsigned        InstSetManager::getOpcodesNum()                                                   { assert(opcodeMap.size() > 0);   return (unsigned)opcodeMap.size(); }
unsigned        InstSetManager::getOpcode(unsigned idx)                                           { assert(idx < opcodeMap.size()); return opcodeMap[idx].opcode; }
unsigned        InstSetManager::getFormat(unsigned opcode)                                        { return getInstSet(opcode)->getFormat(opcode); }
unsigned        InstSetManager::getCategory(unsigned opcode)                                      { return getInstSet(opcode)->getCategory(opcode); }

const unsigned* InstSetManager::getProps(unsigned opcode, unsigned& prm, unsigned& sec)           { return getInstSet(opcode)->getProps(opcode, prm, sec); }
const unsigned* InstSetManager::getValidPropVals(unsigned opcode, unsigned propId, unsigned& num) { return getInstSet(opcode)->getPropVals(opcode, propId, num); }
const unsigned* InstSetManager::getAllPropVals(unsigned opcode, unsigned propId, unsigned& num)   { return getInstSet(opcode)->getPropVals(propId, num); }

bool            InstSetManager::isValidProp(Inst inst, unsigned propId)                           { return getInstSet(inst)->isValidProp(inst, propId); }
bool            InstSetManager::validatePrimaryProps(Inst inst)                                   { return getInstSet(inst)->validatePrimaryProps(inst); }
bool            InstSetManager::isValidInst(Inst inst)                                            { return getInstSet(inst)->isValidInst(inst); }

//==============================================================================

const char*     InstSetManager::opcode2str(unsigned val)                                          { return propVal2mnemo(PROP_OPCODE, val); };
const char*     InstSetManager::propVal2str(unsigned prop, unsigned val)                          { return extMgr().propVal2str(prop, val); }
const char*     InstSetManager::propVal2mnemo(unsigned prop, unsigned val)                        { return extMgr().propVal2mnemo(prop, val); }

//==============================================================================

vector<      OpcodeMap>  InstSetManager::opcodeMap;
vector<const InstSet*>   InstSetManager::instSet;
ExtManager               InstSetManager::extManager;
bool                     InstSetManager::imageExtEnabled = false;

//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

