add_library(
hc_core
AddressTests.cpp  AtomicTests.hpp   BranchTests.cpp  CrossLaneTests.cpp  DirectiveTests.hpp       FunctionsTests.cpp    InitializerTests.hpp  MemoryFenceTests.cpp     MiscOperationsTests.hpp  SignalTests.cpp         UserModeQueueTests.hpp  LibrariesTests.cpp
AddressTests.hpp  BarrierTests.cpp  BranchTests.hpp  CrossLaneTests.hpp  DispatchPacketTests.cpp  FunctionsTests.hpp    LimitsTests.cpp       MemoryFenceTests.hpp     PrmCoreTests.cpp         SignalTests.hpp         ExceptionsTests.cpp     LibrariesTests.hpp
AtomicTests.cpp   BarrierTests.hpp  CMakeLists.txt   DirectiveTests.cpp  DispatchPacketTests.hpp  InitializerTests.cpp  LimitsTests.hpp       MiscOperationsTests.cpp  PrmCoreTests.hpp         UserModeQueueTests.cpp  ExceptionsTests.hpp
MModelTests.cpp   MModelTests.hpp   ExecModelTests.cpp  ExecModelTests.hpp   AtomicTestHelper.cpp AtomicTestHelper.hpp   SysArchMandatoryTests.cpp  SysArchMandatoryTests.hpp
)

target_link_libraries(hc_core hexl_base hexl_hsaruntime hexl_emitter hexl_HSAILTestGen)
target_link_libraries(hc_core hc_common)

target_include_directories(hc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
