#Only Call this when adding clang stdlib for MACOSX!
function(add_clang_lib CLANG_LIB_TARGET)
    set(CXX_STANDARD_LIB_MACOSX /usr/lib/libc++.1.dylib)
    include_directories(SYSTEM "/Library/Developer/Toolchains/swift-5.3-DEVELOPMENT-SNAPSHOT-2020-08-18-a.xctoolchain/usr/include/c++/v1")
    if(TARGET ${CLANG_LIB_TARGET})
        target_link_libraries(${CLANG_LIB_TARGET} ${CXX_STANDARD_LIB_MACOSX})
    endif()
endfunction()
