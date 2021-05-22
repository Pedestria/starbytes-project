#include "starbytes/Parser/SemanticA.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTNodes.def"
#include <llvm/Support/FormatVariadic.h>
#include <iostream>
namespace starbytes {

struct SemanticADiagnostic : public Diagnostic {
    typedef enum : int {
      Error,
      Warning,
      Suggestion
    } Ty;
    Ty type;
    ASTStmt *stmt;
    std::string message;
    bool isError(){
        return type == Error;
    };
    void format(llvm::raw_ostream &os){
        os << llvm::raw_ostream::RED << "ERROR: " << llvm::raw_ostream::RESET << message << "\n";
    };
    SemanticADiagnostic(Ty type,const llvm::formatv_object_base & message,ASTStmt *stmt):type(type),message(message),stmt(stmt){
        
    };
    SemanticADiagnostic(Ty type,ASTStmt *stmt):type(type),stmt(stmt){
        /// Please Set Message!
    };
    ~SemanticADiagnostic(){
        
    };
};


    void Semantics::SymbolTable::addSymbolInScope(Entry *entry, ASTScope *scope){
        body.insert(std::make_pair(entry,scope));
    };

    bool Semantics::SymbolTable::symbolExists(llvm::StringRef symbolName,ASTScope *scope){
        for(auto & sym : body){
            if(sym.getFirst()->name == symbolName && sym.getSecond() == scope){
                return true;
            };
        };
        return false;
    };
    
    Semantics::SymbolTable::Entry * Semantics::STableContext::findEntry(llvm::StringRef symbolName,SemanticsContext & ctxt,ASTScope *scope){
        unsigned entryCount = 0;
        Semantics::SymbolTable::Entry *ent = nullptr;
        for(auto & pair : main->body){
            if(pair.getFirst()->name == symbolName && pair.getSecond() == scope){
                ent = pair.getFirst();
                ++entryCount;
            };
        };
        
        for(auto & table : otherTables){
            for(auto & pair : table->body){
                if(pair.getFirst()->name == symbolName && pair.getSecond() == scope){
                    ent = pair.getFirst();
                    ++entryCount;
                };
            };
        };
        
        if(entryCount > 1){
            ctxt.errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Multiple symbol defined with the same name (`{0}`) in a scope.",symbolName),ctxt.currentStmt);
            return nullptr;
        }
        else if(entryCount == 0){
            ctxt.errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Undefined symbol `{0}`",symbolName),ctxt.currentStmt);
            return nullptr;
        };
        
        return ent;
    };

    SemanticA::SemanticA(Syntax::SyntaxA & syntaxARef,DiagnosticBufferedLogger & errStream):syntaxARef(syntaxARef),errStream(errStream){

    };

    void SemanticA::start(){
        std::cout << "Starting SemanticA" << std::endl;
    };

    void SemanticA::finish(){
        
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
                e->type = Semantics::SymbolTable::Entry::Var;
                tablePtr->addSymbolInScope(e,decl->scope);
                break;
            }
            case FUNC_DECL : {
                auto *func_id = (ASTIdentifier *)decl->declProps[0].dataPtr;
                Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
                e->name = func_id->val;
                e->node = decl;
                e->type = Semantics::SymbolTable::Entry::Function;
                tablePtr->addSymbolInScope(e,decl->scope);
                break;
            }
            // case CLS_DECL : {
            //     ASTIdentifier *cls_id = (ASTIdentifier *)decl->declProps[0].dataPtr;
            //     Semantics::SymbolTable::Entry *e = new Semantics::SymbolTable::Entry();
            //     e->name = cls_id->val;
            //     e->node = decl;
            //     e->type = Semantics::SymbolTable::Entry::Class;
            //     tablePtr->addSymbolInScope(e,decl->scope);
            //     break;
            // }
        default : {
            break;
        }
        }
    };
    
ASTType * VOID_TYPE = ASTType::Create("Void",nullptr,false);
ASTType * STRING_TYPE = ASTType::Create("String",nullptr,false);
ASTType * ARRAY_TYPE = ASTType::Create("Array",nullptr,false);
ASTType * DICTIONARY_TYPE = ASTType::Create("Dict",nullptr,false);
ASTType * BOOL_TYPE  = ASTType::Create("Bool",nullptr,false);
ASTType * INT_TYPE = ASTType::Create("Int",nullptr,false);

