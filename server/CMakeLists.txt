cmake_minimum_required(VERSION 3.10)

project(DebuggerBootstrap)

include(ExternalProject)

set(json-c_BUILD_DIR ${CMAKE_BINARY_DIR}/json-c-build) 

ExternalProject_Add(json-c-project
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/json-c
	BINARY_DIR ${json-c_BUILD_DIR}
	DOWNLOAD_COMMAND ""
	UPDATE_COMMAND ""
	INSTALL_COMMAND ""
)	

add_executable(DebuggerBootstrap main.c)
add_dependencies(DebuggerBootstrap json-c-project)
target_link_libraries(DebuggerBootstrap ${json-c_BUILD_DIR}/libjson-c.a)

find_package(OpenSSL REQUIRED)
target_link_libraries(DebuggerBootstrap OpenSSL::Crypto)

add_executable(FileSystemWatcher filesystemwatcher.c)
