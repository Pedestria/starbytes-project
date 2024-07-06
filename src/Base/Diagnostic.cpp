#include "starbytes/Base/Diagnostic.h"
#include <iostream>
#include <ostream>
#include <sstream>

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

StreamLogger::StreamLogger(std::ostream & out):out(out){

}

void StreamLogger::open(){
    
}

void StreamLogger::color(unsigned color){
    #ifdef _WIN32
    out << "\033[" << color << "m";
    #else 
    out << "\e[" << color << "m";
    #endif
}

void StreamLogger::string(string_ref s){
    out.write(s.getBuffer(),s.size());
}

void StreamLogger::lineBreak(unsigned c){
    if(c > 0){
        out << "\n";
        out.flush();
        --c;
    }
}

void StreamLogger::close(){
    out.flush();
}

DiagnosticHandler::DiagnosticHandler(std::ostream & out):logger(out){

}


std::unique_ptr<DiagnosticHandler> DiagnosticHandler::createDefault(std::ostream & out){
    return std::make_unique<DiagnosticHandler>(DiagnosticHandler(out));
}

DiagnosticHandler & DiagnosticHandler::operator<<(DiagnosticPtr diagnostic){
    buffer.push_back(diagnostic);
    return *this;
};

bool DiagnosticHandler::empty(){
    return buffer.empty();
};

bool DiagnosticHandler::hasErrored(){
    for(auto & diag : buffer){
        if(diag->isError()){
            return true;
            break;
        };
    };
    return false;
};

void DiagnosticHandler::logAll(){
    while(!buffer.empty()){
        auto & d = buffer.front();
        if(!d->resolved){
            d->send(logger);
        };
        buffer.pop_front();
    };
};

DiagnosticPtr StandardDiagnostic::createError(string_ref message){
    StandardDiagnostic d {};
    d.message = message.getBuffer();
    d.t = Diagnostic::Error;
    return std::make_unique<StandardDiagnostic>(std::move(d));
}

DiagnosticPtr StandardDiagnostic::createWarning(string_ref message){
    StandardDiagnostic d {};
    d.message = message.getBuffer();
    d.t = Diagnostic::Warning;
    return std::make_unique<StandardDiagnostic>(std::move(d));
}

auto ERROR_MESSAGE = "error:";

void StandardDiagnostic::send(StreamLogger & logger){
    if(t == Error){
        logger.color(LOGGER_COLOR_RED);
        logger.string(const_cast<char *>(ERROR_MESSAGE));
        logger.color(LOGGER_COLOR_RESET);
        logger.lineBreak();
        logger.string(message);
    }
    else {

    }
};

};
