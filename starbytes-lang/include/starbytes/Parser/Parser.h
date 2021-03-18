#include <istream>
#include "starbytes/Syntax/Lexer.h"

#ifndef STARBYTES_PARSER_PARSER_H
#define STARBYTES_PARSER_PARSER_H
namespace Starbytes {
    class Parser {
        std::unique_ptr<Syntax::Lexer> lexer;
    public:
        void parseFromStream(std::istream & in);
    };
};

#endif
