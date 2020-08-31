add_executable(starbytes "Starbytes.cpp" "Starbytes.h")


# find_package(nlohmann_json CONFIG REQUIRED)
add_dependencies(starbytes starbytes-lib)
include_directories(${STARBYTES_LIB_INCLUDE} SYSTEM "/Library/Developer/Toolchains/swift-5.3-DEVELOPMENT-SNAPSHOT-2020-08-18-a.xctoolchain/usr/include/c++/v1")
target_link_libraries(starbytes starbytes-lib)
if(APPLE)
    add_clang_lib(starbytes)
endif()
# target_link_libraries(StarbytesCompiler PRIVATE starbytes-lib PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
