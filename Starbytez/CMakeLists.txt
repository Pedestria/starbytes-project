# CMakeList.txt : CMake project for CPlusPlus, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

set(SB_PARSER "Parser.h" "Parser.cpp" "Token.cpp" "Token.h" "Lexer.h" "Lexer.cpp" "Document.h" "Document.cpp")
set(UTILS "Lookup.cpp" "Lookup.h")
# Add source to this project's executable.
#find_package(nlohmann_json CONFIG REQUIRED)
add_executable (StarbytesCompiler "Starbytes.cpp" "Starbytes.h" ${UTILS} ${SB_PARSER} "AST.h" "AST.cpp")
#target_link_libraries(CPlusPlus PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
# TODO: Add tests and install targets if needed.
