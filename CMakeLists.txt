cmake_minimum_required(VERSION 3.12)

project(vmxethproxy VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(
    src/config-macros.h.in
    config-macros.h
)

add_executable(vmxethproxy
	src/vmxethproxy.cc
	src/proxycore.cc
	src/socket-moderator.cc
	src/vmxhost.cc
	src/vmxserver.cc
	src/vmxserver-client.cc
	src/vmxmonitor.cc
	src/vmxpacket.cc
	src/vmxpacket-identify.cc
	src/misc.cc
	src/vmxprop.cc
	util/os_gettime.c
)

target_include_directories(vmxethproxy PRIVATE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	if (MSVC)
		add_definitions(/MP /d2FH4-)
		add_definitions("-D_USE_MATH_DEFINES")
		add_definitions("-D_CRT_SECURE_NO_WARNINGS") # to avoid a warning for `fopen`
	endif()
else()
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()
