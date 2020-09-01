add_executable(starbytes-lspserver "Starbytes-LSP.cpp" "Starbytes-LSP.h" "JSONOutput.h" "JSONOutput.cpp")

add_dependencies(starbytes-lspserver starbytes-lib)
include_directories(${STARBYTES_LIB_INCLUDE})
target_link_libraries(starbytes-lspserver starbytes-lib)
if(APPLE)
    add_clang_lib(starbytes-lspserver)
endif()
