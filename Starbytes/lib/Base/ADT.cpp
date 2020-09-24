#include "Base/ADT.h"
#include <string>
#include <fstream>
#include <sstream>

namespace Starbytes::Foundation {
    void execute_cmd(std::string &cmd){
        
        #ifdef _WIN32
        
        #elif __APPLE__

        #endif
    }
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
};