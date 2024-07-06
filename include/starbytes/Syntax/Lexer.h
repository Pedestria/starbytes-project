#include <istream>
#include <vector>
#include "starbytes/Base/Diagnostic.h"
#include "Toks.def"

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
        BooleanLiteral,
        StringLiteral,
        NumericLiteral,
        FloatingNumericLiteral,
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
    DiagnosticHandler & errStream;
public:
    Lexer(DiagnosticHandler & errStream);
    void tokenizeFromIStream(std::istream & in,std::vector<Tok> & tokStreamRef);
};

}
}


// namespace llvm {
//     template<>
//     struct format_provider<starbytes::Syntax::Tok> {
//         static void format(const starbytes::Syntax::Tok &V,raw_ostream &Stream, StringRef Style){
//             std::string tokTypeName;
//             using starbytes::Syntax::Tok;
//             switch (V.type) {
//                 case Tok::Keyword : {
//                     tokTypeName = "Keyword";
//                     break;
//                 }
//                 case Tok::Identifier: {
//                     tokTypeName = "Identifier";
//                     break;
//                 }
//                 case Tok::OpenBrace : {
//                     tokTypeName = "OpenBrace";
//                     break;
//                 }
//                 case Tok::CloseBrace : {
//                     tokTypeName = "CloseBrace";
//                     break;
//                 }
//                 case Tok::Asterisk : {
//                     tokTypeName = "Asterisk";
//                     break;
//                 }
//                 case Tok::Colon : {
//                     tokTypeName = "Colon";
//                     break;
//                 }
//                 case Tok::Comma : {
//                     tokTypeName = "Comma";
//                     break;
//                 }
//                 default: {
//                     tokTypeName = "N/A";
//                     break;
//                 }
//             }

//             Stream << formatv(
//                 "Syntax::Tok : {\n" 
//                 "   type: {0}\n"
//                 "   content:{1}\n"
//                 "}"
//             ,tokTypeName,V.content);
//         };
//     };
// }

#endif
