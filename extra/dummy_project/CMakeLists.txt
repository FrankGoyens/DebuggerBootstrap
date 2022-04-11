project(DummyProject)

add_executable(DummyProject Main.c)

add_subdirectory(dummy_dependency)

target_link_libraries(DummyProject DummyDependency)