#include <any>
#include "Document.h"
#include <string>

#ifndef BASE_STARBYTES_ERROR_H
#define BASE_STARBYTES_ERROR_H

enum class ErrorType:int {
    ParseError,SemanticsError
};

struct ParseErrorArgs {
    Starbytes::DocumentPosition start_pos;
    Starbytes::DocumentPosition end_pos;
    std::string message;
};

// template<typename ErrorArgs = std::any>
class Error {
    ErrorType type;
    ParseErrorArgs args;
    Error(ParseErrorArgs _args){
        _args = args;
    };
    ~Error(){};
};



#endif