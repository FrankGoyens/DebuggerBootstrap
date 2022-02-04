project(DebuggerBootstrapTest)

enable_testing()
find_package(GTest REQUIRED)

add_executable(DebuggerBootstrapTest 
	../DynamicStringArray.c 
	../ProjectDescription.c 
	../ProjectDescription_json.c 
	
	testProjectDescription.cpp
)

add_dependencies(DebuggerBootstrapTest json-c)
target_link_libraries(DebuggerBootstrapTest GTest::GTest GTest::Main json-c-target)

include(GoogleTest)
gtest_discover_tests(DebuggerBootstrapTest)

