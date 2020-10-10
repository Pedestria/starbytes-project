#include "starbytes/Base/FS.h"
#include <sstream>
#include <fstream>

STARBYTES_FOUNDATION_NAMESPACE

std::string * readFile(std::string & file) {
        std::string * buffer = new std::string();
        std::ifstream input (file);
        if(input.is_open()){
            std::ostringstream file;
            file << input.rdbuf();
            *buffer = file.str();
            input.close();
        }
        else{
            *buffer = "";
        }
        return buffer;
        
}

NAMESPACE_END