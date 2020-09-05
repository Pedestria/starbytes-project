# CMakeList.txt : CMake project for CPlusPlus, include source and define
# project specific logic here.
#




set(STARBYTES_LIB_INCLUDE ${CMAKE_SOURCE_DIR}/Starbytes/starbytes-lib/include)
add_subdirectory("starbytes-lib")
add_subdirectory("starbytes")
add_subdirectory("starbytes-lspserver")

set(STARBYTES_ALL_LIBS "starbytes-lib-parser")


#target_link_libraries(CPlusPlus PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
# TODO: Add tests and install targets if needed.
