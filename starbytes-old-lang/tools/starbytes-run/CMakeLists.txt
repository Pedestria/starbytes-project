configure_file("Starbytes.h.in" "Starbytes.h")
add_starbytes_tool(
    NAME 
    starbytes-r 
    INCLUDE_LIB 
    FILES 
    "Starbytes.cpp" 
    "Starbytes.h" 
    DEPENDENCIES 
    ${STARBYTES_ALL_LIBS})
target_include_directories(starbytes-r PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) 
