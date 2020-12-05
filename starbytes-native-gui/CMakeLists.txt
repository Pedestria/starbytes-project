# set(STARBYTES_LSP_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include/starbytes/LSP/)
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/starbytes)

if(WIN32)
    add_subdirectory("windows")
elseif(APPLE)
    add_subdirectory("macos")