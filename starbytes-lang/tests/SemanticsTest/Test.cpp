#include "starbytes/Semantics/Main.h"
#include "starbytes/Parser/Lexer.h"
#include "starbytes/Parser/Parser.h"
#include "starbytes/AST/AST.h"

int main (){
    using Starbytes::Lexer;
    using Starbytes::Parser;
    using Starbytes::AST::AbstractSyntaxTree;
    using Starbytes::Semantics::SemanticA;
    std::string testcode = "import mylib\nimport otherLibrary\ndecl var:String\ndecl anotherVar:String";
    auto toks = Lexer(testcode).tokenize();
    try {
        AbstractSyntaxTree * tree = new AbstractSyntaxTree();
        Parser(toks,tree).convertToAST();
        SemanticA(tree).initialize();
    }
    catch(std::string error){
        std::cerr << "SyntaxError:\n" << error;
        exit(1);
    }
    return 0;
    // an.initialize();
}