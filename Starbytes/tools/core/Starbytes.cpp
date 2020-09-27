//
#include "Starbytes.h"
#include <stdio.h>
#include <fstream>
#include <string>
#include "Base/Base.h"


//Commeny Break;

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

// Another Break;

// #include <nlohmann/json.hpp>

// using JSON = nlohmann::json;

using namespace std;
using namespace Starbytes;
/*Serializes Position into String*/
string convertPosition(DocumentPosition pos){
	return string("{Line:" +to_string(pos.line)+" ,Start:"+to_string(pos.start)+" ,End:"+to_string(pos.end)+"}");
}

string help(){
	return "\n \u001b[35m\u001b[4mThe Starbytes Compiler (v. 0.0.1a)\u001b[0m \n \n \u001b[4mFlags:\u001b[0m \n \n --help = Display help information \n --project-file = Directory to project file to build";
}
// Wraps ANSI Escape Code around message!
string createStarbytesError(string message,string escc = RED){
	return "\x1b["+escc+message+"\x1b[0m";
}

struct StarbytesArgs {
	bool hasModuleFile = false;
	string module_file;
	bool hasSources = false;
	vector<string> files;
};

StarbytesArgs * parseArguments(char * arguments[],int count){
	StarbytesArgs *arg = new StarbytesArgs();
    char ** flags = arguments;
    ++flags;
    vector<string> FLAGS;
    for(int i = 1;i < count;++i){
        FLAGS.push_back(string(*flags));
        ++flags;
    }
    int i = 0;
    while (i < FLAGS.size()) {
        if(FLAGS[i] == "--help"){
            cout << help();
            exit(0);
        } else if(FLAGS[i] == "--project-file"){
			arg->hasModuleFile = true;
			arg->module_file = FLAGS[++i];
		} else if(FLAGS[i] == "--source"){
			arg->hasSources = true;
			arg->files.push_back(FLAGS[++i]);
		}
		++i;
    }
    return arg;

    
}

int main(int argc, char* argv[]) {
	#ifdef _WIN32
	setupConsole();
	#endif

	using namespace Foundation;

	StarbytesArgs *args = parseArguments(argv,argc);
	if(args->hasModuleFile){
		cout << "Module File Location:" << args->module_file;
		cout << *(readFile(args->module_file));
	}
	//If we can test from source files!
	if(args->hasSources){
		for(auto srcfile : args->files){
			string *file_buf = readFile(srcfile);
			auto result = Lexer(*file_buf).tokenize();
			AbstractSyntaxTree * tree = new AbstractSyntaxTree();
			try {
				auto parser = Parser(result,tree);
				parser.convertToAST();

				for(auto node : tree->nodes){
					cout << "{\nType:" << int(node->type) << "\n BeginFold:"+convertPosition(node->BeginFold) << "\n EndFold:"+convertPosition(node->EndFold) << "\n}\n";
				}
			} catch (string message) {
				cerr << "\x1b[31mSyntaxError:\n" << message << "\x1b[0m";
			}
		}
	}
	else{
		string test = "import mylib\nimport otherLibrary\ndecl hello = [\"One\",\"Two\"]\ndecl immutable hellop:Array = [\"One\",\"Two\"]";
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
				cout << "{\nType:" << int(node->type) << "\n BeginFold:"+convertPosition(node->BeginFold) << "\n EndFold:"+convertPosition(node->EndFold) << "\n}\n";
			}
		} catch (string message) {
			cerr << "\x1b[31mSyntaxError:\n" << message << "\x1b[0m";
		}
	}
			// cout << help();
		// cout << loghelp() << "\n";

	return 0;
}



