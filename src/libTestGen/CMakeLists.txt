add_library(
libTestGen
HSAILTestGenBackend.cpp   HSAILTestGenBrigContext.cpp  HSAILTestGenDataProvider.cpp  HSAILTestGenEmulator.cpp  HSAILTestGenProp.cpp      HSAILTestGenProvider.cpp  HSAILTestGenTestData.h     HSAILTestGenUtilities.h
HSAILTestGenBackend.h     HSAILTestGenBrigContext.h    HSAILTestGenDataProvider.h    HSAILTestGenEmulator.h    HSAILTestGenProp.h        HSAILTestGenProvider.h    HSAILTestGenTestDesc.cpp   HSAILTestGenVal.cpp
HSAILTestGenBackendEml.h  HSAILTestGenContext.cpp      HSAILTestGenDump.cpp          HSAILTestGenInstDesc.h    HSAILTestGenPropDesc.cpp  HSAILTestGenSample.cpp    HSAILTestGenTestDesc.h     HSAILTestGenVal.h
HSAILTestGenBackendLua.h  HSAILTestGenContext.h        HSAILTestGenDump.h            HSAILTestGenManager.h     HSAILTestGenPropDesc.h    HSAILTestGenSample.h      HSAILTestGenUtilities.cpp
HSAILTestGenFpEmulator.h  HSAILTestGenFpEmulator.cpp   HSAILTestGenEmulatorTypes.cpp HSAILTestGenEmulatorTypes.h 
)

if(UNIX)
  add_custom_target(TestGenIncludes
    perl ${HSAIL-Tools-PATH}/libHSAIL/libHSAIL/HDLProcessor.pl -target=testgen < ${HSAIL-Tools-PATH}/libHSAIL/libHSAIL/HSAILBrigInstr.hdl > ${CMAKE_SOURCE_DIR}/src/libTestGen/HSAILTestGen_gen.hpp
  )
else()
  add_custom_target(TestGenIncludes
    $ENV{DK_ROOT}/perl/5.12.2/bin/perl ${HSAIL-Tools-PATH}/libHSAIL/libHSAIL/HDLProcessor.pl -target=testgen < ${HSAIL-Tools-PATH}/libHSAIL/libHSAIL/HSAILBrigInstr.hdl > ${CMAKE_SOURCE_DIR}/src/libTestGen/HSAILTestGen_gen.hpp
  )
endif()

include_directories(${CMAKE_SOURCE_DIR}/src/libTestGen)
add_dependencies(libTestGen TestGenIncludes)
#target_link_libraries(libTestGen hexl_base)
