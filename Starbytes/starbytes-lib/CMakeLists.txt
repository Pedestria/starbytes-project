list(APPEND HEADERS "AST.h" "Document.h" "Lexer.h" "Lookup.h" "Parser.h" "Token.h")
list(TRANSFORM HEADERS PREPEND "include/")
include_directories(include)
list(APPEND LIB_SOURCES "AST.cpp" "Document.cpp" "Lexer.cpp" "Lookup.cpp" "Parser.cpp" "Token.cpp")
list(TRANSFORM LIB_SOURCES PREPEND "lib/" )

add_library(starbytes-lib STATIC ${LIB_SOURCES} ${HEADERS})
