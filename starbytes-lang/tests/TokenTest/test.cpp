#include "starbytes/Parser/Lexer.h"
#include "starbytes/Base/Base.h"

#ifdef _WIN32
WINDOWS_CONSOLE_INIT
#endif

using namespace Starbytes;

std::string file_path;

Foundation::CommandInput file ("file","f",[](std::string & s){
    file_path = s;
});

int main(int argc,char * argv[]){

    #ifdef _WIN32
    setupConsole();
    #endif

    Foundation::parseCmdArgs(argc,argv,{},{&file},[](){
        std::cout << "Token Test!\n\nInput a file to test via flag -> -f or --file" << std::endl;
    });

    if(file_path.empty()) {
        std::cerr << ERROR_ANSI_ESC << "Error: No input file!\nExiting!  " << ANSI_ESC_RESET << std::endl;
        exit(1);
    }
    else {
        std::string * file_buf = Foundation::readFile(file_path);
        std::vector<Token> toks;
        Lexer(*file_buf,toks).tokenize();
        std::cout << "Token Count: " << toks.size() << std::endl << "Lexer Result:" << std::endl;
        auto count = 1;
        for(auto & tok : toks){
            std::cout << "Tok " << count << " {\nContent:" << tok.getContent() << "\nType:" << int(tok.getType()) << "\nPos:" << document_position_to_str(tok.getPosition()) << "\n}" << std::endl;
            ++count;
        }
    }

    return 0;

};