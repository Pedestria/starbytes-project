
#include <string>
#include <vector>
#include "starbytes/Base/Base.h"
#include "Token.h"

#ifndef PARSER_LEXER_H
#define PARSER_LEXER_H


STARBYTES_STD_NAMESPACE
	class Lexer {
	public:
		Lexer(std::string _code) : code(_code),currentIndex(0),TokenBuffer(),line(1), bufptr(), start(),column(0),raw_index(0) {}
		/*Move to next char in code*/
		char & nextChar() {
			++column;
			++raw_index;
			return code[++currentIndex];
		};
		/*Gets next char without mutating currentindex*/
		char & lookAhead() {
			return code[currentIndex + 1];
		};
		void resolveTokenAndClearCache(TokenType tokT = TokenType::Identifier);
		std::vector<Token> tokenize();
		std::string saveTokenBuffer(size_t size);
	private:
		char TokenBuffer[500];
		char * bufptr;
		int currentIndex;
		int raw_index;
		int line;
		int column;
		char* start;
		std::string code;
		std::vector<Token> tree;
	};
NAMESPACE_END

#endif