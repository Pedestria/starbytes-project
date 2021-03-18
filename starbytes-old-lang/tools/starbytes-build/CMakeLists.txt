configure_file("Main.h.in" "Main.h")
add_starbytes_tool(
    NAME 
    starbytes-b
    INCLUDE_LIB 
    FILES 
    "Main.cpp"
    "Main.h" 
    DEPENDENCIES 
    ${STARBYTES_ALL_LIBS})
target_include_directories(starbytes-b PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) 
