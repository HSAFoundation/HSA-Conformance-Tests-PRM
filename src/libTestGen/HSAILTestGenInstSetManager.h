//===-- HSAILTestGenPropDesc.h - HSAIL Test Generator - Description of HDL properties ===//
//
//===----------------------------------------------------------------------===//
//
// (C) 2013 AMD Inc. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef INCLUDED_HSAIL_TESTGEN_PROP_DESC_H
#define INCLUDED_HSAIL_TESTGEN_PROP_DESC_H

#include "HSAILItems.h"
#include "HSAILExtManager.h"
#include <vector>

using HSAIL_ASM::Inst;
using HSAIL_ASM::ExtManager;
using HSAIL_ASM::Extension;

using std::vector;

namespace TESTGEN {

class InstSet;

//==============================================================================
//==============================================================================
//==============================================================================
// Map of an opcode to the instruction set where it is defined

struct OpcodeMap
{
    unsigned opcode;
    const InstSet* is;

    bool operator<(const OpcodeMap& c) const { return this->opcode < c.opcode; }
};

//==============================================================================
//==============================================================================
//==============================================================================
// Manager of all registered instruction sets
//
// The purposes of this component are:
// - manage a list of registered instruction sets
// - manage a list of enabled instruction sets
// - map opcodes to their instruction sets
// - redirect requests about opcode properties to the corresponding instruction sets

class InstSetManager
{
private:
    static vector<      OpcodeMap> opcodeMap;           // Mapping of opcodes to instruction sets
    static vector<const InstSet*>  instSet;             // Registered sets of instructions
    static ExtManager              extManager;          // Extension manager
    static bool                    imageExtEnabled;     // true if a enabled extension uses image-specific instruction formats

public:                                                                
    static void         registerInstSet(const InstSet* is);     // Register an instruction set
    static bool         enable(const string& instSetName);      // Enable an instruction set
    static bool         isEnabled(const string& name);          // Check if an instruction set is enabled
    static void         getEnabledExtensions(vector<string>& extNames); // Return a list of all enabled extensions
    static const char*  getExtension(unsigned opcode);          // Return the name of an extension this opcode belongs to directly ("" for CORE)

    static const ExtManager& getExtMgr() { return extMgr(); }   // Return an extension manager

private:
    static       ExtManager& extMgr();

public: // Mapping of property values to strings
    static const char*  opcode2str(unsigned opcode);
    static const char*  propVal2str(unsigned propId, unsigned propVal);
    static const char*  propVal2mnemo(unsigned propId, unsigned propVal);

public:
    static    unsigned  getOpcodesNum();                // Return number of registered opcodes
    static    unsigned  getOpcode(unsigned idx);        // Return i-th opcode

    static    unsigned  getFormat(unsigned opcode);     // Return format of the specified opcode
    static    unsigned  getCategory(unsigned opcode);   // Return category of the specified opcode

                                                        // Return all properties which describe specified instruction (primary and secondary)
                                                        // The order of primary properties is important and must be preserved.
                                                        // Primary properties have to be assigned and validated in the specified order.
                                                        // Note that meta-properties are not included in this list because TestGen
                                                        // does not work with these properties _directly_.
    static const unsigned* getProps(unsigned opcode, unsigned& prm, unsigned& sec);

                                                        // Return all positive values for 'propId' of specified instruction
    static const unsigned* getValidPropVals(unsigned opcode, unsigned propId, unsigned& num);

                                                        // Return all possible values 'propId' may take
    static const unsigned* getAllPropVals(unsigned opcode, unsigned propId, unsigned& num);  

public:
                                                        // Return true if 'propId' has a valid value for instruction 'inst', false otherwise.
                                                        // This function is able to validate each property independently of each other but
                                                        // assumes sertain order of validation. Namely, primary properties must be assigned
                                                        // and validated in the same order as specified by getProps because validation of some
                                                        // properties may include implicit checks of other properties.
                                                        // Note that this validation may be incomplete. Full validation of all necessary
                                                        // conditions for all primary properties is performed only as part of validation for
                                                        // last primary property (this also includes validation of meta-properties).
                                                        // Also note that secondary properties only depend on primary properties.
    static bool isValidProp(Inst inst, unsigned propId);

    static bool validatePrimaryProps(Inst inst);        // Return true if all primary properties have valid values. This is the complete check.

    static bool isValidInst(Inst inst);                 // Return true if instruction has valid values for all props.
                                                        // This function duplicates functionality of InstValidator (used for debugging only).
private:
    static       void       registerOpcodes(const InstSet* is);
    static const OpcodeMap* getOpcodeMap(unsigned opcode);
    static const InstSet*   getInstSet(unsigned opcode);
    static const InstSet*   getInstSet(Inst inst);
    static const InstSet*   getInstSet(const string& instSetName);

};
//==============================================================================
//==============================================================================
//==============================================================================

}; // namespace TESTGEN

#endif // INCLUDED_HSAIL_TESTGEN_PROP_DESC_H
