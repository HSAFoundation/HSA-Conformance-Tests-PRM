add_library(
hexl_hsaruntime
HsailRuntime.cpp  HsailRuntime.hpp
)

target_link_libraries(hexl_hsaruntime hexl_base)

target_include_directories(hexl_hsaruntime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
