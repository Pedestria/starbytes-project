# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
cmake_policy(SET CMP0079 NEW)

set(VCPKG_ROOT "/Users/alextopper/Documents/GitHub/vcpkg")
set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
message("- Toolchain File = " ${CMAKE_TOOLCHAIN_FILE})

if(APPLE)
	set(STARBYTES_MODULE_PATH "~/Documents/GitHub/starbytes-project/cmake/modules")
	list(APPEND CMAKE_MODULE_PATH ${STARBYTES_MODULE_PATH})

	include(AddClangCXXLib)
	include(AddStarbytesLib)
elseif(WIN32)
	set(STARBYTES_MODULE_PATH "./cmake/modules")
	list(APPEND CMAKE_MODULE_PATH ${STARBYTES_MODULE_PATH})

	include(AddStarbytesLib)
endif()


option(MSVC_BUILD "Whether to Build With MSVC Or clang++" NO)

if(NOT ${MSVC_BUILD})
	if(APPLE)
		set(CMAKE_CXX_COMPILER "/Library/Developer/Toolchains/swift-5.3-DEVELOPMENT-SNAPSHOT-2020-08-18-a.xctoolchain/usr/bin/clang++")
	else()
		set(CMAKE_CXX_COMPILER "clang++")
	endif()

	set(CMAKE_CXX_COMPILER_FLAGS "-fansi-escape-codes -fms-compatibility")
	set(CMAKE_CXX_STANDARD 14)
	set(CMAKE_CXX_STANDARD_REQUIRED YES)
	message("Clang Compiler!")
elseif()
	set(CMAKE_GENERATOR "Visual Studio 16 2019")
	# set(LINK_FLAGS "/SUBSYSTEM:CONSOLE")
	message("MSVC Compiler!")
	# set(CMAKE_CXX_COMPILER_FLAGS "/link /LIBPATH 'C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Tools/MSVC/14.27.29110/lib/x64' ")
	

endif()

project ("Starbytes" CXX)

# Include sub-projects.
add_subdirectory ("Starbytes")
