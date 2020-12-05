#include "GenDecl.h"
#include "GenExpr.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/ByteCode/BCDef.h"

STARBYTES_GEN_NAMESPACE

using namespace ByteCode;

ASTVisitorResponse genVarDecl(ASTTravelContext & context,CodeGenR *ptr){
    
    ASTVariableDeclaration * node = ASSERT_AST_NODE(context.current,ASTVariableDeclaration);
    //Visiting!
    std::cout << "Generating Var Decl!" << std::endl;
    int code;
    for(auto & spec : node->specifiers){
        std::cout << "Generating Var Spec" << std::endl;
        code = CRTVR;
        ptr->out.write((char *)&code,sizeof(code));
        std::cout << "Did not Find the Problem!" << std::endl;
        BCId var_name;
        if(AST_NODE_IS(spec->id,ASTIdentifier)){
            ASTIdentifier *node = ASSERT_AST_NODE(spec->id,ASTIdentifier);
            var_name = node->value;
            ptr->out.write((char *)&var_name,sizeof(var_name));
        }
        else if(AST_NODE_IS(spec->id,ASTTypeCastIdentifier)){
            ASTTypeCastIdentifier *node = ASSERT_AST_NODE(spec->id,ASTTypeCastIdentifier);
            var_name = node->id->value;
            ptr->out.write((char *)&var_name,sizeof(var_name));
        }
        if(spec->initializer.has_value()){
            int code = STVR;
            ptr->out.write((char *)&code,sizeof(code));
            ptr->out.write((char *)&var_name,sizeof(code));
            //Wait For Initializer to Generate!
            genExpr(spec->initializer.value(),ptr);
        }
    }
    VISITOR_RETURN
};

ASTVisitorResponse genConstDecl(ASTTravelContext & context,CodeGenR * ptr){
    ASTConstantDeclaration * node = ASSERT_AST_NODE(context.current,ASTConstantDeclaration);
    //Visiting!
    std::cout << "Generating Const Decl!" << std::endl;
    int code;
    for(auto & spec : node->specifiers){
        std::cout << "Generating Const Spec" << std::endl;
        code = CRTVR;
        ptr->out.write((char *)&code,sizeof(code));
        std::cout << "Did not Find the Problem!" << std::endl;
        BCId var_name;
        if(AST_NODE_IS(spec->id,ASTIdentifier)){
            ASTIdentifier *node = ASSERT_AST_NODE(spec->id,ASTIdentifier);
            var_name = node->value;
            ptr->out.write((char *)&var_name,sizeof(var_name));
        }
        else if(AST_NODE_IS(spec->id,ASTTypeCastIdentifier)){
            ASTTypeCastIdentifier *node = ASSERT_AST_NODE(spec->id,ASTTypeCastIdentifier);
            var_name = node->id->value;
            ptr->out.write((char *)&var_name,sizeof(var_name));
        }
        if(spec->initializer.has_value()){
            int code = STVR;
            ptr->out.write((char *)&code,sizeof(code));
            ptr->out.write((char *)&var_name,sizeof(code));
            //Wait For Initializer to Generate!
            genExpr(spec->initializer.value(),ptr);
        }
    }
    VISITOR_RETURN
};

NAMESPACE_END