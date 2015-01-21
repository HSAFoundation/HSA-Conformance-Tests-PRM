add_library(
hc_core
AddressTests.cpp  BarrierTests.hpp  CMakeLists.txt      DispatchPacketTests.cpp  FunctionsTests.hpp    MemoryFenceTests.cpp     MiscOperationsTests.hpp  SignalTests.cpp         UserModeQueueTests.hpp
AddressTests.hpp  BranchTests.cpp   CrossLaneTests.cpp  DispatchPacketTests.hpp  InitializerTests.cpp  MemoryFenceTests.hpp     PrmCoreTests.cpp         SignalTests.hpp         DirectiveTests.cpp
BarrierTests.cpp  BranchTests.hpp   CrossLaneTests.hpp  FunctionsTests.cpp       InitializerTests.hpp  MiscOperationsTests.cpp  PrmCoreTests.hpp         UserModeQueueTests.cpp  DirectiveTests.hpp
)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_base)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_hsaruntime)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_emitter)
include_directories(${CMAKE_SOURCE_DIR}/src/hexl/hexl_HSAILTestGen)
include_directories(${CMAKE_SOURCE_DIR}/src/hsail_conformance/common)
target_link_libraries(hc_core hexl_emitter)
target_link_libraries(hc_core hc_common)
