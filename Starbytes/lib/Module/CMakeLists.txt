add_starbytes_lib(
    LIB_NAME "starbytesModule"
    SOURCE_FILES
    Module.cpp
    HEADER_FILES
    Module.h
    LIBS_TO_LINK
    "starbytesBase"
    "starbytesAST"
    "starbytesParser"
)