#include "starbytes/Parser/SemanticA.h"
#include "starbytes/AST/AST.h"
#include <iostream>
namespace starbytes {
    void Semantics::SymbolTable::addSymbolInScope(Entry *entry, ASTScope *scope){
        if(body.empty())
            body.init(1);
        else 
            body.grow(1);
        body.insert(std::make_pair(entry,scope));
    };

    SemanticA::SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticBufferedLogger & errStream):syntaxARef(syntaxARef),errStream(errStream){

    };

    void SemanticA::start(){
        std::cout << "Starting SemanticA" << std::endl;
    };
     /// Only registers new symbols associated with top level decls!
    void SemanticA::addSTableEntryForDecl(ASTDecl *decl,Semantics::SymbolTable *tablePtr){
       
        switch (decl->type) {
            case VAR_DECL : {
                auto *varDeclarator = (ASTDecl *)decl->declProps[0].dataPtr;
                Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
                auto * id = (ASTIdentifier *)varDeclarator->declProps[0].dataPtr;
                e->name = id->val;
                e->node = varDeclarator;
                tablePtr->addSymbolInScope(e,decl->scope);
                break;
            }
            case CLS_DECL : {
                ASTIdentifier *cls_id = (ASTIdentifier *)decl->declProps[0].dataPtr;
                Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
                e->name = cls_id->val;
                e->node = decl;
                e->type = Semantics::SymbolTable::Entry::Class;
                tablePtr->addSymbolInScope(e,decl->scope);
                break;
            }
        default : {
            break;
        }
        }
    };

    void SemanticA::checkSymbolsForStmt(ASTStmt *stmt,Semantics::STableContext & symbolTableContext){
        if(stmt->type & DECL){
            ASTDecl *decl = (ASTDecl *)stmt;
            switch (decl->type) {
                case VAR_DECL : {
                    
                    break;
                }
            }
        }
        else if(stmt->type & EXPR){

        };
    };
};

