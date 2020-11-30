#include <string>
#include <ctype.h>
#include "starbytes/Parser/Token.h"
#include "starbytes/Parser/Lexer.h"
#include <iostream>
#include "starbytes/Parser/Lookup.h"

using namespace Starbytes;

bool isBracket(char &c) {
	if (c == ']' || c == '[') {
		return true;
	}
	else {
		return false;
	}
}

bool isBrace(char &c) {
	if(c == '{' || c == '}'){
		return true;
	} else{
		return false;
	}
}

bool isOperator(char &c) {
	if (c == '=' || c == '+' || c == '-' || c== '&' || c == '|') {
		return true;
	}
	else {
		return false;
	}
}

bool isQuote(char &c){
	if(c == '\''||c == '"'||c == '\"'){
		return true;
	} else {
		return false;
	}
}

bool isTypecast(char &c) {
	if (c == ':') {
		return true;
	}
	else {
		return false;
	}
}

bool isParen(char &c) {
	if (c == '(' || c == ')') {
		return true;
	}
	else {
		return false;
	}
}

bool isCarrot(char &c){
	if(c == '<'||c == '>'){
		return true;
	} else{
		return false;
	}
}

bool isSlash(char &c){
	if(c == '/'){
		return true;
	}
	else{
		return false;
	}
}

bool isAt(char &c){
	return c == '@';
};

LookupArray<std::string> keywordLookup = {"import","scope","func","lazy","interface","class","struct","return","if","else","alias","deftype","immutable","decl","extends","utilizes","new","loose","enum","for","while","catch","secure"};

bool isKeyword(std::string &subject){
	return keywordLookup.lookup(subject);
}

bool isNumeric(std::string &subject){
	for(int i = 0;i < subject.length();++i){
		if(!isdigit(subject[i])){
			return false;
		}
	}
	return true;

}


std::string Lexer::saveTokenBuffer(size_t size) {
	std::string result;
	result.append(TokenBuffer, size);
	return result;
}
/*Copies token from buffer, clears buffer, and resets pointer.*/
void Lexer::resolveTokenAndClearCache(TokenType tokT) {
	size_t size = bufptr - start;
	if (bufptr == TokenBuffer) {
		return;
	}
	std::string result = saveTokenBuffer(size);
	if (isKeyword(result)) {
		tokT = TokenType::Keyword;
	}
	else if(tokT != TokenType::Numeric && isNumeric(result)){
		tokT = TokenType::Numeric;
	}


	DocumentPosition position;
	position.line = line;
	position.start = column-size;
	position.end = column-1;
	position.raw_index = raw_index-size;


	tree.push_back(Token(tokT,result,position,size));
	bufptr = TokenBuffer;
	start = bufptr;
}

void Lexer::tokenize() {
	bufptr = TokenBuffer;
	char c = code[0];
	start = bufptr;
	while (true) {
		if (c == '\n') {
			resolveTokenAndClearCache();
			column = 0;
			++raw_index;
			++line;
		}
		else if (isspace(c)) {
			resolveTokenAndClearCache();
		}
		else if(c == '.'){
			resolveTokenAndClearCache();
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache();
		}
		else if(isAt(c)){
			*bufptr = c;
			++bufptr;
			char & a = lookAhead();
			if(a == '['){
				++currentIndex;
				*bufptr = a;
				++bufptr;
				resolveTokenAndClearCache(TokenType::TemplateBegin);
			}
		}
		else if (isalnum(c)) {
			*bufptr = c;
			++bufptr;
			char & a = lookAhead();
			if(!isalnum(a))
				resolveTokenAndClearCache();
		}
		else if (isQuote(c)){
			//Essesntially String Literal!
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache(TokenType::Quote);
			char a;
			while(true){
				a = nextChar();
				if(a == c){
					resolveTokenAndClearCache();
					*bufptr = a;
					++bufptr;
					resolveTokenAndClearCache(TokenType::Quote);
					break;
				} else{
					*bufptr = a;
					++bufptr;
				}
			}
		}
		else if (isBracket(c)) {
			TokenType type;
			if(c == '['){
				type = TokenType::OpenBracket;
			} else if(c == ']'){
				type = TokenType::CloseBracket;
			}
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache(type);
		}
		else if(isBrace(c)){
			TokenType type;
			if(c == '{'){
				type = TokenType::OpenBrace;
			} else {
				type = TokenType::CloseBrace;
			}
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache(type);
		}
		else if (isOperator(c)){
			//If look ahead char is operator
			char nc = lookAhead();
			if (isOperator(nc)) {
				*bufptr = c;
				++bufptr;
				*bufptr = nextChar();
				++bufptr;
				resolveTokenAndClearCache(TokenType::DoubleOperator);

			}
			else {
				*bufptr = c;
				++bufptr;
				resolveTokenAndClearCache(TokenType::Operator);
			}

		}
		else if (isTypecast(c)) {
			// Check to see if tokenbuffer is empty and look behind.
			if (TokenBuffer != bufptr && isalnum(*(bufptr - 1))) {
				resolveTokenAndClearCache();
			}
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache(TokenType::Typecast);

		}
		else if (isParen(c)) {
			// Check to see if tokenbuffer is empty and look behind.
			if (TokenBuffer != bufptr && isalnum(*(bufptr-1))) {
				resolveTokenAndClearCache();
			}
			TokenType type;
			if(c == '('){
				type = TokenType::OpenParen;
			}else if (c == ')'){
				type = TokenType::CloseParen;
			}
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache(type);
		}
		else if(isCarrot(c)){
			resolveTokenAndClearCache();
			TokenType type;
			if(c == '<'){
				type = TokenType::OpenCarrot;
			}else if(c == '>'){
				type = TokenType::CloseCarrot;
			}
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache(type);
		}
		else if (c == ',') {
			if(TokenBuffer != bufptr && isalnum(*(bufptr-1))){
				resolveTokenAndClearCache();
			}
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache(TokenType::Comma);
		}
		else if(isSlash(c)){
			//Comments!
			resolveTokenAndClearCache(TokenType::LineCommentDBSlash);
			*bufptr = c;
			++bufptr;
			c = nextChar();
			if(isSlash(c)){
				*bufptr = c;
				++bufptr;
				while(true){
					if(c == '\n'){
						resolveTokenAndClearCache(TokenType::Comment);
						column = 0;
						++line;
						break;
					}
					else{
						*bufptr = c;
						++bufptr;
					}
					c = nextChar();
				}
			}
			else if(c == '*'){
				*bufptr = c;
				++bufptr;
				resolveTokenAndClearCache(TokenType::BlockCommentStart);
				while(true){
					if(c == '\n'){
						resolveTokenAndClearCache(TokenType::Comment);
						column = 0;
						++line;
					}
					else if(c == '*'){
						resolveTokenAndClearCache(TokenType::Comment);
						c = nextChar();
						*bufptr = c;
						++bufptr;
						if(c == '/'){
							*bufptr = c;
							++bufptr;
							resolveTokenAndClearCache(TokenType::BlockCommentEnd);
							break;
						}

					}
					else{
						*bufptr = c;
						++bufptr;
					}
					c = nextChar();
				}
			}
		}
		else if (c == '\0') {
			resolveTokenAndClearCache();
			*bufptr = c;
			++bufptr;
			resolveTokenAndClearCache(TokenType::EndOfFile);
			break;
		}

		c = nextChar();
	}
	
}
