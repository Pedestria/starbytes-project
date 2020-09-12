# find_package(nlohmann_json CONFIG REQUIRED)
add_starbytes_tool(NAME starbytes-lsp INCLUDE_LIB FILES "Starbytes-LSP.cpp" "Starbytes-LSP.h" "JSONOutput.h" "JSONOutput.cpp" "LSPProtocol.h" "LSPProtocol.cpp" DEPENDENCIES ${STARBYTES_ALL_LIBS})

