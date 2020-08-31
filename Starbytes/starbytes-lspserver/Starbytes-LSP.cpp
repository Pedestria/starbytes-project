#include "Starbytes-LSP.h"
#include <array>
#include <string>

using namespace std;

// array<string,2> ARGS = {"--help",""};
// void parseArguments(char * args[]){
//     ++args;
//     while(true){
//         if(args){
//             if(ARGS[0].find(*args)){
//                 return;
//             }
//             ++args;
//         }
//         else{
//             break;
//         }
//     }
// }

std::string help(){
 return "\n \u001b[35m\u001b[4m The Starbytes LSP Implementation!\u001b[0m \n \n \u001b[4mFlags:\u001b[0m \n \n --help = Display help info. \n";
}

int main(int argc, char* argv[]) {
    
    cout << help();
}