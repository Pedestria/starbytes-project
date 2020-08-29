add_executable(StarbytesCompiler "Starbytes.cpp" "Starbytes.h")


# find_package(nlohmann_json CONFIG REQUIRED)
add_dependencies(StarbytesCompiler starbytes-lib)
include_directories(${STARBYTES_LIB_INCLUDE})
# target_link_libraries(StarbytesCompiler PRIVATE starbytes-lib PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
