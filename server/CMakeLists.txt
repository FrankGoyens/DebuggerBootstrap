cmake_minimum_required(VERSION 3.10)

project(DebuggerBootstrap)

set (CMAKE_C_STANDARD 11)

set(DebuggerBootstrap_Headers 
	DynamicStringArray.h
	ProjectDescription.h
	ProjectDescription_json.h
)

set(DebuggerBootstrap_Sources
	main.c
	DynamicStringArray.c
	ProjectDescription.c
	ProjectDescription_json.c
)

add_executable(DebuggerBootstrap ${DebuggerBootstrap_Headers} ${DebuggerBootstrap_Sources})

find_package(OpenSSL REQUIRED)
target_link_libraries(DebuggerBootstrap OpenSSL::Crypto)

#Add the external json library
include(ExternalProject)

set(json-c_BUILD_DIR ${CMAKE_BINARY_DIR}/json-c-build) 

ExternalProject_Add(json-c
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/../server_dependencies/json-c
	BINARY_DIR ${json-c_BUILD_DIR}
	DOWNLOAD_COMMAND ""
	UPDATE_COMMAND ""
	INSTALL_COMMAND ""
)	

add_dependencies(DebuggerBootstrap json-c)

#An interface target for all necessary json-c linking
add_library(json-c-target INTERFACE)
target_include_directories(json-c-target INTERFACE ../server_dependencies/json-c ${CMAKE_BINARY_DIR}/json-c-build)
target_link_libraries(json-c-target INTERFACE ${json-c_BUILD_DIR}/libjson-c.a)

target_link_libraries(DebuggerBootstrap json-c-target)

add_subdirectory(test)

#A sandbox program for trying out filesystem watching
add_executable(FileSystemWatcher filesystemwatcher.c)

