enable_testing()

add_executable(rangeset_test rangeset_test.cc)
target_include_directories(rangeset_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME run_rangeset_test COMMAND rangeset_test)

add_test(NAME invalid_json COMMAND ../vmxethproxy -c ${CMAKE_SOURCE_DIR}/data/config-invalid.json)
set_tests_properties(invalid_json PROPERTIES PASS_REGULAR_EXPRESSION "Error: loading file")

add_executable(vmxpacket_test vmxpacket_test.cc ../src/vmxpacket.cc)
target_include_directories(vmxpacket_test PRIVATE ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src)
add_test(NAME run-vmxpacket_test COMMAND vmxpacket_test)

add_subdirectory(cmocka)
