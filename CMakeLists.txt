
cmake_minimum_required (VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
# cmake_policy(SET CMP0079 NEW)

option(USE_VCPKG "Use vcpkg manager!" OFF)
option(USE_VCPKG_AS_TOOLCHAIN "Use vcpkg cmake toolchain file" OFF)
set(ENABLE_STARBYTES_LSP TRUE)
set(STARBYTES_PROJECT_VERSION "v0.2")
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
					include(${VCPKG_TOOLCHAIN_FILE_DIR}/vcpkg.cmake)
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


# set(STARBYTES_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)
# list(APPEND CMAKE_MODULE_PATH ${STARBYTES_MODULE_PATH})


# if(UNIX)
# 	include(AddClangCXXLib)
# endif()



set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
# set(CMAKE_LINK_FLAGS "${CMAKE_LINK_FLAGS} -frtti")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)


project ("Starbytes" CXX C)

if(NOT LLVM_BUILD)
	set(LLVM_BUILD FALSE)
endif()

if(NOT ${LLVM_BUILD})
	find_package(LLVM REQUIRED)
endif()




include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
message("DEF: ${LLVM_DEFINITIONS_LIST}")
add_definitions(${LLVM_DEFINITIONS_LIST})

message("Found LLVM: ${LLVM_PACKAGE_VERSION}")



llvm_map_components_to_libnames(llvm_libs support)


include(./Starbytes.cmake)

if(XCODE)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

if(APPLE)
	message("macOS: ${CMAKE_OSX_SYSROOT}")
endif()

include(CheckIncludeFile)
check_include_file(unistd.h UNISTD_H_FOUND)
check_include_file(filesystem FILESYSTEM_H_FOUND)
check_include_file(sys/wait.h SYS_WAIT_H_FOUND)
if(UNISTD_H_FOUND)
    add_header_macro("unistd.h" ${UNISTD_H_FOUND})
endif()
if(FILESYSTEM_H_FOUND)
    add_header_macro("filesystem" ${FILESYSTEM_H_FOUND})
endif()
if(SYS_WAIT_H_FOUND)
	add_header_macro("sys_wait" ${SYS_WAIT_H_FOUND})
endif()

install(DIRECTORY "include/" DESTINATION "${CMAKE_INSTALL_PREFIX}/include" FILES_MATCHING PATTERN "*.h")

add_subdirectory("src")
add_subdirectory("driver")
add_subdirectory("extra")
add_subdirectory("tests")
add_subdirectory("embed")
# TODO : Get Starbytes Compilation to work!
# add_subdirectory("starbytes-modules")

# if(ENABLE_STARBYTES_LSP)
# 	add_subdirectory(../starbytes-lsp "lsp")
# endif()

#target_link_libraries(CPlusPlus PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
# TODO: Add tests and install targets if needed.
