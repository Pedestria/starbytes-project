#include "starbytes/Gen/Gen.h"

STARBYTES_GEN_NAMESPACE

using namespace AST;

ASTTravelerCallbackList<CodeGenR> callback_list = {};

CodeGenR::CodeGenR(std::vector<AST::AbstractSyntaxTree *> & m_srcs):ASTTraveler<CodeGenR>(this,callback_list),module_sources(m_srcs){
    result = new ByteCode::BCProgram();
};

void CodeGenR::_pushNodeToOut(ByteCode::BCUnit *unit){
    result->units.push_back(unit);
};

unsigned & CodeGenR::flushArgsCount(){
    return bc_args_count;
};

void CodeGenR::_generateAST(AST::AbstractSyntaxTree *& src){
   travel(src);
};

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