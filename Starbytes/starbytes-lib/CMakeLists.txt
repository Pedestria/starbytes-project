# New files eventually. (May split lib into mutliple libraries.)
#
# starbytes-lib:
# - SemanticsEngine.h 
# - SemanticsEngine.cpp
# - RuntimeEngine.h
# - RuntimeEngine.cpp
# - RuntimeLib.h
# - RuntimeLib.cpp
# - ModuleConstructor.cpp
# - ModuleConstructor.h
# - ModuleWriter.h
# - ModuleWriter.cpp
# - ModuleReader.h
# - ModuleReader.cpp
# - ModuleFileParser.h
# - ModuleFileParser.cpp
#
list(APPEND HEADERS "AST.h" "Document.h" "Lexer.h" "Lookup.h" "Parser.h" "Token.h" "Serialization.h")
list(TRANSFORM HEADERS PREPEND "include/")
include_directories(include)
list(APPEND LIB_SOURCES "AST.cpp" "Document.cpp" "Lexer.cpp" "Lookup.cpp" "Parser.cpp" "Token.cpp" "Serialization.cpp")
list(TRANSFORM LIB_SOURCES PREPEND "lib/" )

add_library(starbytes-lib STATIC ${LIB_SOURCES} ${HEADERS})
if(APPLE)
    add_clang_lib(starbytes-lib)
endif()
