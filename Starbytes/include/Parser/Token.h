#pragma once
#include <string>
#include "AST/Document.h"

namespace Starbytes {
	enum class TokenType:int {
		Keyword, Identifier, Operator,Comma,Typecast,DoubleOperator,Quote,Dot,Semicolon,Numeric,EndOfFile,OpenBrace,CloseBrace,OpenBracket,CloseBracket,OpenParen,CloseParen,OpenCarrot,CloseCarrot,Comment,LineCommentDBSlash,BlockCommentStart,BlockCommentEnd
	};
	class Token
	{
	private:
		TokenType type;
		std::string content;
		DocumentPosition pos;
	public:
		Token(TokenType type, std::string content, DocumentPosition &position) : type(type), content(content),pos(position) {};
		std::string getContent();
		TokenType getType();
		DocumentPosition getPosition();
	};
}