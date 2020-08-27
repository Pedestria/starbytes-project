// CPlusPlus.cpp : Defines the entry point for the application.
//

#include "Starbytes.h"
 
using namespace std;
using namespace Starbytes;

string convertPosition(DocumentPosition pos){
	return string("{Line:" +to_string(pos.line)+" ,Start:"+to_string(pos.start)+" ,End:"+to_string(pos.end)+"}");
}

int main(int argc, char* argv[]) {

	string test = "import myLib\nscope MyProgram {\nclass Hello {\nmyMethod(a:String,b:Integer) >> Integer {} \n}\n \n}";

	auto result = Lexer(test).tokenize();

	for (Token tok : result) {
		cout << "Content:"+tok.getContent() << "\t" << "Type: " << int(tok.getType()) << "\t" << "Position:"+convertPosition(tok.getPosition());
	}

	return 0;
}



