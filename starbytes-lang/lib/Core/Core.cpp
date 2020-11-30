#include "starbytes/Core/Core.h"
#include "starbytes/Interpreter/BCReader.h"
#include "starbytes/Parser/Lexer.h"
#include "starbytes/Parser/Parser.h"
#include "starbytes/Semantics/Main.h"

STARBYTES_STD_NAMESPACE

using namespace std;

using namespace Starbytes;

string highlightCode(std::string & code,std::vector<Token> & tokens){
	int index = 0;
	size_t s = sizeof("\x1b[30m");
	for(auto tok : tokens){
		DocumentPosition & pos = tok.getPosition();
		string color_code;
		if(tok.getType() == Starbytes::TokenType::Keyword){
			color_code = PURPLE;
		} else if(tok.getType() == Starbytes::TokenType::Identifier){
			color_code = YELLOW;
		} else if(tok.getType() == Starbytes::TokenType::Operator){
			color_code = PURPLE;
		} else {
			color_code = "0m";
		}
		
		if(pos.raw_index == 0){
			cout << "StrLength:" << code.length();
			code = "\x1b["+color_code + code;
			cout << "start:" << pos.raw_index << " end:" << pos.raw_index+tok.getTokenSize()+(3*index)+3 << " ";
			++index;
			code.insert(pos.raw_index+tok.getTokenSize()+(3*index)+3,"\x1b[0m");
		}
		else if(tok.getType() != Starbytes::TokenType::EndOfFile){
			cout << "start:" << pos.raw_index+(3 * index)+3 << " end:" << pos.raw_index+tok.getTokenSize()+(3*index)+3 << " ";
			code.insert(pos.raw_index+(3* index)+3,"\x1b["+color_code);
			++index;
			if(pos.raw_index == code.size()-1){
				code.append("\x1b[0m");
			}
			else{
				code.insert(pos.raw_index+tok.getTokenSize()+(3*index)+3,"\x1b[0m");
			}
		}
		else if(pos.raw_index == code.size()-1){
			code.append("\x1b[0m");
		}
		++index;
	}
	cout << "New Size:" << code.size();
	return code;
}

void logError(std::string message,std::string code, std::vector<Token> &tokens){
	string result = highlightCode(code,tokens);
	std::cerr << message << "\n\n" << result << "\n";
}

AbstractSyntaxTree * parseCode(std::string & code){
	std::vector<Token> tok_stream;
	Lexer(code,tok_stream).tokenize();
	AbstractSyntaxTree *_ast = new AbstractSyntaxTree();
	try {
		Parser(tok_stream,_ast).convertToAST();
	}
	catch (std::string error) {
		std::cerr << error << std::endl;
	};
	return _ast;
};

NAMESPACE_END