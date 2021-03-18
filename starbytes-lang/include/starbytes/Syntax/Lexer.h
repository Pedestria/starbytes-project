

#ifndef STARBYTES_SYNTAX_LEXER_H
#define STARBYTES_SYNTAX_LEXER_H

namespace Starbytes {
namespace Syntax {

class Lexer {
    
public:
    Lexer();
    void initializeWithStream(std::istream & in);
    void finish();
};

};
}

#endif
