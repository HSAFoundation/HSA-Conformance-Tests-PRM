
#include "HSAILTestGenProvider.h"

namespace TESTGEN {

// Context in which all (temporary) test samples are created.
// This context and all generated code are thrown away at the end of test generation.
static Context* playground;

bool TestGen::isOptimalSearch = true;

Context* TestGen::getPlayground() { assert(playground); return playground; }

void TestGen::init(bool isOpt)
{ 
    isOptimalSearch = isOpt; 
    playground = new Context(true); 
    playground->defineTestKernel();
    playground->startKernelBody();
}

void TestGen::clean()
{ 
    playground->finishKernelBody();
    delete playground;
}

}; // namespace TESTGEN
