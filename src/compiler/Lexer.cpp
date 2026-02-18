#include "starbytes/compiler/Lexer.h"
#include <iostream>
#include <cctype>

namespace starbytes::Syntax {

bool isBooleanLiteral(string_ref str){
    return (str == TOK_TRUE) || (str == TOK_FALSE);
}

bool isKeyword(string_ref str){
    return (str == KW_DECL) || (str == KW_IMUT) || (str == KW_IMPORT)
    || (str == KW_FUNC) || (str == KW_IF) || (str == KW_ELIF) || (str == KW_ELSE) || (str == KW_RETURN)
    || (str == KW_CLASS) || (str == KW_INTERFACE) || (str == KW_FOR) || (str == KW_WHILE)
    || (str == KW_SECURE) || (str == KW_CATCH)
    || (str == KW_NEW) || (str == KW_SCOPE)
    || (str == KW_DEF);
}

bool isNumber(string_ref str,bool * isFloating){
    *isFloating = false;
    for(auto & c : str){

        if(c == '.'){
            *isFloating = true;
            continue;
        }

        if(!std::isdigit(c)){
            return false;
            break;
        };
    };
    
    return true;
}

struct LexicalError : public Diagnostic {
    
    std::string message;
    SourcePos pos;

    static DiagnosticPtr create(std::string message, SourcePos & pos);
    // void format(llvm::raw_ostream &os,CodeViewSrc & src) override{
    //     os << llvm::raw_ostream::RED << "LexerError: " << llvm::raw_ostream::RESET << message << "\n";
    // };
    // LexicalError(const llvm::formatv_object_base & formatted_message,SourcePos pos):message(formatted_message.str()),pos(pos){
        
