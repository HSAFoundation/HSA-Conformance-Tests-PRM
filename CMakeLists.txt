cmake_minimum_required (VERSION 2.6)

project (HSA-Conformance-Tests-PRM)

set(HSAIL-Tools-PATH "NOT-DEFINED" CACHE PATH "docstring")
set(HSA-Runtime-Inc-PATH "NOT_DEFINED" CACHE PATH "docstring")
set(HSA-Runtime-Ext-Inc-PATH "NOT_DEFINED" CACHE PATH "docstring")

add_definitions( -DENABLE_HEXL_HSARUNTIME=1 )
add_definitions( -DENABLE_HEXL_HSAILTESTGEN=1 )

#set (CMAKE_BINARY_DIR "bin/")
#set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
#set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


if(UNIX)
  set(LIB_PREFIX lib)
  set(LIB_EXT .a)  
  set(HSAIL-Tools-LIBS ${HSAIL-Tools-PATH}/build/${HSAIL-Tools-Subdir}/libHSAIL)
else()
  set(LIB_PREFIX)
  set(LIB_EXT .lib)
  set(HSAIL-Tools-LIBS ${HSAIL-Tools-PATH}/build/${HSAIL-Tools-Subdir}/libHSAIL/debug)
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

if(UNIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -pthread -D __STDC_LIMIT_MACROS -D __STDC_CONSTANT_MACROS  -D __STDC_FORMAT_MACROS -fno-rtti")
endif()

include_directories(${HSAIL-Tools-PATH}/libHSAIL)
include_directories(${HSAIL-Tools-PATH}/build/${HSAIL-Tools-Subdir}/libHSAIL/generated)
include_directories(${HSA-Runtime-Inc-PATH})
include_directories(${HSA-Runtime-Ext-Inc-PATH})

add_subdirectory(src/libTestGen)
add_subdirectory(src/hexl)
add_subdirectory(src/hsail_conformance)
