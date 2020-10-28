set(STARBYTES_BUILD_COMMAND ${CMAKE_BINARY_DIR}/bin/starbytes-b)
message(STATUS "Compilation For Starbytes: ENABLED")
function(compile_starbytes_lib)
    cmake_parse_arguments("COMP_STB_LIB" "" "NAME" "SOURCE_DIR;DEPS" ${ARGN})
    set(DEPS )
    foreach(DEP IN ITEMS ${COMP_STB_LIB_DEPS})
        string(APPEND ${DEPS} "--link-module ${DEP}")
    endforeach()
    message("Adding Starbytes Module - ${COMP_STB_LIB_NAME}")
    add_custom_command(
        OUTPUT "${COMP_STB_LIB_NAME}.stbxm"
        COMMAND ${STARBYTES_BUILD_COMMAND} 
        ARGS "-L --module-name ${COMP_STB_LIB_NAME} --source-dir ${COMP_STB_LIB_SOURCE_DIR} ${DEPS}")
    add_custom_target(${COMP_STB_LIB_NAME} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${COMP_STB_LIB_NAME}.stbxm)
endfunction()
