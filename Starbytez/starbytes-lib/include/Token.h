#pragma once
#include <string>
#include "Document.h"

namespace Starbytes {
	enum class TokenType :int {
		Keyword, Identifier, Operator,Bracket,Paren,Comma,Typecast,DoubleOperator,Quote,Dot,Semicolon,Numeric
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