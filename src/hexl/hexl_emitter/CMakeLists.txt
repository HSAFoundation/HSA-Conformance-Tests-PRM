add_library(
hexl_emitter
BrigEmitter.cpp
CoreConfig.cpp
Emitter.cpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base)
target_link_libraries(hexl_emitter hexl_base)
