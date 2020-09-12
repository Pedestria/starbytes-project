# CMakeList.txt : CMake project for CPlusPlus, include source and define
# project specific logic here.
#
set(STARBYTES_LIB_INCLUDE ${CMAKE_SOURCE_DIR}/Starbytes/starbytes-lib/include)

function(add_starbytes_tool)
    cmake_parse_arguments("STARBYTES_TOOL" "INCLUDE_LIB" "NAME" "FILES;DEPENDENCIES" ${ARGN})
    add_executable(${STARBYTES_TOOL_NAME} ${STARBYTES_TOOL_FILES})
    if(${STARBYTES_TOOL_INCLUDE_LIB})
        message("Including All libraries for tool target:" ${STARBYTES_TOOL_NAME})
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
    target_link_libraries(${STARBYTES_TOOL_NAME} PRIVATE ${STARBYTES_TOOL_DEPENDENCIES})
    
endfunction()


set(STARBYTES_ALL_LIBS "starbytesParser" "starbytesSemantics")


add_subdirectory("starbytes-lib")
add_subdirectory("starbytes")
add_subdirectory("starbytes-lsp")



#target_link_libraries(CPlusPlus PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
# TODO: Add tests and install targets if needed.
