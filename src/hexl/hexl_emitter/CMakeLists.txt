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

target_link_libraries(hexl_emitter hexl_base)

target_include_directories(hexl_emitter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
