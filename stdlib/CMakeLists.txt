
function(add_starbytes_stdlib_module _NAME)
    set(REST ${ARGN})
    # message("ARGS:${REST}")
    set(T_NAME ${_NAME})
    add_library(${T_NAME} SHARED ${REST})
    add_dependencies(${T_NAME} "starbytesRuntime")
    target_include_directories(${T_NAME} PRIVATE ${STARBYTES_LIB_INCLUDE})
    target_link_libraries(${T_NAME} PRIVATE "starbytesRuntime" "starbytesCompiler" "starbytesBase")
    set_target_properties(${T_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/stdlib" SUFFIX ".ntstarbmod")
endfunction()

add_starbytes_stdlib_module("IO" "IO/IO.cpp")
