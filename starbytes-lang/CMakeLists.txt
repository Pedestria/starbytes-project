#CMakeLists.txt Top Level File (Starbytes Project)
cmake_minimum_required (VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
cmake_policy(SET CMP0079 NEW)

option(USE_VCPKG "Use vcpkg manager!" OFF)
option(USE_VCPKG_AS_TOOLCHAIN "Use vcpkg cmake toolchain file" OFF)
set(STARBYTES_PROJECT_VERSION "v0.0.4")
message("Starbytes Project Version " ${STARBYTES_PROJECT_VERSION})

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
			message(FATAL_ERROR "vcpkg executable is provided but there are NO ports found in its directory!")
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



set(STARBYTES_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
list(APPEND CMAKE_MODULE_PATH ${STARBYTES_MODULE_PATH})

if(APPLE)
	include(AddClangCXXLib)
endif()

include(Starbytes)


set(CMAKE_CXX_COMPILER_FLAGS "-fansi-escape-codes -fms-compatibility")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)



project ("Starbytes" CXX C)

include(CheckIncludeFile)
# add_header_macro("unistd.h")
check_include_file(io.h IO_H_FOUND)
check_include_file(dirent.h DIRENT_H_FOUND)
check_include_file(filesystem FILESYSTEM_H_FOUND)
if(IO_H_FOUND)
    add_header_macro("io.h" ${IO_H_FOUND})
endif()
if(DIRENT_H_FOUND)
    add_header_macro("dirent.h" ${DIRENT_H_FOUND})
endif()
if(FILESYSTEM_H_FOUND)
    add_header_macro("filesystem" ${FILESYSTEM_H_FOUND})
endif()

add_subdirectory("lib")
add_subdirectory("tools")
add_subdirectory("tests")



#target_link_libraries(CPlusPlus PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
# TODO: Add tests and install targets if needed.
