add_library(
hexl_base
Arena.cpp
BasicHexlTests.hpp
Grid.hpp
HexlTest.cpp
HexlTestFactory.hpp
HexlTestRunner.cpp
MObject.hpp
RuntimeContext.cpp
Scenario.hpp
Utils.hpp
Image.cpp
Image.hpp
Arena.hpp
DllApi.hpp
HexlResource.cpp
HexlTest.hpp
HexlTestList.cpp
HexlTestRunner.hpp
Options.cpp
RuntimeContext.hpp
Stats.hpp
BasicHexlTests.cpp
Grid.cpp
HexlResource.hpp
HexlTestFactory.cpp
HexlTestList.hpp
MObject.cpp
Options.hpp
Scenario.cpp
Utils.cpp
RuntimeCommon.hpp
HexlContext.hpp
HexlContext.cpp
HexlObjects.hpp
HexlObjects.cpp
)

target_link_libraries(hexl_base hsail)

if(UNIX)
  target_link_libraries(hexl_base pthread dl)
endif()

target_include_directories(hexl_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
