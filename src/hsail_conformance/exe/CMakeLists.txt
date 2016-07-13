add_executable(
hc
HsailConformanceRunner.cpp
)

target_link_libraries(hc hexl_base hexl_emitter hexl_hsaruntime hexl_lib)
target_link_libraries(hc hc_common hc_core hc_image)
