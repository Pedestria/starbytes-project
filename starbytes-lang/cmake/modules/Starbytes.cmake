set(STARBYTES_LIB_INCLUDE ${CMAKE_SOURCE_DIR}/include)
set(STARBYTES_SOURCE_DIR ${CMAKE_SOURCE_DIR})
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
    set(OPTIONS STATIC)
    set(oneValue LIB_NAME)
    set(MULTI SOURCE_FILES HEADER_FILES LIBS_TO_LINK)
    include(CMakeParseArguments)

    cmake_parse_arguments("STARBYTES_LIB"
    "${OPTIONS}"
    "${oneValue}"
    "${MULTI}"
    ${ARGN})

    include_directories(${STARBYTES_LIB_INCLUDE})
    file(RELATIVE_PATH includePath ${STARBYTES_SOURCE_DIR}/lib ${CMAKE_CURRENT_SOURCE_DIR})
    list(TRANSFORM STARBYTES_LIB_HEADER_FILES PREPEND ${STARBYTES_SOURCE_DIR}/include/starbytes/${includePath}/)
    add_library(${STARBYTES_LIB_LIB_NAME} STATIC ${STARBYTES_LIB_HEADER_FILES} ${STARBYTES_LIB_SOURCE_FILES})
    set_target_properties(${STARBYTES_LIB_LIB_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    message("Adding Starbytes Library - " ${STARBYTES_LIB_LIB_NAME})

    set(STARBYTES_ALL_LIBS ${STARBYTES_ALL_LIBS} ${STARBYTES_LIB_LIB_NAME} CACHE INTERNAL "All libs")
    if(APPLE)
        add_clang_lib(${STARBYTES_LIB_LIB_NAME})
    endif()
    list(LENGTH STARBYTES_LIB_LIBS_TO_LINK LENGTH_LIST)
    if(${LENGTH_LIST} GREATER 0)
        foreach(_LIB IN ITEMS ${STARBYTES_LIB_LIBS_TO_LINK})
            add_dependencies(${STARBYTES_LIB_LIB_NAME} ${_LIB})
        endforeach()
        message(${STARBYTES_LIB_LIB_NAME} "- Dependencies:" ${STARBYTES_LIB_LIBS_TO_LINK})
        target_link_libraries(${STARBYTES_LIB_LIB_NAME} PRIVATE ${STARBYTES_LIB_LIBS_TO_LINK})
    endif()






endfunction()

function(add_starbytes_tool)
    cmake_parse_arguments("STARBYTES_TOOL" "INCLUDE_LIB" "NAME" "FILES;DEPENDENCIES" ${ARGN})
    add_executable(${STARBYTES_TOOL_NAME} ${STARBYTES_TOOL_FILES})
    set_target_properties(${STARBYTES_TOOL_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    message("Adding Tool - " ${STARBYTES_TOOL_NAME})
    if(${STARBYTES_TOOL_INCLUDE_LIB})
        target_include_directories(${STARBYTES_TOOL_NAME} PUBLIC ${STARBYTES_LIB_INCLUDE})
    endif()
    if(APPLE)
        add_clang_lib(${STARBYTES_TOOL_NAME})
    endif()
    foreach(DEP IN ITEMS ${STARBYTES_TOOL_DEPENDENCIES})
        if(TARGET ${DEP})
            add_dependencies(${STARBYTES_TOOL_NAME} ${DEP})
        endif()
    endforeach()
    message(${STARBYTES_TOOL_NAME} "Dependencies - " ${STARBYTES_TOOL_DEPENDENCIES})
    target_link_libraries(${STARBYTES_TOOL_NAME} PRIVATE ${STARBYTES_TOOL_DEPENDENCIES})

endfunction()

function(add_starbytes_test)
    cmake_parse_arguments("STARBYTES_TEST" "INCLUDE_LIB" "NAME" "FILES;DEPENDENCIES" ${ARGN})
    add_executable(${STARBYTES_TEST_NAME} ${STARBYTES_TEST_FILES})
    set_target_properties(${STARBYTES_TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    message("Adding Test - " ${STARBYTES_TEST_NAME})
    if(${STARBYTES_TEST_INCLUDE_LIB})
        target_include_directories(${STARBYTES_TEST_NAME} PUBLIC ${STARBYTES_LIB_INCLUDE})
    endif()
    if(APPLE)
        add_clang_lib(${STARBYTES_TEST_NAME})
    endif()
    foreach(DEP IN ITEMS ${STARBYTES_TEST_DEPENDENCIES})
        if(TARGET ${DEP})
            add_dependencies(${STARBYTES_TEST_NAME} ${DEP})
        endif()
    endforeach()
    message(${STARBYTES_TEST_NAME} "Dependencies - " ${STARBYTES_TEST_DEPENDENCIES})
    target_link_libraries(${STARBYTES_TEST_NAME} PRIVATE ${STARBYTES_TEST_DEPENDENCIES})

    add_test(NAME "Test-${STARBYTES_TEST_NAME}" COMMAND ${STARBYTES_TEST_NAME})
endfunction()