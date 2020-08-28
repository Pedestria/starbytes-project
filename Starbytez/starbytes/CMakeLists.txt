add_executable(StarbytesCompiler "Starbytes.cpp" "Starbytes.h")

add_dependencies(StarbytesCompiler starbytes-lib)
include_directories(${STARBYTES_LIB_INCLUDE})
target_link_libraries(StarbytesCompiler starbytes-lib)
