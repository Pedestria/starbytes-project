

add_starbytes_tool(
    NAME "starbytes" 
    INCLUDE_LIB FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/profile/CompileProfile.cpp
    DEPENDENCIES ${STARBYTES_ALL_LIBS})

add_starbytes_tool(
    NAME "starbytes-disasm" 
    INCLUDE_LIB FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/disasm/disasm.cpp 
    DEPENDENCIES ${STARBYTES_ALL_LIBS})

file(GLOB LSP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lsp/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/lsp/*.h)
add_starbytes_tool(
    NAME "starbytes-lsp" 
    INCLUDE_LIB FILES 
    ${LSP_SOURCES}
    DEPENDENCIES ${STARBYTES_ALL_LIBS})
