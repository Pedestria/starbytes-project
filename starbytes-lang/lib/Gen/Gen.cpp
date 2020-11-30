#include "starbytes/Gen/Gen.h"
#include "GenDecl.h"

STARBYTES_GEN_NAMESPACE

using namespace AST;

ASTTravelerCallbackList<CodeGenR> callback_list = {};

CodeGenR::CodeGenR(std::vector<AST::AbstractSyntaxTree *> & m_srcs,std::ofstream & _output):ASTTraveler<CodeGenR>(this,callback_list),out(_output),module_sources(m_srcs){
  
};

unsigned & CodeGenR::flushArgsCount(){
    return bc_args_count;
};

void CodeGenR::_generateAST(AST::AbstractSyntaxTree *& src){
   travel(src);
};

NAMESPACE_END