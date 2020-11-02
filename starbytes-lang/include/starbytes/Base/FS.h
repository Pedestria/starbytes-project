#include "Macros.h"
#include <string>

#ifdef HAS_FILESYSTEM_H
#include <filesystem>
#endif


#ifndef BASE_FS_H
#define BASE_FS_H

// #ifdef HAS_DIRENT_H
// #include <dirent.h>
// #elif HAS_FILESYSTEM_H 
// #include <filesystem>
// #endif


STARBYTES_FOUNDATION_NAMESPACE



std::string * readFile(std::string & file);

#ifdef HAS_FILESYSTEM_H

namespace FS = std::filesystem;

template<typename Lambda>
void foreachInDirectory(std::string directory,Lambda callback){
    const FS::path path_to_it {directory};
    for(auto & entry : FS::directory_iterator(path_to_it)){
        callback(entry);
    }
};

#endif


NAMESPACE_END
#endif