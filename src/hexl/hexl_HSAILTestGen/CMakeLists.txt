find_package(Perl REQUIRED)

set(generated_dir ${CMAKE_CURRENT_BINARY_DIR})

set(generated_srcs
    ${generated_dir}/HSAILTestGen_core_gen.hpp
)

add_custom_command(
    OUTPUT ${generated_srcs}
    PRE_BUILD
    COMMAND ${PERL_EXECUTABLE} ${HSAIL-Tools-PATH}/libHSAIL/HDLProcessor.pl -target=testgen ${HSAIL-Tools-PATH}/libHSAIL/HSAILDefs.hdl ${HSAIL-Tools-PATH}/libHSAIL/HSAILCore.hdl > ${generated_dir}/HSAILTestGen_core_gen.hpp
    DEPENDS ${HSAIL-Tools-PATH}/libHSAIL/Brig.h
    COMMENT "Generating TestGen sources"
)

add_custom_target(TestGenIncludes ALL DEPENDS ${generated_srcs})

add_library(
    hexl_HSAILTestGen
    HexlTestGen.cpp
    HexlTestGen.hpp
    ${generated_srcs}
)

target_link_libraries(hexl_HSAILTestGen libTestGen hexl_base hexl_emitter)

target_include_directories(hexl_HSAILTestGen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${generated_dir})

add_dependencies(hexl_HSAILTestGen TestGenIncludes)
