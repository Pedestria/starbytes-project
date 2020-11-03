#include "starbytes/Gen/Gen.h"
#include "GenDecl.h"

STARBYTES_GEN_NAMESPACE

using namespace AST;

CodeGenR::CodeGenR(std::vector<AST::AbstractSyntaxTree *> & m_srcs):module_sources(m_srcs){
    result = new ByteCode::BCProgram();
};

void CodeGenR::_pushNodeToOut(ByteCode::BCUnit *unit){
    result->units.push_back(unit);
};

#define VISIT_NODE(node,_node_ptr) visit##node(ASSERT_AST_NODE(_node_ptr,node),this)

void CodeGenR::_generateAST(AST::AbstractSyntaxTree *& src){
    for(auto & node : src->nodes){
        if(AST_NODE_IS(node,ASTFunctionDeclaration)){
            VISIT_NODE(ASTFunctionDeclaration,node);
        }
        else if(AST_NODE_IS(node,ASTClassDeclaration)){
            VISIT_NODE(ASTClassDeclaration, node);
        }
    }
};

#undef VISIT_NODE

ByteCode::BCProgram *& CodeGenR::generate(){
    //TODO Generate!
    for(auto & src : module_sources){
        _generateAST(src);
    }
    return result;
};

ByteCode::BCProgram * generateToBCProgram(std::vector<AST::AbstractSyntaxTree *> &module_sources){
    return CodeGenR(module_sources).generate();
};

NAMESPACE_END