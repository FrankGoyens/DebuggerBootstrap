cmake_minimum_required(VERSION 3.10)

project(DebuggerBootstrap)

set (CMAKE_C_STANDARD 11)

set(DebuggerBootstrap_Headers 
	DynamicStringArray.h
	ProjectDescription.h
	ProjectDescription_json.h
	EventDispatch.h
	Bootstrapper.h
	FileHasher.h

	protocol/Protocol.h
)

set(DebuggerBootstrap_Sources
	DynamicStringArray.c
	ProjectDescription.c
	ProjectDescription_json.c
	EventDispatch.c
	Bootstrapper.c
	FileHasher.c

	protocol/Protocol.c
)

#Everythin but main.c is put into its own library, that way it is easier to setup the test executable
add_library(DebuggerBootstrap_lib STATIC ${DebuggerBootstrap_Headers} ${DebuggerBootstrap_Sources})
target_include_directories(DebuggerBootstrap_lib PUBLIC protocol)

add_executable(DebuggerBootstrap main.c)
target_link_libraries(DebuggerBootstrap DebuggerBootstrap_lib)

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

add_dependencies(DebuggerBootstrap_lib json-c)

#An interface target for all necessary json-c linking
add_library(json-c-target INTERFACE)
target_include_directories(json-c-target INTERFACE ../server_dependencies/json-c ${CMAKE_BINARY_DIR}/json-c-build)
target_link_libraries(json-c-target INTERFACE ${json-c_BUILD_DIR}/libjson-c.a)

target_link_libraries(DebuggerBootstrap_lib json-c-target)

add_subdirectory(test)

#A sandbox program for trying out filesystem watching
add_executable(FileSystemWatcher filesystemwatcher.c)

