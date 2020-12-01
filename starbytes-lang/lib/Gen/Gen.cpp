#include "starbytes/Gen/Gen.h"
#include "GenDecl.h"

STARBYTES_GEN_NAMESPACE

using namespace AST;

ASTTravelerCallbackList<CodeGenR> callback_list = {
    Foundation::dict_vec_entry(ASTType::VariableDeclaration,&genVarDecl)
};

CodeGenR::CodeGenR(std::ofstream & _output):ASTTraveler<CodeGenR>(this,callback_list),out(_output){
  
};

unsigned & CodeGenR::flushArgsCount(){
    return bc_args_count;
};

void CodeGenR::_generateAST(AST::AbstractSyntaxTree *& src){
   travel(src);
};

void generateToBCProgram(std::vector<AST::AbstractSyntaxTree *> &module_sources,std::ofstream & out){
    CodeGenR gen(out);
    for(auto & src : module_sources){
        gen._generateAST(src);
    }
};

NAMESPACE_END