    // };
};



Lexer::Lexer(DiagnosticHandler & errStream):buffer(),errStream(errStream){
    
}

void Lexer::tokenizeFromIStream(std::istream & in, std::vector<Tok> & tokStreamRef){
    auto getChar = [&](){
        return in.get();
    };
    
    auto aheadChar = [&](){
        return in.peek();
    };
    
    SourcePos pos;
    pos.line = 1;
    pos.endCol = 0;

    
    char *bufferStart = buffer;
    char *bufferEnd = bufferStart;
    
#define PUSH_CHAR(ch) *bufferEnd = static_cast<char>(ch); ++bufferEnd; ++pos.endCol
#define INCREMENT_TO_NEXT_CHAR (void)in.get()
    
    auto pushToken = [&](Tok::TokType type){
        size_t bufferLen = bufferEnd - bufferStart;
        pos.startCol = pos.endCol - (unsigned)bufferLen;
        Tok tok;
        tok.content = std::string(bufferStart,bufferLen);

        tok.type = type;
        if(type == Tok::Identifier){
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
        }
        tok.srcPos = pos;
        tokStreamRef.push_back(tok);
        bufferEnd = bufferStart;
    };

    auto canStartRegexLiteral = [&]() -> bool {
        if(tokStreamRef.empty()){
            return true;
        }
        Tok::TokType prev = tokStreamRef.back().type;
        switch(prev){
            case Tok::Identifier:
            case Tok::BooleanLiteral:
            case Tok::StringLiteral:
            case Tok::RegexLiteral:
            case Tok::NumericLiteral:
            case Tok::FloatingNumericLiteral:
            case Tok::CloseParen:
            case Tok::CloseBracket:
            case Tok::CloseBrace:
                return false;
            default:
                return true;
        }
    };
    
    // src.code = "";
    
    int c;
    // bool finish = false;
    while((c = getChar()) != EOF){
        switch (c) {
            case '\n': {
                ++pos.line;
                pos.endCol = 0;
                break;
            }
            case '"': {
                PUSH_CHAR(c);
                while((c = getChar()) != '"' && c != EOF){
                    PUSH_CHAR(c);
                };
                if(c == '"'){
                    PUSH_CHAR(c);
                }
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
                    pushToken(Tok::AtSign);
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
            case '/': {
                int ahead = aheadChar();
                if(ahead == '/'){
                    INCREMENT_TO_NEXT_CHAR;
                    ++pos.endCol;
                    while((c = getChar()) != EOF && c != '\n'){
                        ++pos.endCol;
                    }
                    if(c == '\n'){
                        ++pos.line;
                        pos.endCol = 0;
                    }
                    break;
                }
                if(ahead == '*'){
                    INCREMENT_TO_NEXT_CHAR;
                    ++pos.endCol;
                    int prev = 0;
                    while((c = getChar()) != EOF){
                        if(c == '\n'){
                            ++pos.line;
                            pos.endCol = 0;
                        }
                        else {
                            ++pos.endCol;
                        }
                        if(prev == '*' && c == '/'){
                            break;
                        }
                        prev = c;
                    }
                    break;
                }

                if(!canStartRegexLiteral()){
                    PUSH_CHAR(c);
                    if(ahead == '='){
                        PUSH_CHAR(ahead);
                        pushToken(Tok::FSlashEqual);
                        INCREMENT_TO_NEXT_CHAR;
                    }
                    else {
                        pushToken(Tok::FSlash);
                    }
                    break;
                }

                PUSH_CHAR(c);
                bool escaped = false;
                bool terminated = false;
                while((c = getChar()) != EOF){
                    if(c == '\n'){
                        ++pos.line;
                        pos.endCol = 0;
                        break;
                    }
                    PUSH_CHAR(c);
                    if(c == '/' && !escaped){
                        terminated = true;
                        break;
                    }
                    if(c == '\\' && !escaped){
                        escaped = true;
                    }
                    else {
                        escaped = false;
                    }
                }
                if(terminated){
                    while(true){
                        int flag = aheadChar();
                        if(!std::isalpha((unsigned char)flag)){
                            break;
                        }
                        INCREMENT_TO_NEXT_CHAR;
                        PUSH_CHAR(flag);
                    }
                    pushToken(Tok::RegexLiteral);
                }
                else {
                    pushToken(Tok::FSlash);
                }
                break;
            }
            case '*': {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '='){
                    PUSH_CHAR(c);
                    pushToken(Tok::AsteriskEqual);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else {
                    pushToken(Tok::Asterisk);
                }
                break;
            }
            case '%': {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '='){
                    PUSH_CHAR(c);
                    pushToken(Tok::PercentEqual);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else {
                    pushToken(Tok::Percent);
                }
                break;
            }
            case '<': {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '='){
                    PUSH_CHAR(c);
                    pushToken(Tok::LessEqual);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else {
                    pushToken(Tok::LessThan);
                }
                break;
            }
            case '>': {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '='){
                    PUSH_CHAR(c);
                    pushToken(Tok::GreaterEqual);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else {
                    pushToken(Tok::GreaterThan);
                }
                break;
            }
            case '&': {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '&'){
                    PUSH_CHAR(c);
                    pushToken(Tok::LogicAND);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else {
                    pushToken(Tok::BitwiseAND);
                }
                break;
            }
            case '|': {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '|'){
                    PUSH_CHAR(c);
                    pushToken(Tok::LogicOR);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else {
                    pushToken(Tok::BitwiseOR);
                }
                break;
            }
            case '?': {
                PUSH_CHAR(c);
                pushToken(Tok::QuestionMark);
                break;
            }
            case '!': {
                PUSH_CHAR(c);
                c = aheadChar();
                if(c == '='){
                    PUSH_CHAR(c);
                    pushToken(Tok::NotEqual);
                    INCREMENT_TO_NEXT_CHAR;
                }
                else {
                    pushToken(Tok::Exclamation);
                }
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
                if(std::isalnum(static_cast<unsigned char>(c)) || c == '_'){
                    PUSH_CHAR(c);
                    c = aheadChar();
                    bool tokenStartsWithDigit = std::isdigit(static_cast<unsigned char>(*bufferStart));
                    bool dotContinuesFloat = tokenStartsWithDigit && c == '.';
                    bool identifierContinues = std::isalnum(static_cast<unsigned char>(c)) || c == '_';
                    if(c == EOF || (!identifierContinues && !dotContinuesFloat)){
                        pushToken(Tok::Identifier);
                    }
                   
                }
                else if(std::isspace(static_cast<unsigned char>(c))){
                    ++pos.endCol;
                };
                break;
            }
        }
    };
    
    PUSH_CHAR('\0');
    pushToken(Tok::EndOfFile);
}

}
