configure_file("Starbytes.cpp.in" ${CMAKE_CURRENT_SOURCE_DIR}/Starbytes.cpp)
add_starbytes_tool(
    NAME 
    starbytes 
    INCLUDE_LIB 
    FILES 
    "Starbytes.cpp" 
    "Starbytes.h" 
    DEPENDENCIES 
    ${STARBYTES_ALL_LIBS})

