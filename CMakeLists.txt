cmake_minimum_required (VERSION 2.6)

project (HSA-Conformance-Tests-PRM)

set (HSAIL-Tools-PATH "NOT-DEFINED" CACHE PATH "docstring")
set (HSA-Runtime-Inc-PATH "NOT_DEFINED" CACHE PATH "docstring")
set (HSA-Runtime-Ext-PATH "NOT_DEFINED" CACHE PATH "docstring")

add_definitions( -DENABLE_HEXL_HSARUNTIME=1 )
add_definitions( -DENABLE_HEXL_HSAILTESTGEN=1 )

#set (CMAKE_BINARY_DIR "bin/")
#set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x -pthread")
endif()

if(UNIX)
  set(HSAIL-Tools-LIBS ${HSAIL-Tools-PATH}/libHSAIL/build_linux)
  set(LIB_EXT .a)
else()
  set(HSAIL-Tools-LIBS ${HSAIL-Tools-PATH}/build)
  set(LIB_EXT .lib)
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

include_directories(${HSAIL-Tools-PATH}/libHSAIL/libHSAIL ${HSAIL-Tools-PATH}/libHSAIL/libHSAIL/generated)
include_directories(${HSA-Runtime-Inc-PATH})
include_directories(${HSA-Runtime-Ext-PATH})

add_subdirectory(src/libTestGen)
add_subdirectory(src/hexl)
add_subdirectory(src/hsail_conformance)
