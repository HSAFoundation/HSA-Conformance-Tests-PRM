add_library(
hexl_hsaruntime
HsailRuntime.cpp  HsailRuntime.hpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base)
target_link_libraries(hexl_hsaruntime hexl_base)
