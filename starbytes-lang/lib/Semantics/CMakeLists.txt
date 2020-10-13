

add_starbytes_lib(
    SOURCE_FILES 
    SemanticsMain.cpp
    Scope.cpp
    Symbols.cpp 
    StarbytesDecl.cpp
    HEADER_FILES 
    SemanticsMain.h 
    Scope.h
    StarbytesDecl.h
    Symbols.h 
    Main.h
    LIB_NAME 
    "starbytesSemanticsEngine" 
    LIBS_TO_LINK 
    "starbytesBase"
    "starbytesAST"
    )
# create_lib_set("Semantics")
# add_lib_to_set("Semantics" "starbytesSemanticsEngineCore")
# add_lib_to_set("Semantics" "starbytesSemanticsEngineVisitors")