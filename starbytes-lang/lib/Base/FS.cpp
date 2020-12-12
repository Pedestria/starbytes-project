#include "starbytes/Base/FS.h"
#include <sstream>
#include <fstream>
#include <iostream>

STARBYTES_FOUNDATION_NAMESPACE

std::string * readFile(std::string & file) {
        std::string * buffer = new std::string();
        std::cout << "File to Open:" << file <<  std::endl;
        std::ifstream input (file);
        if(input.is_open()){
            std::ostringstream file;
            file << input.rdbuf();
            *buffer = file.str();
            input.close();
        }
        else{
            std::cout << "Failed to Open:!" << file << std::endl;
            *buffer = "";
        }
        return buffer;
        
}

NAMESPACE_END