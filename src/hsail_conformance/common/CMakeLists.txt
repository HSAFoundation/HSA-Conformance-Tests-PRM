add_library(
hc_common
HCTests.cpp  UtilTests.cpp
HCTests.hpp  UtilTests.hpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_emitter)
target_link_libraries(hc_common hexl_base)
target_link_libraries(hc_common hexl_emitter)
