add_library(
hc_image
ImagesTests.cpp  ImagesTests.hpp ImageRdTests.cpp  ImageRdTests.hpp ImageQueryTests.cpp ImageQueryTests.hpp ImageLdTests.cpp  ImageLdTests.hpp ImageStTests.cpp  ImageStTests.hpp ImageInitializerTests.cpp ImageInitializerTests.hpp CMakeLists.txt
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_hsaruntime)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_emitter)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_HSAILTestGen)
include_directories(${CMAKE_SOURCE_DIR}/src/hsail_conformance/common)
target_link_libraries(hc_image hexl_emitter)
target_link_libraries(hc_image hc_common)