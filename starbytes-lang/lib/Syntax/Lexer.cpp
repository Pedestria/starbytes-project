#include "starbytes/Syntax/Lexer.h"

namespace starbytes::Syntax {

bool isKeyword(llvm::StringRef str){
    return (str == "decl") || (str == "immutable");
};

bool isNumber(llvm::StringRef str){
    for(auto & c : str){
        if(!isdigit(c)){
            return false;
            break;
        };
    };
    return true;
};

struct LexicalError : public Diagnostic {
    void format(llvm::raw_ostream &os);
};

Lexer::Lexer(DiagnosticBufferedLogger & errStream):errStream(errStream){
    
};

void Lexer::tokenizeFromIStream(std::istream & in, llvm::MutableArrayRef<Tok> tokStreamRef){
    auto getChar = [&](){
        return in.get();
    };
    
    auto aheadChar = [&](){
        return in.peek();
    };
    
    SourcePos pos;
    pos.line = 1;
    pos.endCol = 0;
    
    auto toks = tokStreamRef.vec();
    
    char *bufferStart = buffer;
    char *bufferEnd = bufferStart;
    
#define PUSH_CHAR(ch) *bufferEnd = ch; ++bufferEnd; ++pos.endCol
#define INCREMENT_TO_NEXT_CHAR in.seekg(1,std::ios::cur)
    
    auto pushToken = [&](Tok::TokType type){
        size_t bufferLen = bufferEnd - bufferStart;
        pos.startCol = pos.endCol - bufferLen;
        Tok tok;
        tok.content = std::string(bufferStart,bufferLen);
        if(isKeyword(tok.content))
            tok.type = Tok::Keyword;
        else if(isNumber(tok.content))
            tok.type = Tok::Number;
        else
            tok.type = type;
        tok.srcPos = pos;
        toks.push_back(tok);
        bufferEnd = bufferStart;
    };
    
    
    char c = getChar();
    while(!in.eof()){
        switch (c) {
            case '\n': {
                ++pos.line;
                pos.endCol = 0;
                break;
            }
            case '@' : {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '['){
                    PUSH_CHAR(c);
                    pushToken(Tok::TemplateBegin);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else {
                    exit(1);
                };
                break;
            }
            case '(' : {
                PUSH_CHAR(c);
                pushToken(Tok::OpenParen);
                break;
            }
            case ')' : {
                PUSH_CHAR(c);
                pushToken(Tok::CloseParen);
                break;
            }
            case '[' : {
                PUSH_CHAR(c);
                pushToken(Tok::OpenBracket);
                break;
            }
            case ']' : {
                PUSH_CHAR(c);
                pushToken(Tok::CloseBracket);
                break;
            }
            case '{': {
                PUSH_CHAR(c);
                pushToken(Tok::OpenBrace);
                break;
            }
            case '}' : {
                PUSH_CHAR(c);
                pushToken(Tok::CloseBrace);
                break;
            }
            case '=' : {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '=') {
                    PUSH_CHAR(c);
                    pushToken(Tok::EqualEqual);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else
                    pushToken(Tok::Equal);
                break;
            }
            case '+' : {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '='){
                    PUSH_CHAR(c);
                    pushToken(Tok::PlusEqual);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else
                    pushToken(Tok::Plus);
                break;
            }
            case '-' : {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '='){
                    PUSH_CHAR(c);
                    pushToken(Tok::MinusEqual);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else
                    pushToken(Tok::Minus);
                break;
            }
            default: {
                if(isalnum(c)){
                    PUSH_CHAR(c);
                    c = aheadChar();
                    if(!isalnum(c)){
                        pushToken(Tok::Identifier);
                    }
                };
                break;
            }
        }
        c = getChar();
    };
        
};

}
