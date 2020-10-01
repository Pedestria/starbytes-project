add_subdirectory("Visitors")

add_starbytes_lib(
    SOURCE_FILES 
    SemanticsMain.cpp
    Scope.cpp
    Symbols.cpp 
    HEADER_FILES 
    SemanticsMain.h 
    Scope.h
    Symbols.h 
    LIB_NAME 
    "starbytesSemanticsEngineCore" 
    LIBS_TO_LINK 
    "starbytesBase"
    "starbytesAST"
    "starbytesSemanticsEngineVisitors"
    )
# create_lib_set("Semantics")
# add_lib_to_set("Semantics" "starbytesSemanticsEngineCore")
# add_lib_to_set("Semantics" "starbytesSemanticsEngineVisitors")