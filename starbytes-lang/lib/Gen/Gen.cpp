#include "starbytes/Gen/Gen.h"
#include "GenDecl.h"
#include "starbytes/ByteCode/BCDef.h"

STARBYTES_GEN_NAMESPACE

using namespace AST;

ASTTravelerCallbackList<CodeGenR> callback_list = {
    Foundation::dict_vec_entry(ASTType::VariableDeclaration,&genVarDecl)
};

CodeGenR::CodeGenR(std::ofstream & _output):ASTTraveler<CodeGenR>(this,callback_list),out(_output){
  
};

void CodeGenR::_generateAST(AST::AbstractSyntaxTree *& src){
   travel(src);
};

void generateToBCProgram(Foundation::ArrRef<AbstractSyntaxTree *> module_sources,std::ofstream & out,CodeGenROpts & opts){
    CodeGenR gen(out);
    ByteCode::BC code = PROG_END;
    if(out.is_open()) {
        for(auto & src : module_sources){
            gen._generateAST(src);
            out.write((char *)&code,sizeof(code));
        }
    }
    out.close();
};

NAMESPACE_END
