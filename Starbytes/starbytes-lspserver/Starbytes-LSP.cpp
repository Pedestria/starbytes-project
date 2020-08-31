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
 return "The Starbytes LSP Implementation! \n \n Flags: \n \n --help = Display help info.";
}

int main(int argc, char* argv[]) {
    
    cout << help();
}