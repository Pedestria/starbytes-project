add_starbytes_lib(
    SOURCE_FILES 
    SemanticsEngine.cpp 
    HEADER_FILES 
    SemanticsEngine.h 
    LIB_NAME 
    "starbytesSemanticsCore" 
    LIBS_TO_LINK 
    "starbytesAST"
    "starbytesSymbols"
    "starbytesSemanticsDeclarations"
    "starbytesSemanticsExpressions"
    )
create_lib_set("Semantics")
add_lib_to_set("Semantics" "starbytesSemanticsCore")
add_lib_to_set("Semantics" "starbytesSemanticsDeclarations")
add_lib_to_set("Semantics" "starbytesSemanticsExpressions")