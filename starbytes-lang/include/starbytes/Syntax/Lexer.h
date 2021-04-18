#include <istream>
#include <vector>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include "starbytes/Base/Diagnostic.h"

#ifndef STARBYTES_SYNTAX_LEXER_H
#define STARBYTES_SYNTAX_LEXER_H

namespace starbytes {
namespace Syntax {


struct SourcePos {
    unsigned line;
    unsigned startCol;
    unsigned endCol;
};

struct Tok {
    typedef enum : int {
        Identifier,
        Number,
        Keyword,
        TemplateBegin,
        OpenParen,
        CloseParen,
        OpenBracket,
        CloseBracket,
        OpenBrace,
        CloseBrace,
        Comma,
        Colon,
        QuestionMark,
        LogicAND,
        LogicOR,
        BitwiseAND,
        BitwiseOR,
        Asterisk,
        FSlash,
        Plus,
        Minus,
        Equal,
        EqualEqual,
        PlusEqual,
        MinusEqual,
        Dot,
        LineCommentBegin,
        LineCommentEnd,
        BlockCommentBegin,
        BlockCommentEnd,
        EndOfFile
    } TokType;
    TokType type;
    std::string content;
    SourcePos srcPos;
};

class Lexer {
    char buffer[150];
    DiagnosticBufferedLogger & errStream;
public:
    Lexer(DiagnosticBufferedLogger & errStream);
    void tokenizeFromIStream(std::istream & in,std::vector<Tok> & tokStreamRef);
};

};
}

#endif
