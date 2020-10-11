#include "Macros.h"
#include <string>
#include <filesystem>



#ifndef BASE_FS_H
#define BASE_FS_H

// #ifdef HAS_DIRENT_H
// #include <dirent.h>
// #elif HAS_FILESYSTEM_H 
// #include <filesystem>
// #endif


STARBYTES_FOUNDATION_NAMESPACE

namespace FS = std::filesystem;

std::string * readFile(std::string & file);

template<typename Lambda>
void foreachInDirectory(std::string directory,Lambda callback){
    const FS::path path_to_it {directory};
    for(auto & entry : FS::directory_iterator(path_to_it)){
        callback(entry);
    }
};


NAMESPACE_END
#endif