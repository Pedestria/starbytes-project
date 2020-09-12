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

function(add_starbytes_lib)
    set(OPTIONS STATIC)
    set(oneValue LIB_NAME LIB_PREFIX)
    set(MULTI SOURCE_FILES HEADER_FILES LIBS_TO_LINK)
    include(CMakeParseArguments)

    cmake_parse_arguments("STARBYTES_LIB" 
    "${OPTIONS}" 
    "${oneValue}" 
    "${MULTI}" 
    ${ARGN})

    list(TRANSFORM STARBYTES_LIB_HEADER_FILES PREPEND ./include/${STARBYTES_LIB_LIB_PREFIX}/)
    list(TRANSFORM STARBYTES_LIB_SOURCE_FILES PREPEND ./lib/${STARBYTES_LIB_LIB_PREFIX}/)
    
    message(${STARBYTES_LIB_LIB_NAME} " - Sources:" ${STARBYTES_LIB_SOURCE_FILES})
    message(${STARBYTES_LIB_LIB_NAME} " - Headers:" ${STARBYTES_LIB_HEADER_FILES})
    include_directories(include)
    if(APPLE)
        add_clang_lib(${STARBYTES_LIB_LIB_NAME})
    endif()
    add_library(${STARBYTES_LIB_LIB_NAME} STATIC ${STARBYTES_LIB_SOURCE_FILES} ${STARBYTES_LIB_HEADER_FILES})
    list(LENGTH STARBYTES_LIB_LIBS_TO_LINK LENGTH_LIST)
    if(${LENGTH_LIST} GREATER 0)
        foreach(_LIB IN ITEMS ${STARBYTES_LIB_LIBS_TO_LINK})
            add_dependencies(${STARBYTES_LIB_LIB_NAME} ${_LIB})
        endforeach()
        message(${STARBYTES_LIB_LIB_NAME} "- Dependencies:" ${STARBYTES_LIB_LIBS_TO_LINK})
        target_link_libraries(${STARBYTES_LIB_LIB_NAME} PRIVATE ${STARBYTES_LIB_LIBS_TO_LINK})   
    endif()
        
    

    
    
    
endfunction()

add_starbytes_lib(SOURCE_FILES AST.cpp Document.cpp Lexer.cpp Lookup.cpp Parser.cpp Token.cpp Serialization.cpp HEADER_FILES AST.h Document.h Lexer.h Lookup.h Parser.h Token.h Serialization.h LIB_NAME "starbytesParser" LIB_PREFIX "Parser")

add_starbytes_lib(SOURCE_FILES SemanticsEngine.cpp HEADER_FILES SemanticsEngine.h LIB_NAME "starbytesSemantics" LIB_PREFIX "Semantics" LIBS_TO_LINK "starbytesParser")



