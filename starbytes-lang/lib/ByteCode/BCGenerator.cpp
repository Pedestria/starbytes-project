#include "starbytes/ByteCode/BCGenerator.h"

STARBYTES_BYTECODE_NAMESPACE

using namespace AST;

class BCCreator {
    public:
        BCCreator(){};
        ~BCCreator(){};
        void createByteCode();
    private:

};

void BCCreator::createByteCode(){

};


class BCGenerator {
    private:  
        std::vector<AbstractSyntaxTree *> & trees;
        BCProgram * out;
    public:
        BCGenerator(std::vector<AbstractSyntaxTree *> & __trees):trees(__trees){};
        ~BCGenerator(){};
        void generate(BCProgram *& Out);
};

void generateImportDeclaration(BCGenerator *gen,ASTImportDeclaration *node){
    return;
};
void generateVariableDeclaration(BCGenerator *gen,ASTVariableDeclaration *node){

};

void generateStatement(BCGenerator *gen,ASTStatement *node){
    if(AST_NODE_IS(node,ASTImportDeclaration)){
        generateImportDeclaration(gen,ASSERT_AST_NODE(node,ASTImportDeclaration));
    }
    else if(AST_NODE_IS(node,ASTFunctionDeclaration)){
        generateVariableDeclaration(gen,ASSERT_AST_NODE(node,ASTVariableDeclaration));
    }
};



// void BCGenerator::generate(BCProgram *&Out){
//     out = Out;
//     for(auto n : AST->nodes){
//         generateStatement(this,n);
//     }
// }




BCProgram * generateToBCProgram(std::vector<AbstractSyntaxTree *> & module_sources){
    BCProgram *result = new BCProgram();
    return result;
};

NAMESPACE_END