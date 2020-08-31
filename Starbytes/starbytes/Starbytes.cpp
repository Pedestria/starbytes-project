// CPlusPlus.cpp : Defines the entry point for the application.
//
#include "Starbytes.h"
#include "AST.h"
#include <stdio.h>


// #include <nlohmann/json.hpp>

// using JSON = nlohmann::json;

using namespace std;
using namespace Starbytes;
/*Serializes Position into String*/
string convertPosition(DocumentPosition pos){
	return string("{Line:" +to_string(pos.line)+" ,Start:"+to_string(pos.start)+" ,End:"+to_string(pos.end)+"}");
}

int main(int argc, char* argv[]) {
	// cout << loghelp() << "\n";
	// printf("\u001b[40m[30mInfo:\u001b[0m");

	string test = "import mylib\nimport otherLibrary\ndecl hello = \"A String!\"\nfunc hello (hello:String,moma:String) >> String {\n \n}";
	string test2 = "import library\nimport otherLibrary";
	auto result = Lexer(test).tokenize();

	// for (Token tok : result) {
	// 	cout << "Content:"+tok.getContent() << "\t" << "Type: " << int(tok.getType()) << "\t" << "Position:"+convertPosition(tok.getPosition());
	// }
	AbstractSyntaxTree *tree = new AbstractSyntaxTree();

	try {
		auto parser = Parser(result,tree);
		parser.convertToAST();

		for(auto node : tree->nodes){
			cout << "{Type:" << int(node->type) << "\n BeginFold:"+convertPosition(node->BeginFold) << "\n EndFold:"+convertPosition(node->EndFold) << "\n}\n";
		}
	} catch (string message) {
		cerr << "\x1b[31mSyntaxError:\n" << message << "\x1b[0m";
	}



	return 0;
}



