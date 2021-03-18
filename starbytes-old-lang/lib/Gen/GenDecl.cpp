#include "GenDecl.h"
#include "GenExpr.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/ByteCode/BCDef.h"

STARBYTES_GEN_NAMESPACE

using namespace ByteCode;

ASTVisitorResponse genVarDecl(ASTTravelContext & context,CodeGenR *ptr){
    
    ASTVariableDeclaration * node = ASSERT_AST_NODE(context.current,ASTVariableDeclaration);
    //Visiting!
    BC code;
    for(auto & spec : node->specifiers){
        code = CRTVR;
        ptr->out.write((char *)&code,sizeof(code));
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
            BC code = STVR;
            ptr->out.write((char *)&code,sizeof(code));
            ptr->out.write((char *)&var_name,sizeof(var_name));
            //Wait For Initializer to Generate!
            genExpr(spec->initializer.value(),ptr);
        }
    }
    VISITOR_RETURN
};

ASTVisitorResponse genConstDecl(ASTTravelContext & context,CodeGenR * ptr){
    ASTConstantDeclaration * node = ASSERT_AST_NODE(context.current,ASTConstantDeclaration);
    //Visiting!
    BC code;
    for(auto & spec : node->specifiers){
        code = CRTVR;
        ptr->out.write((char *)&code,sizeof(code));
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
            BC code = STVR;
            ptr->out.write((char *)&code,sizeof(code));
            ptr->out.write((char *)&var_name,sizeof(var_name));
            //Wait For Initializer to Generate!
            genExpr(spec->initializer.value(),ptr);
        }
    }
    VISITOR_RETURN
};

NAMESPACE_END
