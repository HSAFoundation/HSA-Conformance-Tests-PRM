add_library(
hc_common
HCTests.cpp  UtilTests.cpp
HCTests.hpp  UtilTests.hpp
)

target_link_libraries(hc_common hexl_base hexl_emitter)

target_include_directories(hc_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
