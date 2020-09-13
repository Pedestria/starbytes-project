# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
cmake_policy(SET CMP0079 NEW)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(USE_VCPKG "Use vcpkg manager!" OFF)
option(USE_VCPKG_AS_TOOLCHAIN "Use vcpkg cmake toolchain file" OFF)

macro(find_vcpkg USE_TOOLCHAIN)
	message(STATUS "Locating vcpkg executable")
	find_program(VCPKG_ROOT "vcpkg" ONLY_CMAKE_FIND_ROOT_PATH)
	if(NOT EXISTS ${VCPKG_ROOT})
		message(STATUS "Locating vcpkg executable - not found")
	else()
		message(STATUS "Locating vcpkg executable - found")
		get_filename_component(VCPKG_DIR ${VCPKG_ROOT} DIRECTORY)
		message(STATUS "Locating vcpkg ports")
		if(IS_DIRECTORY ${VCPKG_DIR}/ports)
			message(STATUS "Locating vcpkg ports - found")
			if(${USE_TOOLCHAIN})
				message(STATUS "Locating vcpkg toolchain file")
				set(VCPKG_TOOLCHAIN_FILE_DIR ${VCPKG_DIR}/scripts/buildsystems)
				if(EXISTS ${VCPKG_TOOLCHAIN_FILE_DIR}/vcpkg.cmake)
					message(STATUS "Locating vcpkg toolchain file - found")
					message(STATUS "Loading toolchain")
					list(APPEND CMAKE_MODULE_PATH ${VCPKG_TOOLCHAIN_FILE_DIR})
					include(vcpkg)
				else()
					message(STATUS "Locating vcpkg toolchain file - not found")
				endif()
			endif()
		else()
			message(STATUS "Locating vcpkg ports - not found")
			message(FATAL_ERROR "vcpkg executable is provided but there ports found in its directory!")
		endif()
	endif()
endmacro()

if(${USE_VCPKG})
	if(${USE_VCPKG_AS_TOOLCHAIN})
		find_vcpkg(TRUE)
	else()
		find_vcpkg(FALSE)
	endif()	
endif()


if(APPLE)
	set(STARBYTES_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
	list(APPEND CMAKE_MODULE_PATH ${STARBYTES_MODULE_PATH})

	include(AddClangCXXLib)
endif()


option(MSVC_BUILD "Whether to Build With MSVC Or clang++" NO)

if(NOT ${MSVC_BUILD})
	if(APPLE)
		set(CMAKE_CXX_COMPILER "/Library/Developer/Toolchains/swift-5.3-DEVELOPMENT-SNAPSHOT-2020-08-18-a.xctoolchain/usr/bin/clang++")
	else()
		set(CMAKE_CXX_COMPILER "clang++")
	endif()

	set(CMAKE_CXX_COMPILER_FLAGS "-fansi-escape-codes -fms-compatibility")
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED YES)
	message(STATUS "Clang Compiler!")
elseif()
	set(CMAKE_GENERATOR "Visual Studio 16 2019")
	# set(LINK_FLAGS "/SUBSYSTEM:CONSOLE")
	message(STATUS "MSVC Compiler!")
	# set(CMAKE_CXX_COMPILER_FLAGS "/link /LIBPATH 'C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Tools/MSVC/14.27.29110/lib/x64' ")
	

endif()

project ("Starbytes" CXX)

# Include sub-projects.
add_subdirectory ("Starbytes")
