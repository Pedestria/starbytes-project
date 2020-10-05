//
#include "Starbytes.h"
#include <stdio.h>
#include <fstream>
#include <string>
#include "starbytes/Base/Base.h"
#include "starbytes/Parser/Token.h"


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

using namespace std;
using namespace Starbytes;
/*Serializes Position into String*/
string convertPosition(DocumentPosition pos){
	return string("{Line:" +to_string(pos.line)+",Start:"+to_string(pos.start)+",End:"+to_string(pos.end)+",RawIndex:"+to_string(pos.raw_index)+"}");
}

string help(){
	return "\n \u001b[35m\u001b[4mThe Starbytes Compiler (v. 0.0.1a)\u001b[0m \n \n \u001b[4mFlags:\u001b[0m \n \n --help = Display help information \n --project-file = Directory to project file to build";
}
// Wraps ANSI Escape Code around message!
string createStarbytesError(string message,string escc = RED){
	return "\x1b["+escc+message+"\x1b[0m";
}

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
		auto result = Lexer(test2).tokenize();
		// logError("TEST",test2,result);
		// for (Token tok : result) {
		// 	cout << "{\nContent:"+tok.getContent() << "\nType: " << int(tok.getType()) << "\nPosition:"+convertPosition(tok.getPosition()) << "\nSize:" <<tok.getTokenSize() << "\n}\n";
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



