//
#include "Starbytes.h"
#include <algorithm>
#include <stdio.h>
#include <fstream>
#include <string>
#include "starbytes/Base/Base.h"
#include "starbytes/Parser/Token.h"


//Commeny Break;
#ifdef _WIN32 
WINDOWS_CONSOLE_INIT
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

struct StarbytesCoreSettings {
	bool hasProjectFile = false;
	string project_file;
	bool hasSources;
	vector<string> files;
};

StarbytesCoreSettings settings;

Foundation::CommandOption interpret_only {"no-project","N",[]{
	if(settings.hasProjectFile){
		cerr << "\x1b[31mSyntaxError:\n" << "Command Line Args Parse Error:\n\nProject file will be dropped!" << "\x1b[0m";
		exit(1);
	}
}};

Foundation::CommandInput project_file {"project-file","p",[](std::string p_file){
	settings.hasProjectFile = true;
	settings.project_file = p_file;
}};

Foundation::CommandInput source {"source","s",[](std::string source_file){
	if(!settings.hasSources){
		settings.hasSources = true;
	}
	settings.files.push_back(source_file);
}};


int main(int argc,char *argv[]){

	#ifdef _WIN32 
		setupConsole();
	#endif

	Foundation::parseCmdArgs(argc,argv,{},{&project_file,&source},[]{
		cout << help();
		exit(0);
	});

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
			// cout << help();
		// cout << loghelp() << "\n";

	return 0;
}



