
include(CMakeParseArguments)

function(add_starbytes_stdlib_module _NAME)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES INCLUDE_DIRS LIBS DEFINES)
    cmake_parse_arguments(STDMOD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(STDMOD_SOURCES)
        set(REST ${STDMOD_SOURCES})
    else()
        set(REST ${ARGN})
    endif()
    # message("ARGS:${REST}")
    set(T_NAME ${_NAME})
    add_library(${T_NAME} SHARED ${REST})
    add_dependencies(${T_NAME} "starbytesRuntime")
    target_include_directories(${T_NAME} PRIVATE ${STARBYTES_LIB_INCLUDE})
    target_link_libraries(${T_NAME} PRIVATE "starbytesRuntime" "starbytesCompiler" "starbytesBase")
    if(STDMOD_INCLUDE_DIRS)
        target_include_directories(${T_NAME} PRIVATE ${STDMOD_INCLUDE_DIRS})
    endif()
    if(STDMOD_LIBS)
        target_link_libraries(${T_NAME} PRIVATE ${STDMOD_LIBS})
    endif()
    if(STDMOD_DEFINES)
        target_compile_definitions(${T_NAME} PRIVATE ${STDMOD_DEFINES})
    endif()
    set_target_properties(${T_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/stdlib" SUFFIX ".ntstarbmod")
endfunction()

add_starbytes_stdlib_module("CmdLine" "CmdLine/CmdLine.cpp")
add_starbytes_stdlib_module("IO" "IO/IO.cpp")
add_starbytes_stdlib_module("FS" "FS/FS.cpp")
add_starbytes_stdlib_module("Time" "Time/Time.cpp")
add_starbytes_stdlib_module("Env" "Env/Env.cpp")
add_starbytes_stdlib_module("Process" "Process/Process.cpp")
add_starbytes_stdlib_module("JSON" "JSON/JSON.cpp")
add_starbytes_stdlib_module("Log" "Log/Log.cpp")
add_starbytes_stdlib_module("Config" "Config/Config.cpp")

find_package(Threads REQUIRED)
add_starbytes_stdlib_module("Threading"
    SOURCES "Threading/Threading.cpp"
    LIBS Threads::Threads)

set(STARBYTES_NET_INCLUDE_DIRS)
set(STARBYTES_NET_LIBS Threads::Threads)
set(STARBYTES_NET_DEFINES)
if(STARBYTES_ASIO_INCLUDE_DIRS)
    list(APPEND STARBYTES_NET_INCLUDE_DIRS ${STARBYTES_ASIO_INCLUDE_DIRS})
    list(APPEND STARBYTES_NET_DEFINES ASIO_STANDALONE STARBYTES_HAS_ASIO=1)
else()
    message(WARNING "Asio headers not found. Building Net stdlib module in fallback mode.")
endif()

add_starbytes_stdlib_module("Net"
    SOURCES "Net/Net.cpp"
    INCLUDE_DIRS ${STARBYTES_NET_INCLUDE_DIRS}
    LIBS ${STARBYTES_NET_LIBS}
    DEFINES ${STARBYTES_NET_DEFINES})

set(STARBYTES_HTTP_INCLUDE_DIRS)
set(STARBYTES_HTTP_LIBS)
set(STARBYTES_HTTP_DEFINES)
if(STARBYTES_CURL_TARGET)
    list(APPEND STARBYTES_HTTP_LIBS ${STARBYTES_CURL_TARGET})
    list(APPEND STARBYTES_HTTP_DEFINES STARBYTES_HAS_CURL=1)
    if(STARBYTES_CURL_INCLUDE_DIRS)
        list(APPEND STARBYTES_HTTP_INCLUDE_DIRS ${STARBYTES_CURL_INCLUDE_DIRS})
    endif()
else()
    message(WARNING "libcurl target not found. Building HTTP stdlib module in fallback mode.")
endif()

add_starbytes_stdlib_module("HTTP"
    SOURCES "HTTP/HTTP.cpp"
    INCLUDE_DIRS ${STARBYTES_HTTP_INCLUDE_DIRS}
    LIBS ${STARBYTES_HTTP_LIBS}
    DEFINES ${STARBYTES_HTTP_DEFINES})

set(STARBYTES_UNICODE_INCLUDE_DIRS)
set(STARBYTES_UNICODE_LIBS)
set(STARBYTES_UNICODE_DEFINES)
find_package(ICU QUIET COMPONENTS uc i18n)
if(ICU_FOUND)
    list(APPEND STARBYTES_UNICODE_INCLUDE_DIRS ${ICU_INCLUDE_DIRS})
    list(APPEND STARBYTES_UNICODE_LIBS ${ICU_LIBRARIES})
    list(APPEND STARBYTES_UNICODE_DEFINES STARBYTES_HAS_ICU=1)
else()
    message(WARNING "ICU not found. Building Unicode stdlib module in fallback mode.")
endif()

add_starbytes_stdlib_module("Unicode"
    SOURCES "Unicode/Unicode.cpp"
    INCLUDE_DIRS ${STARBYTES_UNICODE_INCLUDE_DIRS}
    LIBS ${STARBYTES_UNICODE_LIBS}
    DEFINES ${STARBYTES_UNICODE_DEFINES})

set(STARBYTES_TEXT_INCLUDE_DIRS ${STARBYTES_UNICODE_INCLUDE_DIRS})
set(STARBYTES_TEXT_LIBS ${STARBYTES_UNICODE_LIBS} ${STARBYTES_PCRE2_TARGET})
set(STARBYTES_TEXT_DEFINES ${STARBYTES_UNICODE_DEFINES})
add_starbytes_stdlib_module("Text"
    SOURCES "Text/Text.cpp"
    INCLUDE_DIRS ${STARBYTES_TEXT_INCLUDE_DIRS}
    LIBS ${STARBYTES_TEXT_LIBS}
    DEFINES ${STARBYTES_TEXT_DEFINES})
