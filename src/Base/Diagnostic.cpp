#include "starbytes/Base/Diagnostic.h"
#include <iostream>

namespace starbytes {



DiagnosticBufferedLogger & DiagnosticBufferedLogger::operator<<(Diagnostic *diagnostic){
    buffer.push_back(diagnostic);
    return *this;
};

bool DiagnosticBufferedLogger::empty(){
    return buffer.empty();
};

bool DiagnosticBufferedLogger::hasErrored(){
    for(auto & diag : buffer){
        if(diag->isError()){
            return true;
            break;
        };
    };
    return false;
};

void DiagnosticBufferedLogger::logAll(){
    while(!buffer.empty()){
        auto d = buffer.front();
        buffer.pop_front();
        if(!d->resolved){
            std::string s;
            llvm::raw_string_ostream stream(s);
            stream.enable_colors(true);
            d->format(stream,*codeViewSrc);
            std::cout << s << std::endl;
        };
        delete d;
    };
};

void Diagnostic::format(llvm::raw_ostream & os,CodeViewSrc & src){
    
};

};
