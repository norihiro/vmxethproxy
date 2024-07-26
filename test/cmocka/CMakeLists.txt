find_package(CMocka CONFIG REQUIRED)

add_executable(test_internal_proxycore internal_proxycore.cc ../../src/vmxpacket.cc ../../src/vmxpacket-identify.cc)
target_include_directories(test_internal_proxycore PRIVATE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_internal_proxycore PRIVATE cmocka::cmocka)
add_test(test_internal_proxycore ${CMAKE_CURRENT_BINARY_DIR}/test_internal_proxycore)

add_executable(test_vmxpacket_str test_vmxpacket_str.cc ../../src/vmxpacket.cc ../../src/vmxpacket-str.cc)
target_include_directories(test_vmxpacket_str PRIVATE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_vmxpacket_str PRIVATE cmocka::cmocka)
add_test(test_vmxpacket_str ${CMAKE_CURRENT_BINARY_DIR}/test_vmxpacket_str)

add_executable(test_vmxhost_midi
	test_vmxhost_midi.cc
	../../src/vmxpacket.cc
	../../src/proxycore.cc
	../../src/socket-moderator.cc
	../../src/vmxpacket-identify.cc
)
target_include_directories(test_vmxhost_midi PRIVATE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR})
target_link_libraries(test_vmxhost_midi PRIVATE cmocka::cmocka)
add_test(test_vmxhost_midi ${CMAKE_CURRENT_BINARY_DIR}/test_vmxhost_midi)

add_executable(test_misc test_misc.cc ../../src/misc.cc)
target_include_directories(test_misc PRIVATE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_misc PRIVATE cmocka::cmocka)
add_test(test_misc ${CMAKE_CURRENT_BINARY_DIR}/test_misc)
