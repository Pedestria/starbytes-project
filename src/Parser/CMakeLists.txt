add_starbytes_lib(
    LIB_NAME 
    "starbytesParser"
    SOURCE_FILES 
    "Parser.cpp"
    "SemanticA.cpp"
    "SymTable.cpp"
    "DeclSema.cpp"
    "ExprSema.cpp"
    HEADER_FILES 
    "Parser.h"
    "SemanticA.h"
    "SymTable.h"
    LIBS_TO_LINK 
    "starbytesSyntax"
)