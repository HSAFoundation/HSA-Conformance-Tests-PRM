find_package(Perl REQUIRED)

set(generated_srcs
HSAILTestGen_gen.hpp
)

add_library(
libTestGen
HSAILTestGenBackend.cpp   HSAILTestGenBrigContext.cpp  HSAILTestGenDataProvider.cpp  HSAILTestGenEmulator.cpp  HSAILTestGenProp.cpp      HSAILTestGenProvider.cpp  HSAILTestGenTestData.h     HSAILTestGenUtilities.h
HSAILTestGenBackend.h     HSAILTestGenBrigContext.h    HSAILTestGenDataProvider.h    HSAILTestGenEmulator.h    HSAILTestGenProp.h        HSAILTestGenProvider.h    HSAILTestGenTestDesc.cpp   HSAILTestGenVal.cpp
HSAILTestGenBackendEml.h  HSAILTestGenContext.cpp      HSAILTestGenDump.cpp          HSAILTestGenInstDesc.h    HSAILTestGenPropDesc.cpp  HSAILTestGenSample.cpp    HSAILTestGenTestDesc.h     HSAILTestGenVal.h
HSAILTestGenBackendLua.h  HSAILTestGenContext.h        HSAILTestGenDump.h            HSAILTestGenManager.h     HSAILTestGenPropDesc.h    HSAILTestGenSample.h      HSAILTestGenUtilities.cpp
HSAILTestGenFpEmulator.h  HSAILTestGenFpEmulator.cpp   HSAILTestGenEmulatorTypes.cpp HSAILTestGenEmulatorTypes.h 
${generated_srcs}
)

set(generated_dir ${CMAKE_CURRENT_BINARY_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
  OUTPUT ${generated_srcs}
  PRE_BUILD
  COMMAND ${PERL_EXECUTABLE} ${HSAIL-Tools-PATH}/libHSAIL/libHSAIL/HDLProcessor.pl -target=testgen < ${HSAIL-Tools-PATH}/libHSAIL/libHSAIL/HSAILBrigInstr.hdl > ${generated_dir}/HSAILTestGen_gen.hpp
  DEPENDS ${HSAIL-Tools-PATH}/libHSAIL/libHSAIL/Brig.h
  COMMENT "Generating libTestGen sources"
)

add_custom_target(TestGenIncludes ALL DEPENDS ${generated_srcs})

include_directories(${CMAKE_SOURCE_DIR}/src/libTestGen)
add_dependencies(libTestGen TestGenIncludes)
#target_link_libraries(libTestGen hexl_base)
