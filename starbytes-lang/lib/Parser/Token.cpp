#include "starbytes/Parser/Token.h"

STARBYTES_STD_NAMESPACE

std::string & Token::getContent(){
	return content;
}
Starbytes::TokenType & Token::getType() {
	return type;
}
DocumentPosition & Token::getPosition(){
	return pos;
}

NAMESPACE_END
