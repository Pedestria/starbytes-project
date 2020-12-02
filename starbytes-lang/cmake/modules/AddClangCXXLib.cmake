#Only Call this when adding clang stdlib for MACOSX!
function(add_clang_lib CLANG_LIB_TARGET)
    if(XCODE)
        return()
    endif()
    set(CXX_STANDARD_LIB_MACOSX /usr/lib/libc++.1.dylib)
    set(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
    set(CMAKE_INCLUDE_DIRECTORIES_BEFORE OFF)
    #/Applications/Xcode.app/Contents/Developer/usr/lib/llvm-gcc/4.2.1/include
    if(TARGET ${CLANG_LIB_TARGET})
        target_include_directories(${CLANG_LIB_TARGET} SYSTEM PUBLIC "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1")
        target_link_libraries(${CLANG_LIB_TARGET} PRIVATE ${CXX_STANDARD_LIB_MACOSX})
    endif()
endfunction()
