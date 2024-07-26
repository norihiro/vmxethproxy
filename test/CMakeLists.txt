enable_testing()

add_executable(rangeset_test rangeset_test.cc)
target_include_directories(rangeset_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME run_rangeset_test COMMAND rangeset_test)

add_test(NAME invalid_json COMMAND ../vmxethproxy -c ${CMAKE_CURRENT_SOURCE_DIR}/data/config-invalid.json)
set_tests_properties(invalid_json PROPERTIES PASS_REGULAR_EXPRESSION "Error: loading file")

add_test(NAME main_multiple_config COMMAND ../vmxethproxy
	-c ${CMAKE_SOURCE_DIR}/data/config-invalid.json
	-c ${CMAKE_SOURCE_DIR}/data/config-invalid.json)
set_tests_properties(main_multiple_config PROPERTIES PASS_REGULAR_EXPRESSION "Error: ")

add_test(NAME main_unknown_option COMMAND ../vmxethproxy -x)
set_tests_properties(main_unknown_option PROPERTIES PASS_REGULAR_EXPRESSION "Error: unknown option")

add_test(NAME cfg_unknown_id COMMAND ../vmxethproxy -c ${CMAKE_CURRENT_SOURCE_DIR}/data/invalid-id.json)
set_tests_properties(cfg_unknown_id PROPERTIES PASS_REGULAR_EXPRESSION "Error: .* Unknown ID")

add_test(NAME cfg_unknown_id_1 COMMAND ../vmxethproxy -c ${CMAKE_CURRENT_SOURCE_DIR}/data/invalid-id-1.json)
set_tests_properties(cfg_unknown_id_1 PROPERTIES PASS_REGULAR_EXPRESSION "Error: cannot find instance id.")

add_executable(vmxpacket_test vmxpacket_test.cc ../src/vmxpacket.cc)
target_include_directories(vmxpacket_test PRIVATE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src)
add_test(NAME run-vmxpacket_test COMMAND vmxpacket_test)

add_test(NAME run-dummy COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run-dummy.sh ../vmxethproxy)

add_test(NAME run-autoport COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run-autoport.sh ../vmxethproxy)

add_subdirectory(cmocka)
