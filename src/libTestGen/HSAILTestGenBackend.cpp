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

#include "HSAILTestGenBackend.h"
#include "HSAILTestGenBackendLua.h"
#include "HSAILTestGenUtilities.h"

using std::string;

namespace TESTGEN {

//=============================================================================
//=============================================================================
//=============================================================================

TestGenBackend *TestGenBackend::backend = 0;

TestGenBackend* TestGenBackend::get(string name)
{
    if (!backend)
    {
        if (name.length() == 0) //  default dummy backend
        {
            backend = new TestGenBackend();
        }
        else if (name == "LUA" || name == "lua")
        {
            backend = new LuaBackend();
        }
        else
        {
            throw TestGenError("Unknown TestGen extension: " + name);
        }
    }
    return backend;
}

void TestGenBackend::dispose()
{
    if (backend)
    {
        delete backend;
        backend = 0;
    }
}

//=============================================================================

} // namespace TESTGEN

