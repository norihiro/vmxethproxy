enable_testing()

add_executable(rangeset_test rangeset_test.cc)
target_include_directories(rangeset_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME run_rangeset_test COMMAND rangeset_test)
