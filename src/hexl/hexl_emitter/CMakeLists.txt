add_library(
hexl_emitter
BrigEmitter.cpp
CoreConfig.cpp
Emitter.cpp
BrigEmitter.hpp
CoreConfig.hpp
Emitter.hpp
EmitterCommon.hpp
Sequence.hpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base)
target_link_libraries(hexl_emitter hexl_base)
