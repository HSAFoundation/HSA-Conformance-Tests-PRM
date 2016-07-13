add_library(
hc_image
ImagesTests.cpp  ImagesTests.hpp ImageRdTests.cpp  ImageRdTests.hpp ImageQueryTests.cpp ImageQueryTests.hpp ImageLdTests.cpp  ImageLdTests.hpp 
ImageStTests.cpp  ImageStTests.hpp ImageInitializerTests.cpp ImageInitializerTests.hpp ImageLimitsTests.cpp ImageLimitsTests.hpp CMakeLists.txt
)

target_link_libraries(hc_image hexl_base hexl_hsaruntime hexl_emitter)
target_link_libraries(hc_image hc_common)

target_include_directories(hc_image PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
