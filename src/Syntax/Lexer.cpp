#include "starbytes/Syntax/Lexer.h"
#include "llvm/ADT/StringExtras.h"
#include <llvm/Support/raw_ostream.h>
#include <iostream>

namespace starbytes::Syntax {

bool isBooleanLiteral(llvm::StringRef str){
    return (str == TOK_TRUE) || (str == TOK_FALSE);
}

bool isKeyword(llvm::StringRef str){
    return (str == KW_DECL) || (str == KW_IMUT) || (str == KW_IMPORT)
    || (str == KW_FUNC) || (str == KW_IF) || (str == KW_ELSE);
}

bool isNumber(llvm::StringRef str,bool * isFloating){
    *isFloating = false;
    for(auto & c : str){

        if(c == '.'){
            *isFloating = true;
            continue;
        }

        if(!llvm::isDigit(c)){
            return false;
            break;
        };
    };
    
    return true;
}

struct LexicalError : public Diagnostic {
    
    bool isError() override{
        return true;
    };
    
    std::string message;
    SourcePos pos;
    void format(llvm::raw_ostream &os) override{
        os << llvm::raw_ostream::RED << "LexerError: " << llvm::raw_ostream::RESET << message << "\n";
    };
    LexicalError(const llvm::formatv_object_base & formatted_message,SourcePos pos):message(formatted_message.str()),pos(pos){
        
    };
};



Lexer::Lexer(DiagnosticBufferedLogger & errStream):errStream(errStream){
    
}

void Lexer::tokenizeFromIStream(std::istream & in, std::vector<Tok> & tokStreamRef,CodeViewSrc &src){
    auto getChar = [&](){
        char rc = in.get();
        // std::cout << "getChar:" << rc << std::endl;
        if(rc != -1)
            src.code += rc;
        return rc;
    };
    
    auto aheadChar = [&](){
        char rc = in.get();
        in.seekg(-1,std::ios::cur);
        // std::cout << "peekChar:" << rc << std::endl;
        return rc;
    };
    
    SourcePos pos;
    pos.line = 1;
    pos.endCol = 0;

    
    char *bufferStart = buffer;
    char *bufferEnd = bufferStart;
    
#define PUSH_CHAR(ch) *bufferEnd = ch; ++bufferEnd; ++pos.endCol
#define INCREMENT_TO_NEXT_CHAR in.seekg(1,std::ios::cur)
    
    auto pushToken = [&](Tok::TokType type){
        size_t bufferLen = bufferEnd - bufferStart;
        pos.startCol = pos.endCol - bufferLen;
        Tok tok;
        tok.content = std::string(bufferStart,bufferLen);
        
        bool isFloatingN;
        if(isKeyword(tok.content))
            tok.type = Tok::Keyword;
        else if(isNumber(tok.content,&isFloatingN)) {
            if(isFloatingN)
                tok.type = Tok::FloatingNumericLiteral;
            else
                tok.type = Tok::NumericLiteral;
        }
        else if(isBooleanLiteral(tok.content))
            tok.type = Tok::BooleanLiteral;
        else
            tok.type = type;
        tok.srcPos = pos;
        tokStreamRef.push_back(tok);
        bufferEnd = bufferStart;
    };
    
    src.code = "";
    
    char c;
    bool finish = false;
    while((c = getChar()) != -1){
        switch (c) {
            case '\n': {
                ++pos.line;
                pos.endCol = 0;
                break;
            }
            case '"': {
                PUSH_CHAR(c);
                while((c = getChar()) != '"'){
                    PUSH_CHAR(c);
                };
                PUSH_CHAR(c);
                pushToken(Tok::StringLiteral);
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
            case '.': {
                PUSH_CHAR(c);
                c = aheadChar();
                if(! (isdigit(c))){
                    pushToken(Tok::Dot);
                }

                break;
            }
            case ',' : {
                PUSH_CHAR(c);
                pushToken(Tok::Comma);
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
            case ':': {
                PUSH_CHAR(c);
                pushToken(Tok::Colon);
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
                    if(!isalnum(c) && c != '.'){
                        pushToken(Tok::Identifier);
                    }
                   
                }
                else if(isspace(c)){
                    
                };
                break;
            }
        }
    };
    
    PUSH_CHAR('\0');
    pushToken(Tok::EndOfFile);
}

}
