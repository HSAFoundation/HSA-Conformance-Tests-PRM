add_library(
hexl_HSAILTestGen
HexlTestGen.cpp  HexlTestGen.hpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base)
include_directories(${CMAKE_SOURCE_DIR}/src/libTestGen)
target_link_libraries(hexl_HSAILTestGen libTestGen)
