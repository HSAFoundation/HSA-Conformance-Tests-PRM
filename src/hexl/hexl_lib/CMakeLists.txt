add_library(
hexl_lib
HexlLib.cpp  HexlLib.hpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base ${CMAKE_SOURCE_DIR}/src/hexl/hexl_hsaruntime)
target_link_libraries(hexl_lib hexl_base hexl_hsaruntime)
