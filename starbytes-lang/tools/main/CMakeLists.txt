configure_file("Starbytes.h.in" "Starbytes.h")
add_starbytes_tool(
    NAME 
    starbytes 
    INCLUDE_LIB 
    FILES 
    "Starbytes.cpp" 
    "Starbytes.h" 
    DEPENDENCIES 
    ${STARBYTES_ALL_LIBS})
target_include_directories(starbytes PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) 
