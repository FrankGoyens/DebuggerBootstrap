project(DebuggerBootstrapTest)

enable_testing()
find_package(GTest REQUIRED)

add_executable(DebuggerBootstrapTest 
	../ProjectDescription.c 
	../DynamicStringArray.c 
	
	testProjectDescription.cpp
)
target_link_libraries(DebuggerBootstrapTest GTest::GTest GTest::Main)

include(GoogleTest)
gtest_discover_tests(DebuggerBootstrapTest)

