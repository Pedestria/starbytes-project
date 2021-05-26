set(STARBYTES_LIB_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/include)
set(STARBYTES_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(STARBYTES_ALL_LIBS "" CACHE INTERNAL "All libs")


macro(add_header_macro HEADER FOUND)
    string(REGEX MATCH "([A-Za-z0-9_]+)\\.h" TEST ${HEADER})
    if(TEST)
        if(${TEST} STREQUAL ${HEADER})
            get_filename_component(FINAL_HEADER ${TEST} NAME_WE)
        endif()
    else()
        set(FINAL_HEADER ${HEADER})
    endif()
    string(TOUPPER ${FINAL_HEADER} UPPER_HEADER)
    message("HAS_${UPPER_HEADER}_H")
    add_compile_definitions("HAS_${UPPER_HEADER}_H")
endmacro()

function(add_starbytes_lib)
    set(OPTIONS STATIC SHARED)
    set(oneValue LIB_NAME)
    set(MULTI SOURCE_FILES HEADER_FILES LIBS_TO_LINK)
    include(CMakeParseArguments)

    cmake_parse_arguments("STARBYTES_LIB"
    "${OPTIONS}"
    "${oneValue}"
    "${MULTI}"
    ${ARGN})

    file(RELATIVE_PATH includePath ${STARBYTES_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
    list(TRANSFORM STARBYTES_LIB_HEADER_FILES PREPEND ${STARBYTES_SOURCE_DIR}/include/starbytes/${includePath}/)
    add_library(${STARBYTES_LIB_LIB_NAME} STATIC ${STARBYTES_LIB_HEADER_FILES} ${STARBYTES_LIB_SOURCE_FILES})
    set_target_properties(${STARBYTES_LIB_LIB_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" FOLDER "Starbytes Libraries")
    if(UNIX AND NOT APPLE)
        set_source_files_properties(${STARBYTES_LIB_HEADER_FILES} ${STARBYTES_LIB_SOURCE_FILES} COMPILE_FLAGS -fPIC)
    endif()
    message("Adding Starbytes Library - " ${STARBYTES_LIB_LIB_NAME})
    target_include_directories(${STARBYTES_LIB_LIB_NAME} PUBLIC ${STARBYTES_LIB_INCLUDE})

    set(STARBYTES_ALL_LIBS ${STARBYTES_ALL_LIBS} ${STARBYTES_LIB_LIB_NAME} CACHE INTERNAL "All libs")
    # if(UNIX)
    #     add_clang_lib(${STARBYTES_LIB_LIB_NAME})
    # endif()
    list(LENGTH STARBYTES_LIB_LIBS_TO_LINK LENGTH_LIST)
    if(${LENGTH_LIST} GREATER 0)
        foreach(_LIB IN ITEMS ${STARBYTES_LIB_LIBS_TO_LINK})
            add_dependencies(${STARBYTES_LIB_LIB_NAME} ${_LIB})
        endforeach()
        message(${STARBYTES_LIB_LIB_NAME} "- Dependencies:" ${STARBYTES_LIB_LIBS_TO_LINK})
        target_link_libraries(${STARBYTES_LIB_LIB_NAME} PRIVATE ${STARBYTES_LIB_LIBS_TO_LINK} ${llvm_libs})
    endif()

    install(TARGETS ${STARBYTES_LIB_LIB_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")

endfunction()

function(add_external_starbytes_lib)
    set(OPTIONS STATIC)
    set(oneValue LIB_NAME INCLUDE_PATH)
    set(MULTI SOURCE_FILES HEADER_FILES LIBS_TO_LINK)
    include(CMakeParseArguments)

    cmake_parse_arguments("STARBYTES_LIB"
    "${OPTIONS}"
    "${oneValue}"
    "${MULTI}"
    ${ARGN})

    list(TRANSFORM STARBYTES_LIB_HEADER_FILES PREPEND ${STARBYTES_LIB_INCLUDE_PATH})
    add_library(${STARBYTES_LIB_LIB_NAME} STATIC ${STARBYTES_LIB_HEADER_FILES} ${STARBYTES_LIB_SOURCE_FILES})
    set_target_properties(${STARBYTES_LIB_LIB_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" FOLDER "Other Starbytes Libraries")
    message("Adding Library - " ${STARBYTES_LIB_LIB_NAME})

    set(STARBYTES_ALL_LIBS ${STARBYTES_ALL_LIBS} ${STARBYTES_LIB_LIB_NAME} CACHE INTERNAL "All libs")
    # if(UNIX)
    #     add_clang_lib(${STARBYTES_LIB_LIB_NAME})
    # endif()
    list(LENGTH STARBYTES_LIB_LIBS_TO_LINK LENGTH_LIST)
    if(${LENGTH_LIST} GREATER 0)
        foreach(_LIB IN ITEMS ${STARBYTES_LIB_LIBS_TO_LINK})
            add_dependencies(${STARBYTES_LIB_LIB_NAME} ${_LIB})
        endforeach()
        message(${STARBYTES_LIB_LIB_NAME} "- Dependencies:" ${STARBYTES_LIB_LIBS_TO_LINK})
        target_link_libraries(${STARBYTES_LIB_LIB_NAME} PRIVATE ${STARBYTES_LIB_LIBS_TO_LINK} ${llvm_libs})
    endif()
endfunction()

function(add_starbytes_tool)
    cmake_parse_arguments("STARBYTES_TOOL" "INCLUDE_LIB" "NAME" "FILES;DEPENDENCIES" ${ARGN})
    add_executable(${STARBYTES_TOOL_NAME} ${STARBYTES_TOOL_FILES})
    set_target_properties(${STARBYTES_TOOL_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" FOLDER "Starbytes Tools")
    message("Adding Tool - " ${STARBYTES_TOOL_NAME})
    if(UNIX AND NOT APPLE)
        set_source_files_properties(${STARBYTES_TOOL_FILES} COMPILE_FLAGS -fPIC)
    endif()
    if(${STARBYTES_TOOL_INCLUDE_LIB})
        target_include_directories(${STARBYTES_TOOL_NAME} PUBLIC ${STARBYTES_LIB_INCLUDE})
    endif()
    # if(UNIX)
    #     add_clang_lib(${STARBYTES_TOOL_NAME})
    # endif()
    foreach(DEP IN ITEMS ${STARBYTES_TOOL_DEPENDENCIES})
        if(TARGET ${DEP})
            add_dependencies(${STARBYTES_TOOL_NAME} ${DEP})
        endif()
    endforeach()
    message(${STARBYTES_TOOL_NAME} "Dependencies - " ${STARBYTES_TOOL_DEPENDENCIES})
    target_link_libraries(${STARBYTES_TOOL_NAME} PRIVATE ${STARBYTES_TOOL_DEPENDENCIES} ${llvm_libs})

    install(TARGETS ${STARBYTES_TOOL_NAME}  RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endfunction()

function(add_starbytes_test)
    cmake_parse_arguments("STARBYTES_TEST" "INCLUDE_LIB" "NAME" "FILES;DEPENDENCIES" ${ARGN})
    add_executable(${STARBYTES_TEST_NAME} ${STARBYTES_TEST_FILES})
    set_target_properties(${STARBYTES_TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests" FOLDER "Starbytes Tests")
    message("Adding Test - " ${STARBYTES_TEST_NAME})
    if(UNIX AND NOT APPLE)
        set_source_files_properties(${STARBYTES_TEST_FILES} COMPILE_FLAGS -fPIC)
    endif()
    if(${STARBYTES_TEST_INCLUDE_LIB})
        target_include_directories(${STARBYTES_TEST_NAME} PUBLIC ${STARBYTES_LIB_INCLUDE})
    endif()
    # if(UNIX)
    #     add_clang_lib(${STARBYTES_TEST_NAME})
    # endif()
    foreach(DEP IN ITEMS ${STARBYTES_TEST_DEPENDENCIES})
        if(TARGET ${DEP})
            add_dependencies(${STARBYTES_TEST_NAME} ${DEP})
        endif()
    endforeach()
    message(${STARBYTES_TEST_NAME} "Dependencies - " ${STARBYTES_TEST_DEPENDENCIES})
    target_link_libraries(${STARBYTES_TEST_NAME} PRIVATE ${STARBYTES_TEST_DEPENDENCIES} ${llvm_libs})

    add_test(NAME "Test-${STARBYTES_TEST_NAME}" COMMAND ${STARBYTES_TEST_NAME})
endfunction()