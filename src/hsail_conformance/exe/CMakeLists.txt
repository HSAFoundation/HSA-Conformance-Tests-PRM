add_executable(
hc
HsailConformanceRunner.cpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_emitter)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_hsaruntime)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_lib)
include_directories(${CMAKE_SOURCE_DIR}/src/hsail_conformance/common)
include_directories(${CMAKE_SOURCE_DIR}/src/hsail_conformance/core)
include_directories(${CMAKE_SOURCE_DIR}/src/hsail_conformance/image)
target_link_libraries(hc hexl_base hexl_hsaruntime hexl_emitter hexl_lib hc_core hc_image hc_common hexl_HSAILTestGen)
