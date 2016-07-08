set HSAIL_TOOLS=C:/ws/git/HSAIL-Tools

set BASIC_OPTS=-DHSAIL-Tools-PATH=%HSAIL_TOOLS%

md build\win32
cd build\win32
cmake -G "Visual Studio 11 2012" %BASIC_OPTS% %* -DHSAIL-Tools-Subdir=win32 ..\..
cd ..\..

md build\win64
cd build\win64
cmake -G "Visual Studio 11 2012 Win64" %BASIC_OPTS% -DHSAIL-Tools-Subdir=win64 %* ..\..
cd ..\..
