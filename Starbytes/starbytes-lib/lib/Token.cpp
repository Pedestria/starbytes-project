#include "Token.h"

using namespace Starbytes;

std::string Token::getContent(){
	return content;
}
TokenType Token::getType() {
	return type;
}
DocumentPosition Token::getPosition(){
	return pos;
}
