set(libTestGen_srcs
    HSAILTestGenBackend.cpp            
    HSAILTestGenBackend.h              
    HSAILTestGenBackendEml.h           
    HSAILTestGenBackendLua.h         
    HSAILTestGenFpEmulator.h     
    HSAILTestGenInstSet.h
    HSAILTestGenBrigContext.cpp
    HSAILTestGenBrigContext.h  
    HSAILTestGenContext.cpp    
    HSAILTestGenContext.h      
    HSAILTestGenFpEmulator.cpp 
    HSAILTestGenDataProvider.cpp 
    HSAILTestGenDataProvider.h   
    HSAILTestGenDump.cpp         
    HSAILTestGenDump.h           
    HSAILTestGenEmulatorTypes.cpp
    HSAILTestGenEmulator.cpp  
    HSAILTestGenEmulator.h    
    HSAILTestGenInstDesc.h    
    HSAILTestGenManager.h     
    HSAILTestGenEmulatorTypes.h
    HSAILTestGenProp.cpp   
    HSAILTestGenProp.h     
    HSAILTestGenInstSetManager.cpp
    HSAILTestGenInstSetManager.h  
    HSAILTestGenProvider.cpp
    HSAILTestGenProvider.h  
    HSAILTestGenSample.cpp  
    HSAILTestGenSample.h    
    HSAILTestGenTestData.h   
    HSAILTestGenTestDesc.cpp 
    HSAILTestGenTestDesc.h   
    HSAILTestGenUtilities.cpp
    HSAILTestGenUtilities.h
    HSAILTestGenVal.cpp
    HSAILTestGenVal.h
)

add_library(
    libTestGen
    ${libTestGen_srcs}
)

target_link_libraries(libTestGen hsail)

target_include_directories(libTestGen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
