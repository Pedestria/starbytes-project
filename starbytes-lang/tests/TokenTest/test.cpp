#include "starbytes/Parser/Lexer.h"
#include "starbytes/Base/Base.h"

#ifdef _WIN32
WINDOWS_CONSOLE_INIT
#endif

using namespace Starbytes;

Foundation::Optional<std::string> file_path;

using namespace Foundation;

CommandLine::CommandInput file ("file","f",CommandLine::FlagDescription("A file to test!"),[](std::string & s){
    file_path = s;
});

int main(int argc,char * argv[]){

    #ifdef _WIN32
    setupConsole();
    #endif

    CommandLine::parseCmdArgs(argc,argv,{},{&file},"Token Test!\n\nInput a file to test via flag -> -f or --file");

    if(!file_path.hasVal()) {
        std::cerr << ERROR_ANSI_ESC << "Error: No input file!\nExiting!  " << ANSI_ESC_RESET << std::endl;
        exit(1);
    }
    else {
        std::unique_ptr<std::string> file_buf = Foundation::readFile(file_path.value());
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