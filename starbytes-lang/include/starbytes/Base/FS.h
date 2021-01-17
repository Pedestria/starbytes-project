#include "Macros.h"
#include <string>
#include <memory>

#ifdef _WIN32
#undef max 
#undef min
#endif



#ifndef BASE_FS_H
#define BASE_FS_H

// #ifdef HAS_DIRENT_H
// #include <dirent.h>
// #elif HAS_FILESYSTEM_H 
// #include <filesystem>
// #endif


STARBYTES_FOUNDATION_NAMESPACE



std::unique_ptr<std::string> readFile(std::string & file);



NAMESPACE_END
#endif