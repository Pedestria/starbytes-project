#include "starbytes/Base/FS.h"
#include <sstream>
#include <fstream>
#include <iostream>

STARBYTES_FOUNDATION_NAMESPACE

std::unique_ptr<std::string> readFile(std::string & file) {
        std::unique_ptr<std::string> ptr;
        std::cout << "File to Open:" << file <<  std::endl;
        std::ifstream input (file);
        if(input.is_open()){
            std::ostringstream file;
            file << input.rdbuf();
            ptr = std::make_unique<std::string>(file.str());
            input.close();
        }
        else{
            std::cout << "Failed to Open:" << file << "!" << std::endl;
            ptr = std::make_unique<std::string>("");
        }
        return ptr;
        
}

NAMESPACE_END