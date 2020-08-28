# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)
list(APPEND CMAKE_MODULE_PATH "C:/Users/alex/Documents/GitHub/vcpkg/scripts/buildsystems")
include(vcpkg)

# set(CMAKE_TOOLCHAIN_FILE C:/Users/alex/Documents/GitHub/vcpkg/scripts/buildsystems/vcpkg.cmake)

if(NOT MSVC AND NOT MSVC_IDE OR CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_CXX_COMPILER clang++)
	set(CMAKE_CXX_COMPILER_FLAGS "-ansi")
	set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
	message("Clang Compiler!")

endif()

project ("Starbytes" CXX)

# Include sub-projects.
add_subdirectory ("Starbytes")
