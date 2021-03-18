function(add_test_starbytes_file TEST_FILE)
    file(COPY ${TEST_FILE} DESTINATION ${CMAKE_BINARY_DIR}/tests)
endfunction()

add_test_starbytes_file(${CMAKE_CURRENT_SOURCE_DIR}/test.stb)