#include "starbytes/AST/ASTTraveler.h"

STARBYTES_STD_NAMESPACE

namespace AST {


    bool ASTTraveler::visitImportDecl(){
        bool retr_code = true;
        ASTImportDeclaration * _current = ASSERT_AST_NODE(cntxt.current,ASTImportDeclaration);
        if(!invokeCallback(_current->type))
            retr_code = false;
        return retr_code;
    };

    bool ASTTraveler::visitScopeDecl(){
        bool retr_code = true;
        ASTScopeDeclaration * _current = ASSERT_AST_NODE(cntxt.current,ASTScopeDeclaration);
        if(invokeCallback(_current->type)){
            setParentNode(ASSERT_AST_NODE(_current,ASTNode));
            auto it = _current->body->nodes.begin();
            setNextNode(ASSERT_AST_NODE(_current->body->nodes[0],ASTNode));
            while(it != _current->body->nodes.end()){
                //TODO: Ensure next node is to next node and NOT nullptr!
                nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1),ASTNode));
                if(visitStatement()){
                    setParentNode(ASSERT_AST_NODE(_current,ASTNode));
                    ++it;
                }
                else {
                    retr_code = false;
                    break;
                };
            };
        }
        else{
            retr_code = false;
        }
        return retr_code;
    };


    bool ASTTraveler::visitVariableDecl(){
        bool retr_code = true;
        ASTVariableDeclaration * _current = ASSERT_AST_NODE(cntxt.current,ASTVariableDeclaration);
        // if(invokeCallback(_current->type))
        if(true) {
            setParentNode(ASSERT_AST_NODE(_current,ASTNode));
            auto it = _current->specifiers.begin();
            setNextNode(ASSERT_AST_NODE(_current->specifiers[0],ASTNode));
            while(it != _current->specifiers.end()){
                //TODO: Ensure next node is to next node and NOT nullptr!
                nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1),ASTNode));
                if(invokeCallback((*it)->type))
                    ++it;
                else {
                 retr_code = false;
                 break;
                }
            }
        }
        else
         retr_code = false;
        setParentNode(nullptr);
        return retr_code;
    };

    bool ASTTraveler::visitConstantDecl(){
        bool retr_code = true;
        ASTConstantDeclaration * _current = ASSERT_AST_NODE(cntxt.current,ASTConstantDeclaration);
        // if(invokeCallback(_current->type))
        if(true) {
            setParentNode(ASSERT_AST_NODE(_current,ASTNode));
            auto it = _current->specifiers.begin();
            setNextNode(ASSERT_AST_NODE(_current->specifiers[0],ASTNode));
            while(it != _current->specifiers.end()){
                //TODO: Ensure next node is to next node and NOT nullptr!
                nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1),ASTNode));
                if(invokeCallback((*it)->type))
                    ++it;
                else {
                 retr_code = false;
                 break;
                }
            }
        }
        else
         retr_code = false;
        setParentNode(nullptr);
        return retr_code;
    };
    //TODO : Finish Implementation of this function!
    bool ASTTraveler::visitFunctionDecl(){
        bool retr_code = true;
        ASTFunctionDeclaration * _current = ASSERT_AST_NODE(cntxt.current,ASTFunctionDeclaration);
        if(invokeCallback(_current->type)){
            //TODO: Possibly visit each parameter!
            setParentNode(ASSERT_AST_NODE(_current,ASTNode));
            auto it = _current->body->nodes.begin();
            setNextNode(ASSERT_AST_NODE(_current->body->nodes[0],ASTNode));
            while(it != _current->body->nodes.end()){
                //TODO: Ensure next node is to next node and NOT nullptr!
                nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1),ASTNode));
                if(invokeCallback((*it)->type)){
                    ++it;
                }
                else {
                    retr_code = false;
                    break;
                }
            };
        }
        else 
            retr_code = false;
        return retr_code;
    };
    //TODO : Finish Implementation of this function!
    bool ASTTraveler::visitClassDecl(){
        bool retr_code = true;
        ASTClassDeclaration * _current = ASSERT_AST_NODE(cntxt.current,ASTClassDeclaration);
        if(invokeCallback(_current->type)){
            setParentNode(ASSERT_AST_NODE(_current,ASTNode));
            auto it = _current->body->nodes.begin();
            setNextNode(ASSERT_AST_NODE(_current->body->nodes[0],ASTNode));
            while(it != _current->body->nodes.end()){
                //TODO: Ensure next node is to next node and NOT nullptr!
                nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1),ASTNode));
                if(invokeCallback((*it)->type)){
                    ++it;
                }
                else {
                    retr_code = false;
                    break;
                }
            };
        }
        else 
            retr_code = false;
        return retr_code;
    };

    bool ASTTraveler::visitId(){
        ASTIdentifier * _current = ASSERT_AST_NODE(cntxt.current,ASTIdentifier);
        return invokeCallback(_current->type);
    };

    bool ASTTraveler::visitArrayExpr(){
        bool retr_code = true;
        ASTArrayExpression * _current = ASSERT_AST_NODE(cntxt.current,ASTArrayExpression);
        if(invokeCallback(_current->type)){
            setParentNode(ASSERT_AST_NODE(_current,ASTNode));
             auto it = _current->items.begin();
            setNextNode(ASSERT_AST_NODE(_current->items[0],ASTNode));
             while(it != _current->items.end()){
                    //TODO: Ensure next node is to next node and NOT nullptr!
                    nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1),ASTNode));
                    if(invokeCallback((*it)->type)){
                        ++it;
                    }
                    else {
                        retr_code = false;
                        break;
                    }
             };
        }
        else 
            retr_code = false;
        return retr_code;
    };

    bool ASTTraveler::visitNewExpr(){
        ASTNewExpression * _current = ASSERT_AST_NODE(cntxt.current,ASTNewExpression);
        return invokeCallback(_current->type);
    };

    bool ASTTraveler::visitCallExpr(){
        bool retr_code = true;
        ASTCallExpression * _current = ASSERT_AST_NODE(cntxt.current,ASTCallExpression);
        setParentNode(ASSERT_AST_NODE(_current,ASTNode));
        setCurrentNode(ASSERT_AST_NODE(_current->callee,ASTNode));
        if(visitExpr()){
            auto it = _current->params.begin();
            setCurrentNode(ASSERT_AST_NODE(_current->params[0],ASTNode));
            while(it != _current->params.end()){
                nextNodeAndSetAheadNode(ASSERT_AST_NODE(*(it + 1),ASTNode));
                if(invokeCallback((*it)->type)){
                    ++it;
                }
                else {
                    retr_code = false;
                    break;
                }
            };
            
        }
        else {
            retr_code = false;
        }
        return retr_code;
    };

    bool ASTTraveler::visitExpr(){
        ASTNode *& _current = cntxt.current;
        bool retr_code = false;
        if(AST_NODE_IS(_current,ASTCallExpression)) {
            retr_code = visitCallExpr();
        }
        else if(AST_NODE_IS(_current,ASTIdentifier)){
            retr_code = visitId();
        }
        else if(AST_NODE_IS(_current,ASTMemberExpression)){
            retr_code = visitMemberExpr();
        }
        else if(AST_NODE_IS(_current,ASTArrayExpression)){
            retr_code = visitArrayExpr();
        }
        else if(AST_NODE_IS(_current,ASTNewExpression)){
            retr_code = visitNewExpr();
        }
        else if(AST_NODE_IS(_current,ASTAwaitExpression)){
            retr_code = visitAwaitExpr();
        }
        
        return retr_code;
    };

    bool ASTTraveler::visitExprStmt(){
        ASTExpressionStatement * _current = ASSERT_AST_NODE(cntxt.current,ASTExpressionStatement);
        setCurrentNode(ASSERT_AST_NODE(_current->expression,ASTNode));
        return visitExpr();
    };

    bool ASTTraveler::visitStatement(){
        ASTNode *& _current = cntxt.current;
        bool retr_code = false;
        if(AST_NODE_IS(_current,ASTFunctionDeclaration)){
            retr_code = visitFunctionDecl();
        }
        else if(AST_NODE_IS(_current,ASTVariableDeclaration)){
            retr_code = visitVariableDecl();
        }
        else if(AST_NODE_IS(_current,ASTConstantDeclaration)){
            retr_code = visitConstantDecl();
        }
        else if(AST_NODE_IS(_current,ASTClassDeclaration)){
            retr_code = visitClassDecl();
        }
        else if(AST_NODE_IS(_current,ASTScopeDeclaration)){
            retr_code = visitScopeDecl();
        }
        else if(AST_NODE_IS(_current,ASTImportDeclaration)){
           retr_code = visitImportDecl();
        }
        else if(AST_NODE_IS(_current,ASTExpressionStatement)){
            retr_code = visitExprStmt();
        }
        return retr_code;
    };
    ASTTraveler::ASTTraveler(AbstractSyntaxTree *&ast,
              std::initializer_list<
                  Foundation::DictionaryEntry<ASTType, ASTFuncCallback>>
                  list):ast_lookup(list), tree(ast){

    };

    void ASTTraveler::travel(){
        size_t ast_top_level_len = tree->nodes.size();
        unsigned idx = 0;
        while(ast_top_level_len != idx){
            if(idx == 0) {
                ASTStatement *& node = tree->nodes[idx];
                setCurrentNode(ASSERT_AST_NODE(node,ASTNode));
                setNextNode(ASSERT_AST_NODE(tree->nodes[idx+1],ASTNode));
            }
            else {
                if(ast_top_level_len == (idx + 1))
                    nextNodeAndSetAheadNode();
                
                nextNodeAndSetAheadNode(ASSERT_AST_NODE(tree->nodes[idx+1],ASTNode));
            }
            
            visitStatement();
            ++idx;
        }
    };
};

NAMESPACE_END