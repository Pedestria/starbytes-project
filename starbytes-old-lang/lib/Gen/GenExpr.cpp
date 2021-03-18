#include "GenExpr.h"
#include "starbytes/Gen/Gen.h"
#include "starbytes/ByteCode/BCDef.h"

STARBYTES_GEN_NAMESPACE

using namespace ByteCode;

void genStrLiteral(ASTStringLiteral * node,CodeGenR *ptr){
    // ASTStringLiteral * node = ASSERT_AST_NODE(context.current,ASTStringLiteral);
    BC code = CRT_STB_STR;
    ptr->out.write((char *)&code,sizeof(code));
    ptr->out.write((char *)&(node->value),sizeof(node->value));

    // VISITOR_RETURN
};

void genBoolLiteral(ASTBooleanLiteral * node,CodeGenR *ptr){
    // ASTBooleanLiteral * node = ASSERT_AST_NODE(context.current,ASTBooleanLiteral);
    BC code = CRT_STB_BOOL;
    ptr->out.write((char *)&code,sizeof(code));
    bool val;
    if(node->value == "true"){
        val = true;
        ptr->out.write((char *)&val,sizeof(val));
    }
    else if(node->value == "false"){
        val = false;
        ptr->out.write((char *)&val,sizeof(val));
    }
    // VISITOR_RETURN
};

void genNumLiteral(ASTNumericLiteral * node,CodeGenR * ptr){
    BC code = CRT_STB_NUM;
    ptr->out.write((char*)&code,sizeof(code));
    BC code1 = STB_NUM_INT;
    ptr->out.write((char *)&code1,sizeof(code1));
    ptr->out.write((char *)&(node->value),sizeof(node->value));
};

void genExpr(ASTExpression *exp,CodeGenR * ptr){
    if(AST_NODE_IS(exp,ASTStringLiteral)){
        genStrLiteral(ASSERT_AST_NODE(exp,ASTStringLiteral),ptr);
    }
    else if(AST_NODE_IS(exp,ASTBooleanLiteral)){
        genBoolLiteral(ASSERT_AST_NODE(exp,ASTBooleanLiteral),ptr);
    }
    else if(AST_NODE_IS(exp,ASTNumericLiteral)){
        genNumLiteral(ASSERT_AST_NODE(exp,ASTNumericLiteral),ptr);
    }
};

NAMESPACE_END
