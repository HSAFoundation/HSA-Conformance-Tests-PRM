@rem Includes AMD specific experimental ORCA targets - no support in the main package

set HSAIL_TOOLS=C:/ws/git/HSAIL-Tools
set HSA_RUNTIME_INC=C:/ws/hsa_prm/drivers/hsa/runtime/inc
set HSA_RUNTIME_EXT_INC=C:/ws/hsa_prm/drivers/hsa/compiler/finalizer/Interface
set OCL_PATH=C:/ws/hsa_prm/drivers/opencl/dist/windows/debug
set ORCA_OPTS=-DOCL-Path=%OCL_PATH% -DENABLE_HEXL_ORCA=1

set BASIC_OPTS=-DHSAIL-Tools-PATH=%HSAIL_TOOLS% -DHSA-Runtime-Inc-PATH=%HSA_RUNTIME_INC% -DHSA-Runtime-Ext-Inc-PATH=%HSA_RUNTIME_EXT_INC%

md build\win32
cd build\win32
cmake -G "Visual Studio 11 2012" %BASIC_OPTS% %* -DHSAIL-Tools-Subdir=win32 ..\..
cd ..\..

md build\win64
cd build\win64
cmake -G "Visual Studio 11 2012 Win64" %BASIC_OPTS% -DHSAIL-Tools-Subdir=win64 %* ..\..
cd ..\..

md build\win32_orca
cd build\win32_orca
cmake -G "Visual Studio 11 2012" %BASIC_OPTS% %ORCA_OPTS% -DHSAIL-Tools-Subdir=win32 %* ..\..
cd ..\..

md build\win64_orca
cd build\win64_orca
cmake -G "Visual Studio 11 2012 Win64" %BASIC_OPTS% %ORCA_OPTS% -DHSAIL-Tools-Subdir=win64 %* ..\..
cd ..\..
