project(DebuggerBootstrapTest)

set(CMAKE_CXX_STANDARD 17)

enable_testing()
find_package(GTest REQUIRED)

add_executable(DebuggerBootstrapTest 	
	testProjectDescription.cpp
	testProtocol.cpp
	testBootstrapper.cpp
	testSubscriberUpdate.cpp
	testEventDispatch.cpp
)

add_dependencies(DebuggerBootstrapTest json-c)
target_link_libraries(DebuggerBootstrapTest GTest::GTest GTest::Main json-c-target DebuggerBootstrap_lib)

include(GoogleTest)
gtest_discover_tests(DebuggerBootstrapTest)

