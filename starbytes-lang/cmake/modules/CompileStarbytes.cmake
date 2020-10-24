set(STARBYTES_BUILD_COMMAND ${CMAKE_BINARY_DIR}/bin/starbytes-build)
function(compile_starbytes_lib)
    cmake_parse_arguments("COMP_STB_LIB" "" "NAME" "SOURCE_DIR;DEPS" ${ARGN})
    set(DEPS )
    foreach(DEP IN ITEMS ${COMP_STB_LIB_DEPS})
        string(APPEND ${DEPS} "--link-dependency ${DEP}")
    endforeach()
    add_custom_target(
        ${COMP_STB_LIB_NAME} 
        COMMAND ${STARBYTES_BUILD_COMMAND} --source-dir ${COMP_STB_LIB_SOURCE_DIR} ${DEPS})
endfunction()
