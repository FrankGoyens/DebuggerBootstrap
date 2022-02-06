project(DebuggerBootstrapTest)

enable_testing()
find_package(GTest REQUIRED)

add_executable(DebuggerBootstrapTest 	
	testProjectDescription.cpp
	testProtocol.cpp
	testBootstrapper.cpp
)

add_dependencies(DebuggerBootstrapTest json-c)
target_link_libraries(DebuggerBootstrapTest GTest::GTest GTest::Main json-c-target DebuggerBootstrap_lib)

include(GoogleTest)
gtest_discover_tests(DebuggerBootstrapTest)

