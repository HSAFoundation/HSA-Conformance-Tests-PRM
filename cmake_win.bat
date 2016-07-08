md build\win32
cd build\win32
cmake -G "Visual Studio 11 2012" %* ..\..
cd ..\..

md build\win64
cd build\win64
cmake -G "Visual Studio 11 2012 Win64" %* ..\..
cd ..\..
