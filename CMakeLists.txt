cmake_minimum_required(VERSION 3.12)

project(vmxethproxy VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(
    src/config-macros.h.in
    config-macros.h
)

option(WITH_TEST "Test internal libraries" OFF)
option(WITH_COVERAGE "Enable coverage" OFF)
option(WITH_WEBSOCKETS "Provides websockets to send/receive MIDI" ON)
option(WITH_PORTMIDI "Enable host connection through MIDI device" ON)

if(${WITH_WEBSOCKETS})
	option(LWS_WITH_UPNG "" OFF)
	option(LWS_WITH_JPEG "" OFF)
	option(LWS_WITH_DLO "" OFF)
	option(LWS_WITH_STATIC "" ON)
	option(LWS_WITH_SHARED "" OFF)
	option(LWS_STATIC_PIC "" ON)
	option(LWS_WITHOUT_TESTAPPS "" ON)
	option(LWS_WITHOUT_TEST_CLIENT "" ON)
	option(LWS_WITHOUT_TEST_PING "" ON)
	option(LWS_WITHOUT_TEST_SERVER "" ON)
	option(LWS_WITHOUT_TEST_SERVER_EXTPOLL "" ON)
	option(LWS_WITH_MINIMAL_EXAMPLES "" OFF)

	add_subdirectory(libwebsockets)
endif()

if(${WITH_PORTMIDI})
	set(PORTMIDI_SRC src/vmxhost-midi.cc)
else()
	set(PORTMIDI_SRC)
endif()

add_executable(vmxethproxy
	src/vmxethproxy.cc
	src/proxycore.cc
	src/socket-moderator.cc
	src/vmxhost.cc
	src/vmxhost-dummy.cc
	src/vmxserver.cc
	src/vmxserver-client.cc
	src/vmxmonitor.cc
	src/vmxpacket.cc
	src/vmxpacket-str.cc
	src/vmxpacket-identify.cc
	src/misc.cc
	src/vmxprop.cc
	src/vmxws.cc
	util/os_gettime.c
	${PORTMIDI_SRC}
)

if(${WITH_WEBSOCKETS})
	target_link_libraries(vmxethproxy websockets)
endif()

if(${WITH_PORTMIDI})
	include(CheckIncludeFile)
	check_include_file(portmidi.h PORTMIDI_SYS)
	if(NOT "${PORTMIDI_SYS}" STREQUAL "1")
		option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
		add_subdirectory(portmidi)
		target_include_directories(vmxethproxy PRIVATE ${CMAKE_SOURCE_DIR}/portmidi/pm_common)
		target_link_directories(vmxethproxy PRIVATE ${CMAKE_BINARY_DIR}/portmidi/pm_common)
	endif()
	target_link_libraries(vmxethproxy portmidi)
endif()

find_package(Boost REQUIRED)
target_include_directories(vmxethproxy PRIVATE ${Boost_INCLUDE_DIR})
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

if(${WITH_COVERAGE})
	set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS}   --coverage")
	set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} --coverage")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

if(${WITH_TEST})
	enable_testing()
	add_subdirectory(test)
endif()
