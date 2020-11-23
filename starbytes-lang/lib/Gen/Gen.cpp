#include "starbytes/Gen/Gen.h"

STARBYTES_GEN_NAMESPACE

using namespace AST;

CodeGenR::CodeGenR(std::vector<AST::AbstractSyntaxTree *> & m_srcs):module_sources(m_srcs){
    result = new ByteCode::BCProgram();
};

void CodeGenR::_pushNodeToOut(ByteCode::BCUnit *unit){
    result->units.push_back(unit);
};

unsigned & CodeGenR::flushArgsCount(){
    return bc_args_count;
};

void CodeGenR::_generateAST(AST::AbstractSyntaxTree *& src){
   
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