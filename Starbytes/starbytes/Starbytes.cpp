// CPlusPlus.cpp : Defines the entry point for the application.
//
#include "Starbytes.h"
#include "Parser/AST.h"
#include <stdio.h>

#ifdef _WIN32
#include <Windows.h>
static HANDLE stdoutHandle;
static DWORD outModeInit;

void setupConsole(void){
    DWORD outMode = 0;
    stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if(stdoutHandle == INVALID_HANDLE_VALUE) {
		exit(GetLastError());
	}

    if(!GetConsoleMode(stdoutHandle, &outMode)) {
	    exit(GetLastError());
	}

    outModeInit = outMode;

    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if(!SetConsoleMode(stdoutHandle, outMode)) {
		exit(GetLastError());
	}
}
#endif



// #include <nlohmann/json.hpp>

// using JSON = nlohmann::json;

using namespace std;
using namespace Starbytes;
/*Serializes Position into String*/
string convertPosition(DocumentPosition pos){
	return string("{Line:" +to_string(pos.line)+" ,Start:"+to_string(pos.start)+" ,End:"+to_string(pos.end)+"}");
}

string help(){
	return "\u001b[35m\u001b[4mThe Starbytes Compiler (v. 0.0.1a)\u001b[0m \n \n\u001b[4mFlags:\u001b[0m \n \n--help = Display help information";
}

bool parseArguments(char * arguments[],int count){
    char ** flags = arguments;
    ++flags;
    vector<string> FLAGS;
    for(int i = 1;i < count;++i){
        FLAGS.push_back(string(*flags));
        ++flags;
    }
    
    for(auto f : FLAGS){
        if(f == "--help"){
            cout << help();
            return false;
        }
    }
    return true;

    
}

int main(int argc, char* argv[]) {
	#ifdef _WIN32
	setupConsole();
	#endif

	// cout << help();
	// cout << loghelp() << "\n";
	string test = "import mylib\nimport otherLibrary\ndecl hello = [\"One\",\"Two\"]\nfunc hello (hello:String,moma:String) >> String {\n \n}";
	string test2 = "import library\nimport otherLibrary";
	auto result = Lexer(test).tokenize();
	// // for (Token tok : result) {
	// // 	cout << "Content:"+tok.getContent() << "\t" << "Type: " << int(tok.getType()) << "\t" << "Position:"+convertPosition(tok.getPosition());
	// // }
	AbstractSyntaxTree *tree = new AbstractSyntaxTree();

	try {
		auto parser = Parser(result,tree);
		parser.convertToAST();

		for(auto node : tree->nodes){
			cout << "{\nType:" << int(node->type) << "\n BeginFold:"+convertPosition(node->BeginFold) << "\n EndFold:"+convertPosition(node->EndFold) << "\n}\n";
		}
	} catch (string message) {
		cerr << "\x1b[31mSyntaxError:\n" << message << "\x1b[0m";
	}
	return 0;
}



