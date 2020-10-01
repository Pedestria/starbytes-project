#include "starbytes/Parser/Lexer.h"
#include "starbytes/Parser/Parser.h"
#include <fstream>
#include <iostream>
using namespace Starbytes;
int main(){
    std::fstream f;
    f.open("test.stb");
    std::string code;
    f >> code;
    auto tokens = Lexer(code).tokenize();
    try {
        AbstractSyntaxTree *tree = new AbstractSyntaxTree();
        Parser(tokens,tree).convertToAST();
    }
    catch (std::string message) {
        std::cout << "SyntaxError:" << message;
    }
    return 0;
}