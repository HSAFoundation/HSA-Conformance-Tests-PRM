cmake_minimum_required (VERSION 2.6)

project (HSA-Conformance-Tests-PRM)

set(HSAIL-Tools-PATH "${CMAKE_SOURCE_DIR}/HSAIL-Tools" CACHE PATH "Path to HSAIL-Tools root.")
set(HSA-Runtime-Inc-PATH "${CMAKE_SOURCE_DIR}/include/hsa/1.0" CACHE PATH "Path to HSA 1.0 includes (hsa.h, hsa_ext_image.h).")
set(HSA-Runtime-Ext-Inc-PATH "${CMAKE_SOURCE_DIR}/include/hsa/1.0" CACHE PATH "Path to HSA 1.0 extension includes (hsa_ext_finalize.h).")

add_definitions(-DENABLE_HEXL_HSARUNTIME=1)
add_definitions(-DENABLE_HEXL_HSAILTESTGEN=1)

if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

if(UNIX)
  add_compile_options(-Wall -std=c++11 -pthread -fno-rtti)
  add_definitions(-D__STDC_LIMIT_MACROS)
  add_definitions(-D__STDC_CONSTANT_MACROS)
  add_definitions(-D__STDC_FORMAT_MACROS)
endif()

option(BUILD_LIBBRIGDWARF "" OFF)
option(VENDOR_EXTENSIONS "" OFF)
option(BUILD_HSAILASM "" OFF)
add_subdirectory(${HSAIL-Tools-PATH})

include_directories(${HSA-Runtime-Inc-PATH})
include_directories(${HSA-Runtime-Ext-Inc-PATH})

add_subdirectory(src/libTestGen)
add_subdirectory(src/hexl)
add_subdirectory(src/hsail_conformance)

install(FILES README.md DESTINATION . COMPONENT hsail_conformance)
install(FILES docs/HsaPrmConformance.md docs/ReleaseNotes DESTINATION docs COMPONENT hsail_conformance)
