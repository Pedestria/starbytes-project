add_starbytes_tool(
        NAME
        "starbytes-disasm"
        FILES
        "disasm.cpp"
        INCLUDE_LIB
        DEPENDENCIES
        ${STARBYTES_ALL_LIBS}
)

file(GLOB LSP_SOURCES "./lsp/*.cpp")
file(GLOB LSP_HEADERS "./lsp/*.h")

add_starbytes_tool(
        NAME
        "starbytes-lsp"
        FILES
        ${LSP_SOURCES}
        ${LSP_HEADERS}
        INCLUDE_LIB
        ${STARBYTES_ALL_LIBS}
)