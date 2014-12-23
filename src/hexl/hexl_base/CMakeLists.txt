add_library(
hexl_base
Arena.cpp           BasicHexlTests.hpp  Grid.hpp          HexlTest.cpp         HexlTestFactory.hpp  HexlTestRunner.cpp  MObject.hpp  RuntimeContext.cpp  Scenario.hpp  Utils.hpp
Arena.hpp           DllApi.hpp          HexlResource.cpp  HexlTest.hpp         HexlTestList.cpp     HexlTestRunner.hpp  Options.cpp  RuntimeContext.hpp  Stats.hpp     cmake_install.cmake
BasicHexlTests.cpp  Grid.cpp            HexlResource.hpp  HexlTestFactory.cpp  HexlTestList.hpp     MObject.cpp         Options.hpp  Scenario.cpp        Utils.cpp
)

target_link_libraries(hexl_base ${HSAIL-Tools-LIBS}/libhsail.a)
target_link_libraries(hexl_base dl)
