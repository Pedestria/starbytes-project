# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)
option(MSVC_BUILD "Whether to Build With MSVC Or clang++" OFF)
# list(APPEND CMAKE_MODULE_PATH "C:/Users/alex/Documents/GitHub/vcpkg/scripts/buildsystems")
# include(vcpkg)

# set(CMAKE_TOOLCHAIN_FILE C:/Users/alex/Documents/GitHub/vcpkg/scripts/buildsystems/vcpkg.cmake)

if(NOT ${MSVC_BUILD})
	set(CMAKE_CXX_COMPILER clang++)
	set(CMAKE_CXX_COMPILER_FLAGS "-ansi -fms-compatibility")
	set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
	message("Clang Compiler!")

elseif()
	set(CMAKE_CXX_COMPILER cl)
	message("MSVC Compiler!")

endif()

project ("Starbytes" CXX)

# Include sub-projects.
add_subdirectory ("Starbytes")
