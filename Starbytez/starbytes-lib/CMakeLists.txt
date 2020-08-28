set(HEADERS "include/AST.h" "include/Document.h" "include/Lexer.h" "include/Lookup.h" "include/Parser.h" "include/Token.h")
include_directories("./include")
set(SOURCE "lib/AST.cpp" "lib/Document.cpp" "lib/Lexer.cpp" "lib/Lookup.cpp" "lib/Parser.cpp" "lib/Token.cpp")
add_library(starbytes-lib STATIC ${SOURCE} ${HEADERS})
