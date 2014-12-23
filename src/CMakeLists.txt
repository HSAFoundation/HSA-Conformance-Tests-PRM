cmake_minimum_required (VERSION 2.6)
project (HSA-Conformance-Tests-PRM)
add_library(
src/hexl/hexl_base
Arena.cpp
BasicHexlTests.cpp
Grid.cpp
HexlResource.cpp
HexlTest.cpp
HexlTestFactory.cpp
HexlTestList.cpp
HexlTestRunner.cpp
MObject.cpp
Options.cpp
RuntimeContext.cpp
Scenario.cpp
Utils.cpp
)
