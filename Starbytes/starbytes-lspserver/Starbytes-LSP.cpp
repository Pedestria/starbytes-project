#include "Starbytes-LSP.h"
#include "JSONOutput.h"
#include <array>
#include <cctype>
#include <sstream>
#include <string>
#include <array>
#include <vector>

using namespace std;

string help(){
 return "\n \u001b[35m\u001b[4m The Starbytes LSP Implementation!\u001b[0m \n \n \u001b[4mFlags:\u001b[0m \n \n --help = Display help info. \n";
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
    if(!parseArguments(argv,argc)){
        return 0;
    }
    else {
        LSP::Messenger::getRequest();
        // LSP::LSPServerReply reply;
        // reply.str_result = "RESULT!";
        // LSP::Messenger::reply(reply,1);
    }
    // cout << help();
}