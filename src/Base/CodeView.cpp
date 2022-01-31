#include "starbytes/Base/CodeView.h"
#include <sstream>
#include <iostream>

namespace starbytes {


inline void makeCarrots(unsigned start,unsigned length,std::ostream & out){
    out << "\x1b[31m";
    while(start > 0){
        out << " " << std::endl;
        --start;
    }
    while(length > 0){
        out << "^" << std::flush;
        --length;
    }
    out << "\x1b[0m" << std::endl;
}

void generateCodeView(CodeViewSrc &src,SrcLoc & loc){
    /// Get the Surrounded 1 -2 lines if space is provided.
    std::istringstream codein(src.code);
    std::string temp;
    unsigned currentLine = 0;
    bool yes = false;
#define GETLINE_IF_POSSIBLE() if(src.code.size() < codein.tellg()){ std::getline(codein,temp,'\n'); ++currentLine; yes = true;}else { yes = false;};
    
#define PRINT_IF_POSSIBLE()  if(yes){ std::cout << temp << std::endl;};
    while(currentLine != loc.startLine){
        GETLINE_IF_POSSIBLE()
    }
    /// If error occurs on first line, get the bottom two lines.
    if(currentLine == 0){
        GETLINE_IF_POSSIBLE()
        PRINT_IF_POSSIBLE()
        makeCarrots(loc.startCol,temp.size() - loc.startCol,std::cout);
        while(currentLine != loc.endLine){
            GETLINE_IF_POSSIBLE()
            PRINT_IF_POSSIBLE()
            makeCarrots(0,temp.size(),std::cout);
        }
        GETLINE_IF_POSSIBLE()
        PRINT_IF_POSSIBLE()
        makeCarrots(loc.endCol,temp.size() - loc.endCol,std::cout);
    }
    else {
        PRINT_IF_POSSIBLE()
        makeCarrots(loc.startCol,temp.size() - loc.startCol,std::cout);
        while(currentLine != loc.endLine){
            GETLINE_IF_POSSIBLE()
            PRINT_IF_POSSIBLE()
            makeCarrots(0,temp.size(),std::cout);
        }
        GETLINE_IF_POSSIBLE()
        PRINT_IF_POSSIBLE()
        makeCarrots(loc.endCol,temp.size() - loc.endCol,std::cout);
    }
};

};
