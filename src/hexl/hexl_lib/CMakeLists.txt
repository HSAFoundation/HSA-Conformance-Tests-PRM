add_library(
hexl_lib
HexlLib.cpp  HexlLib.hpp
)

target_link_libraries(hexl_lib hexl_base hexl_hsaruntime)

if(ENABLE_HEXL_ORCA)
  target_link_libraries(hexl_lib hexl_orca)
endif()

target_include_directories(hexl_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
