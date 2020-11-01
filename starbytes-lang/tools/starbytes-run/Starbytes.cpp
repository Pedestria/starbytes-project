﻿//
#include "Starbytes.h"
#include <algorithm>
#include <fstream>
#include <string>
#include "starbytes/Interpreter/BCReader.h"
#include "starbytes/ByteCode/BCSerializer.h"


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
	return "\n \u001b[35m\u001b[4mThe Starbytes BC Interpreter ("+STARBYTES_PROJECT_VERSION+")\u001b[0m \n \n \u001b[4mFlags:\u001b[0m \n \n --help = Display help information \n --execute ";
}
// Wraps ANSI Escape Code around message!
string createStarbytesError(string message,string escc = RED){
	return "\x1b["+escc+message+"\x1b[0m";
}

struct StarbytesCoreSettings {
	bool hasExec = false;
	string executable_input;
};

StarbytesCoreSettings settings;

// Foundation::CommandOption interpret_only {"no-project","N",[]{
// 	if(settings.hasProjectFile){
// 		cerr << "\x1b[31mSyntaxError:\n" << "Command Line Args Parse Error:\n\nProject file will be dropped!" << "\x1b[0m";
// 		exit(1);
// 	}
// }};

Foundation::CommandInput executable_input {"execute","e",[](std::string & _file){
	if(!settings.hasExec){
		settings.hasExec = true;
		settings.executable_input = _file;
	}
}};

// Foundation::CommandInput source {"source","s",[](std::string source_file){
// 	if(!settings.hasSources){
// 		settings.hasSources = true;
// 	}
// 	settings.files.push_back(source_file);
// }};


int main(int argc,char *argv[]){

	#ifdef _WIN32 
		setupConsole();
	#endif

	Foundation::parseCmdArgs(argc,argv,{},{&executable_input},[]{
		cout << help();
		exit(0);
	});

	std::ifstream INPUT (settings.executable_input);
	ByteCode::BCProgram *program_to_exec = new ByteCode::BCProgram();
	ByteCode::deserializeBCProgram(INPUT,program_to_exec);
	Interpreter::execBCProgram(program_to_exec);

	return 0;
}


