add_executable(
hexl
HexlRunner.cpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base ${CMAKE_SOURCE_DIR}/src/hexl/hexl_lib)
target_link_libraries(hexl hexl_base hexl_hsaruntime hexl_lib)
