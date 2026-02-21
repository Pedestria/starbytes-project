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
    "adt-ref-test"
    INCLUDE_LIB
    FILES
    "AdtRefTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "compile-profile-test"
    INCLUDE_LIB
    FILES
    "CompileProfileTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "diagnostic-baseline-test"
    INCLUDE_LIB
    FILES
    "DiagnosticBaselineTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "cmdline-parser-test"
    INCLUDE_LIB
    FILES
    "CmdLineParserTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "doxygen-doc-test"
    INCLUDE_LIB
    FILES
    "DoxygenDocTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "symbol-lookup-stress-test"
    INCLUDE_LIB
    FILES
    "SymbolLookupStressTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "lsp-server-test"
    INCLUDE_LIB
    FILES
    "lspServerTest.cpp"
    "${CMAKE_SOURCE_DIR}/tools/lsp/ServerMain.cpp"
    "${CMAKE_SOURCE_DIR}/tools/lsp/DocumentAnalysis.cpp"
    "${CMAKE_SOURCE_DIR}/tools/lsp/SymbolCache.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "lsp-cache-regression-test"
    INCLUDE_LIB
    FILES
    "LspCacheRegressionTest.cpp"
    "${CMAKE_SOURCE_DIR}/tools/lsp/ServerMain.cpp"
    "${CMAKE_SOURCE_DIR}/tools/lsp/DocumentAnalysis.cpp"
    "${CMAKE_SOURCE_DIR}/tools/lsp/SymbolCache.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

configure_file("test.starb" "${CMAKE_CURRENT_BINARY_DIR}/test.starb" COPYONLY)
