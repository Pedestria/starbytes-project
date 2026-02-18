add_starbytes_test(
    NAME 
    "lex-test"
    INCLUDE_LIB 
    FILES 
    "lexTest.cpp"
    DEPENDENCIES 
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME 
    "parse-test"
    INCLUDE_LIB 
    FILES 
    "parseTest.cpp"
    DEPENDENCIES 
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME 
    "compile-test"
    INCLUDE_LIB 
    FILES 
    "compileTest.cpp"
    DEPENDENCIES 
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME 
    "run-test"
    INCLUDE_LIB 
    FILES 
    "runTest.cpp"
    DEPENDENCIES 
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "lsp-server-test"
    INCLUDE_LIB
    FILES
    "lspServerTest.cpp"
    "${CMAKE_SOURCE_DIR}/tools/lsp/ServerMain.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

configure_file("test.starb" "${CMAKE_CURRENT_BINARY_DIR}/test.starb" COPYONLY)
