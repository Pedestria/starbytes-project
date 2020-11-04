#include "GenDecl.h"
#include "starbytes/Gen/Gen.h"
#include "MakeByteCode.h"

STARBYTES_GEN_NAMESPACE

std::string crtvr = "crtvr";
std::string stvr = "stvr";


void visitASTClassDeclaration(ASTClassDeclaration *_node,CodeGenR * _gen){
    
};

void visitASTFunctionDeclaration(ASTFunctionDeclaration *_node,CodeGenR * _gen){

};

void visitASTVariableDeclaration(ASTVariableDeclaration *_node,CodeGenR * _gen){
    for(auto & _spec : _node->specifiers){
        std::string var_name;
        //Create First BC Method Code!
        _gen->_pushNodeToOut(make_bc_code_begin(crtvr));
            if(AST_NODE_IS(_node,ASTIdentifier))
               var_name = ASSERT_AST_NODE(_node,ASTIdentifier)->value;
            else if (AST_NODE_IS(_node,ASTTypeCastIdentifier))
               var_name = ASSERT_AST_NODE(_node,ASTTypeCastIdentifier)->id->value;

            _gen->_pushNodeToOut(make_bc_string(var_name));

        _gen->_pushNodeToOut(make_bc_code_end(crtvr));
        //Create Set Var BC Method Code only if specifier has an initial value
        if(_spec->initializer.has_value()){
            _gen->_pushNodeToOut(make_bc_code_begin(stvr));
            _gen->_pushNodeToOut(make_bc_string(var_name));
            //Visit Expression!
            _gen->_pushNodeToOut(make_bc_code_end(stvr));
        }
    }
};


NAMESPACE_END