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
        AbstractSyntaxTree *& AST;
        BCProgram * out;
    public:
        BCGenerator(AbstractSyntaxTree *& ast):AST(ast){};
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



void BCGenerator::generate(BCProgram *&Out){
    out = Out;
    for(auto n : AST->nodes){
        generateStatement(this,n);
    }
}




BCProgram * generateToBCProgram(AST::AbstractSyntaxTree *ast){
    BCProgram *result = new BCProgram();
    BCGenerator(ast).generate(result);
    return result;
};

NAMESPACE_END