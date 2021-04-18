#include "starbytes/Base/Diagnostic.h"
#include <iostream>

namespace starbytes {

DiagnosticBufferedLogger & DiagnosticBufferedLogger::operator<<(Diagnostic *diagnostic){
    buffer.push_back(diagnostic);
    return *this;
};

void DiagnosticBufferedLogger::logAll(){
    while(!buffer.empty()){
        auto d = buffer.front();
        buffer.pop_front();
        if(!d->resolved){
            std::string s;
            llvm::raw_string_ostream stream(s);
            d->format(stream);
            std::cout << s << std::endl;
        };
        delete d;
    };
};

};
