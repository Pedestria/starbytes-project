#pragma once

#include<string>
#include<vector>
#include "Token.h"

namespace Starbytes {
	class Lexer {
	public:
		Lexer(std::string _code) : code(_code),currentIndex(0),TokenBuffer(),line(1), bufptr(), start(),column(0) {}
		/*Move to next char in code*/
		char nextChar() {
			++column;
			return code[++currentIndex];
		};
		/*Gets next char without mutating currentindex*/
		char lookAhead() {
			return code[currentIndex + 1];
		};
		void resolveTokenAndClearCache(TokenType tokT = TokenType::Identifier);
		std::vector<Token> tokenize();
		std::string saveTokenBuffer(size_t size);
	private:
		char TokenBuffer[500];
		char * bufptr;
		int currentIndex;
		int line;
		int column;
		char* start;
		std::string code;
		std::vector<Token> tree;
	};
}