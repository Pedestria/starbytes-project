

# add_starbytes_lib(SOURCE_FILES main.cpp hello.cpp HEADER_FILES main.h hello.cpp LIB_NAME "test" LIB_PREFIX "Test")

# function(demo_func)

#     set(prefix DEMO)
#     set(flags IS_ASCII IS_UNICODE)
#     set(singleValues TARGET)
#     set(multiValues SOURCES RES)

#     include(CMakeParseArguments)
#     cmake_parse_arguments(${prefix}
#                      "${flags}"
#                      "${singleValues}"
#                      "${multiValues}"
#                     ${ARGN})
#     message(" DEMO_IS_ASCII: ${DEMO_IS_ASCII}")
#     message(" DEMO_IS_UNICODE: ${DEMO_IS_UNICODE}")
#     message(" DEMO_TARGET: ${DEMO_TARGET}")
#     message(" DEMO_SOURCES: ${DEMO_SOURCES}")
#     message(" DEMO_RES: ${DEMO_RES}")
#     message(" DEMO_UNPARSED_ARGUMENTS: ${DEMO_UNPARSED_ARGUMENTS}")
# endfunction()

# message("test 1")
# demo_func(SOURCES test.cpp main.cpp TARGET mainApp IS_ASCII config DEBUG)
