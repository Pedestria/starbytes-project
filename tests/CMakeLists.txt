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
    "runtime-profile-test"
    INCLUDE_LIB
    FILES
    "RuntimeProfileTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "bytecode-version-compatibility-test"
    INCLUDE_LIB
    FILES
    "BytecodeVersionCompatibilityTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "bytecode-v2-phase1-test"
    INCLUDE_LIB
    FILES
    "BytecodeV2Phase1Test.cpp"
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
    "linguist-cmdline-test"
    INCLUDE_LIB
    FILES
    "LinguistCmdLineTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "linguistics-phase1-test"
    INCLUDE_LIB
    FILES
    "LinguisticsPhase1Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "linguistics-phase3-lint-test"
    INCLUDE_LIB
    FILES
    "LinguisticsPhase3LintTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "linguistics-phase4-suggestion-action-test"
    INCLUDE_LIB
    FILES
    "LinguisticsPhase4SuggestionActionTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

if(NOT WIN32)
    add_starbytes_test(
        NAME
        "linguist-workspace-phase6-test"
        INCLUDE_LIB
        FILES
        "LinguistWorkspacePhase6Test.cpp"
        DEPENDENCIES
        ${STARBYTES_ALL_LIBS})
    add_starbytes_test(
        NAME
        "linguist-lint-mode-test"
        INCLUDE_LIB
        FILES
        "LinguistLintModeTest.cpp"
        DEPENDENCIES
        ${STARBYTES_ALL_LIBS})
else()
    message(STATUS "Skipping linguist-workspace-phase6-test and linguist-lint-mode-test on Windows targets (require sys/wait.h).")
endif()

add_starbytes_test(
    NAME
    "linguistics-phase7-golden-test"
    INCLUDE_LIB
    FILES
    "LinguisticsPhase7GoldenTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "linguistics-phase7-regression-test"
    INCLUDE_LIB
    FILES
    "LinguisticsPhase7RegressionTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "linguistics-phase7-performance-test"
    INCLUDE_LIB
    FILES
    "LinguisticsPhase7PerformanceTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "linguistics-phase8-semantic-lint-test"
    INCLUDE_LIB
    FILES
    "LinguisticsPhase8SemanticLintTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "semantic-unused-bindings-test"
    INCLUDE_LIB
    FILES
    "SemanticUnusedBindingsTest.cpp"
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

add_starbytes_test(
    NAME
    "generic-free-function-phase1-test"
    INCLUDE_LIB
    FILES
    "GenericFreeFunctionPhase1Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "generic-free-function-phase2-test"
    INCLUDE_LIB
    FILES
    "GenericFreeFunctionPhase2Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "generic-method-phase5-test"
    INCLUDE_LIB
    FILES
    "GenericMethodPhase5Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "generic-metadata-wave0-test"
    INCLUDE_LIB
    FILES
    "GenericMetadataWave0Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "generic-defaults-wave1-test"
    INCLUDE_LIB
    FILES
    "GenericDefaultsWave1Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "generic-constructor-wave2-test"
    INCLUDE_LIB
    FILES
    "GenericConstructorWave2Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "numeric-phase1-lexer-test"
    INCLUDE_LIB
    FILES
    "NumericPhase1LexerTest.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "slot-locals-phase2-test"
    INCLUDE_LIB
    FILES
    "SlotLocalsPhase2Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "numeric-arrays-phase3-test"
    INCLUDE_LIB
    FILES
    "NumericArraysPhase3Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "specialized-numeric-bytecode-phase4-test"
    INCLUDE_LIB
    FILES
    "SpecializedNumericBytecodePhase4Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "adaptive-quickening-phase5-test"
    INCLUDE_LIB
    FILES
    "AdaptiveQuickeningPhase5Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "feedback-vectors-phase2-test"
    INCLUDE_LIB
    FILES
    "FeedbackVectorsPhase2Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "execution-model-phase3-test"
    INCLUDE_LIB
    FILES
    "ExecutionModelPhase3Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

add_starbytes_test(
    NAME
    "execution-model-phase4-test"
    INCLUDE_LIB
    FILES
    "ExecutionModelPhase4Test.cpp"
    DEPENDENCIES
    ${STARBYTES_ALL_LIBS})

configure_file("test.starb" "${CMAKE_CURRENT_BINARY_DIR}/test.starb" COPYONLY)
