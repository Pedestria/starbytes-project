
#include <string>
#include "starbytes/Base/Document.h"
#include "starbytes/Base/Base.h"

#ifndef PARSER_TOKEN_H
#define PARSER_TOKEN_H

STARBYTES_STD_NAMESPACE
	TYPED_ENUM TokenType:int {
		Keyword, Identifier, Operator,Comma,Typecast,DoubleOperator,Quote,Dot,Semicolon,Numeric,
		EndOfFile,OpenBrace,CloseBrace,OpenBracket,CloseBracket,OpenParen,CloseParen,OpenCarrot,
		CloseCarrot,Comment,LineCommentDBSlash,BlockCommentStart,BlockCommentEnd,TemplateBegin
	};
	class Token {
		private:
			TokenType type;
			std::string content;
			DocumentPosition pos;
			unsigned int size;
		public:
			Token(TokenType type, std::string & content, DocumentPosition &position,size_t & size) : type(type), content(content),pos(position),size(size) {};
			~Token(){};
			std::string & getContent();
			TokenType & getType();
			DocumentPosition & getPosition();
			unsigned int & getTokenSize(){
				return size;
			};
	};
NAMESPACE_END

#endif