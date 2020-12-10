//
#include "Starbytes.h"
#include <algorithm>
#include <fstream>
#include <string>
#include "starbytes/Interpreter/BCReader.h"


//Commeny Break;
#ifdef _WIN32 
WINDOWS_CONSOLE_INIT
#endif

using namespace std;
using namespace Starbytes;
/*Serializes Position into String*/

string help(){
	return "\n \u001b[35m\u001b[4mThe Starbytes BC Interpreter ("+STARBYTES_PROJECT_VERSION+")\u001b[0m \n \n \u001b[4mFlags:\u001b[0m \n \n --help = Display help information \n --execute ";
}
// Wraps ANSI Escape Code around message!
string createStarbytesError(string message,string escc = RED){
	return "\x1b["+escc+message+"\x1b[0m";
}

struct StarbytesCoreSettings {
	Foundation::Optional<std::string> executable_input;
} settings;

// Foundation::CommandOption interpret_only {"no-project","N",[]{
// 	if(settings.hasProjectFile){
// 		cerr << "\x1b[31mSyntaxError:\n" << "Command Line Args Parse Error:\n\nProject file will be dropped!" << "\x1b[0m";
// 		exit(1);
// 	}
// }};


Foundation::CommandInput executable_input {"execute","e",[](std::string & _file){
	if(!settings.executable_input.hasVal())
		settings.executable_input = _file;
}};

int main(int argc,char *argv[]){

	#ifdef _WIN32 
		setupConsole();
	#endif

	Foundation::parseCmdArgs(argc,argv,{},{&executable_input},[]{
		cout << help();
		exit(0);
	});
	if(settings.executable_input.hasVal()) {
		std::ifstream INPUT (settings.executable_input.value());
	}
	else {
		std::cerr << ERROR_ANSI_ESC << "No input module\nExiting..." << ANSI_ESC_RESET;
	};

	return 0;
}