#define PRINT_FUNC_ID "print"

    bool SemanticA::typeExists(ASTType *type,Semantics::STableContext &contextTableContxt){
        /// First check to see if ASTType is one of the built in types
        if(type == VOID_TYPE || type == STRING_TYPE || type == ARRAY_TYPE || 
        type == DICTIONARY_TYPE || type == BOOL_TYPE || type == INT_TYPE){
            return true;
        }
        else {
            return false;
        }
    };

    ASTType * SemanticA::evalExprForTypeId(ASTExpr *expr_to_eval, Semantics::STableContext & symbolTableContext){
        ASTType *type;
        switch (expr_to_eval->type) {
            case ID_EXPR : {
                ASTIdentifier *id_ = expr_to_eval->id;
                SemanticsContext ctxt {errStream,expr_to_eval};
                
                auto symbol_ = symbolTableContext.findEntry(id_->val,ctxt);
                if(!symbol_){
                    return nullptr;
                    break;
                };
                
                if(symbol_->type == Semantics::SymbolTable::Entry::Var){
                    ASTDecl *varSpecDecl = (ASTDecl *)symbol_->node;
                    ASTType *_type = varSpecDecl->declType;
                    type = _type;
                }
                else {
                    errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Identifier in this context cannot identify any other symbol type except another variable."),expr_to_eval);
                    return nullptr;
                };
                
                break;
            }
            case ARRAY_EXPR : {
                /// TODO: Check for specific Array Type
                type = ARRAY_TYPE;
                break;
            }
                /// Literals
            case BOOL_LITERAL : {
                type = BOOL_TYPE;
                break;
            }
            case STR_LITERAL:
            {
                type = STRING_TYPE;
                break;
            }
            case IVKE_EXPR : {
               
                
                /// 2. Check return type.
                ASTIdentifier *func_id = expr_to_eval->id;
                /// a. Check internal function return types.
                llvm::StringRef func_name = func_id->val;
                if(func_name == PRINT_FUNC_ID){
                    if(expr_to_eval->exprArrayData.size() > 1){
                        errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Incorrect number of arguments"),expr_to_eval);
                    }
                    /// print() -> Void
                    type = VOID_TYPE;
                };
                
                /// 1. Eval Args.
                
                for(auto & arg : expr_to_eval->exprArrayData){
                    ASTType *_id = evalExprForTypeId(arg,symbolTableContext);
                    if(!_id){
                        return nullptr;
                        break;
                    };
                    
                };
                
                
                break;
            }
            default:
                return nullptr;
                break;
        }
        return type;
    }

    bool SemanticA::typeMatches(ASTType *type,ASTExpr *expr_to_eval,Semantics::STableContext & symbolTableContext){
        
        auto other_type_id = evalExprForTypeId(expr_to_eval,symbolTableContext);
        if(!other_type_id){
            return false;
        };
        
        /// For Now only Matches Type by Name
        /// TODO: Eventually Match by Type Args, Scope, etc..
        ///
        return type->match(other_type_id,[&](const llvm::formatv_object_base & message){
            auto diag = new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("{0}\nContext: Type `{1}` was implied from var initializer",message,other_type_id),expr_to_eval);
        });
    };

    bool SemanticA::checkSymbolsForStmt(ASTStmt *stmt,Semantics::STableContext & symbolTableContext){
        if(stmt->type & DECL){
            ASTDecl *decl = (ASTDecl *)stmt;
            switch (decl->type) {
                /// VarDecl
                case VAR_DECL : {
                    for(auto & spec : decl->declProps){
                        ASTDecl *specDecl = (ASTDecl *)spec.dataPtr;
                        if(specDecl->declType && specDecl->declProps.size() > 1){
                            // Type Decl and Type Implication Comparsion
                            if(!typeMatches(specDecl->declType,(ASTExpr *)specDecl->declProps[1].dataPtr,symbolTableContext))
                            {
                                return false;
                            }
                        }
                        else if(specDecl->declProps.size() == 1) {
                            /// No Type Implication nor any type decl
                            ASTIdentifier *id = (ASTIdentifier *)specDecl->declProps[0].dataPtr;
                            errStream << new SemanticADiagnostic(SemanticADiagnostic::Error,llvm::formatv("Variable `{0}` has no type assignment nor an initial value thus the variable's type cannot be deduced.",id->val), specDecl);
                            return false;
                        }
                        else {
                            /// Type Implication Only.
                            auto type = evalExprForTypeId((ASTExpr *)specDecl->declProps[1].dataPtr,symbolTableContext);
                            if(!type){
                                return false;
                            };
                            specDecl->declType = type;
                        };
                    };
                    break;
                }
                /// FuncDecl
                case FUNC_DECL : {
                    auto funcNode = (ASTFuncDecl *)decl;
                    
                    SemanticsContext ctxt {errStream,funcNode};

                    ASTIdentifier *func_id = (ASTIdentifier *)funcNode->declProps[0].dataPtr;
                    
                    /// Ensure that Function declared is unique within the current scope.
                    auto symEntry = symbolTableContext.main->symbolExists(func_id->val,ASTScopeGlobal);
                    if(symEntry){
                        return false;
                    };



                    break;
                }
            }
        }
        else if(stmt->type & EXPR){
            ASTExpr *expr = (ASTExpr *)stmt;
            switch (expr->type) {
                case IVKE_EXPR:
                {
                    /// An Invoke Expression with no return variable capture..
                    /// Example:
                    ///
                    /// func myFunc(){
                    ///     // Do Stuff Here
                    /// }
                    ///
                    /// myfunc()
                    ///
                    
                    ASTType *return_type = evalExprForTypeId(expr,symbolTableContext);
                    if(!return_type){
                        return false;
                    };
                    
                    if(return_type->nameMatches(VOID_TYPE)){
                        /// Warn of non void object not being stored after creation from function invocation.
                    };
                    
                    break;
                }
                default:
                    break;
            }
        };
        return true;
    };
};

