#include "starbytes/compiler/ASTDumper.h"
#include "starbytes/compiler/ASTNodes.def"
#include <iostream>
#include <sstream>

namespace starbytes {

    inline std::string padString(unsigned amount){
        std::string str = "";
        while (amount > 0) {
            str += "    ";
            --amount;
        }
        return str;
    }

    inline void formatIdentifier(std::ostream & os,ASTIdentifier *id,unsigned level){
        auto pad = padString(level);
        os << 
        "Identifier : {\n" <<
        pad << "   value:\"" << id->val << "\"\n" <<
        pad << "}" << std::endl;
    }

    ASTDumper::ASTDumper(std::ostream & os):os(os){

    }
    
    bool ASTDumper::acceptsSymbolTableContext(){
        return false;
    }

    std::unique_ptr<ASTDumper> ASTDumper::CreateStdoutASTDumper(){
        return std::unique_ptr<ASTDumper>(new ASTDumper(std::cout));
    }

    void ASTDumper::printBlockStmt(ASTBlockStmt *blockStmt,unsigned level){
        auto pad = padString(level);
        auto __pad = padString(level + 1);
        os <<
        "BlockStmt : {\n" << pad <<
        "   nodes:[\n" << __pad << std::flush;
        
        for(auto & n : blockStmt->body){
            if(n->type & DECL){
                printDecl((ASTDecl *)n,level + 1);
            }
            else {
                printStmt(n,level + 1);
            };
        };
        
        os << pad << "  ]\n" << pad << "}\n";
    }

    void ASTDumper::printDecl(ASTDecl *decl,unsigned level){
        auto pad = padString(level);
        if(decl->type == IMPORT_DECL){
            auto data = (ASTImportDecl *)decl;
            os <<
            "ImportDecl : {\n" << pad <<
            "   module_name:" << std::flush; 
            formatIdentifier(os,data->moduleName,level+1);
            os << pad << "}" << std::endl;
        }
        else if(decl->type == SCOPE_DECL){
            auto *scopeDecl = (ASTScopeDecl *)decl;
            os << "ScopeDecl : {\n" << pad <<
                  "   id:" << std::flush;
            formatIdentifier(os,scopeDecl->scopeId,level + 1);
            os << pad << "   body:" << std::flush;
            printBlockStmt(scopeDecl->blockStmt,level + 1);
            os << pad << "}" << std::endl;
        }
        else if(decl->type == VAR_DECL){
            os << 
            "VarDecl : {\n" << pad << 
            "   vars: [\n" << std::flush;
            auto n_level = level + 1;
            auto _pad = padString(n_level);
            auto data = (ASTVarDecl *)decl;
            for(auto & varSpec : data->specs){
                os << _pad << "VarSpec : {\n" << _pad <<
                "   identifier:" << std::flush;
                // std::cout << "VarSpec Access Available" << std::endl;
                auto & id = varSpec.id;
                // std::cout << "VarSpec ID Access Available" << std::endl;
                formatIdentifier(os,id,n_level + 1);
                
                if(varSpec.type){
                    os << _pad << "   type:" << std::flush;
                    auto type_id = varSpec.type;
                    os << type_id->getName() << std::endl;
                }
                
                if(varSpec.expr){
                    os << _pad << "   initialVal:" << std::flush;
                    auto expr = varSpec.expr;
                    
                    printStmt(expr,n_level + 1);
                }
                
                os << _pad << "}" << std::endl;
            };
            os << pad << "  ]\n" << pad << "}" << std::endl;
        }
        else if(decl->type == FUNC_DECL){
            os <<
            "FuncDecl : {\n" << pad <<
            "   id:" << std::flush;
            ASTFuncDecl *func_node = (ASTFuncDecl *)decl;
            auto & id = func_node->funcId;
            formatIdentifier(os,id,level + 1);
            os << pad <<"   params:[\n" << std::flush;
            auto __level = level + 1;
            auto _pad = padString(__level);
            for(auto & param : func_node->params){
                os << _pad <<
                "ParamDecl : {\n" << _pad <<
                "   id:" << std::flush;
                formatIdentifier(os,param.first,__level + 1);
                os << _pad <<
                "   type:" << param.second->getName() << std::endl;
                os << _pad << "}" << std::endl;
            };
            os << pad << "  ]\n";
            os << pad << "   body:" << std::flush;
            printBlockStmt(func_node->blockStmt,__level);
            os << pad << "}" << std::endl;
        }
        else if(decl->type == CLASS_CTOR_DECL){
            auto *ctorNode = (ASTConstructorDecl *)decl;
            os << "ConstructorDecl : {\n" << pad << "   params:[\n" << std::flush;
            auto __level = level + 1;
            auto _pad = padString(__level);
            for(auto &param : ctorNode->params){
                os << _pad <<
                "ParamDecl : {\n" << _pad <<
                "   id:" << std::flush;
                formatIdentifier(os,param.first,__level + 1);
                os << _pad <<
                "   type:" << param.second->getName() << std::endl;
                os << _pad << "}" << std::endl;
            }
            os << pad << "  ]\n";
            os << pad << "   body:" << std::flush;
            printBlockStmt(ctorNode->blockStmt,__level);
            os << pad << "}" << std::endl;
        }
        else if(decl->type == COND_DECL){
            ASTConditionalDecl *cond_decl = (ASTConditionalDecl *)decl;

            for(auto cond_it = cond_decl->specs.begin();cond_it < cond_decl->specs.end();cond_it++){
                auto & cond = *cond_it;
                auto __level = level + 1;
                if(cond.isElse()){
                        os << "ElseDecl : {\n" << pad << 
                              "   block:" << std::flush;
                        printBlockStmt(cond.blockStmt,__level); 
                        os << pad << "}" << std::endl;
                }
                else {
                    if(cond_it == cond_decl->specs.begin())
                        os << "IfDecl : {\n" << std::flush;
                    else 
                        os << "ElifDecl : {\n" << std::flush;

                    os << pad << "   expr:" << std::flush;
                    printStmt(cond.expr,__level);
                    os << pad << "   block:" << std::flush;
                    printBlockStmt(cond.blockStmt,__level); 
                    os << pad << "}" << std::endl;
                };
            };
        }
        else if(decl->type == FOR_DECL){
            auto *forDecl = (ASTForDecl *)decl;
            os << "ForDecl : {\n" << std::flush;
            os << pad << "   expr:" << std::flush;
            printStmt(forDecl->expr,level + 1);
            os << pad << "   block:" << std::flush;
            printBlockStmt(forDecl->blockStmt,level + 1);
            os << pad << "}" << std::endl;
        }
        else if(decl->type == WHILE_DECL){
            auto *whileDecl = (ASTWhileDecl *)decl;
            os << "WhileDecl : {\n" << std::flush;
            os << pad << "   expr:" << std::flush;
            printStmt(whileDecl->expr,level + 1);
            os << pad << "   block:" << std::flush;
            printBlockStmt(whileDecl->blockStmt,level + 1);
            os << pad << "}" << std::endl;
        }
        else if(decl->type == SECURE_DECL){
            auto *secureDecl = (ASTSecureDecl *)decl;
            os << "SecureDecl : {\n" << std::flush;
            os << pad << "   decl:" << std::flush;
            if(secureDecl->guardedDecl){
                printDecl(secureDecl->guardedDecl,level + 1);
            }
            if(secureDecl->catchErrorId){
                os << pad << "   catchError:" << secureDecl->catchErrorId->val << std::endl;
            }
            if(secureDecl->catchErrorType){
                os << pad << "   catchType:" << secureDecl->catchErrorType->getName() << std::endl;
            }
            os << pad << "   catchBlock:" << std::flush;
            if(secureDecl->catchBlock){
                printBlockStmt(secureDecl->catchBlock,level + 1);
            }
            os << pad << "}" << std::endl;
        }
        else if(decl->type == RETURN_DECL){
            ASTReturnDecl *return_decl = (ASTReturnDecl *)decl;
            os << "ReturnDecl : {\n" << std::flush;
            if(return_decl->expr != nullptr){
                os << pad << "   expr:" << std::flush;
                printStmt(return_decl->expr,level + 1);
            }
            os << pad << "}" << std::endl;
        };
    }

    void ASTDumper::printStmt(ASTStmt *stmt,unsigned level){
        ASTExpr *expr = (ASTExpr *)stmt;
        auto pad = padString(level);
        if (expr->type == ID_EXPR){
            os << "IdentifierExpr: {\n" << pad <<
                  "   id:" << std::flush;
            formatIdentifier(os,expr->id,level + 1);
            os << pad << "}" << std::endl;
        }
        else if(expr->type == IVKE_EXPR){
            os << "InvokeExpr: {\n" << pad <<
                  "   callee:" << std::flush;
            printStmt(expr->callee,level + 1);
            os << pad << "   args:[\n" << std::flush;
            auto n_level = level + 1;
            auto _pad = padString(n_level);
            for(auto arg : expr->exprArrayData){
                os << _pad << std::flush;
                printStmt(arg,n_level);
            };
            os << pad << "   ]\n" << pad << "}" << std::endl;
        }
        else if(expr->type == ARRAY_EXPR){
            os <<
            "ArrayExpr : {\n" << pad <<
            "   objects:[\n" << std::flush;
            auto n_level = level + 1;
            auto _pad = padString(n_level);
            for(auto arg : expr->exprArrayData){
                os << _pad << std::flush;
                printStmt(arg,n_level);
            };
            os << pad << "   ]\n" << pad << "}" << std::endl;
        }
        /// Literals
        else if(expr->type == STR_LITERAL){
            ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
            os << "StrLiteral: {\n" << pad <<
                  "   value:\"" << literal_expr->strValue.value() << "\"\n" << pad <<
                  "}\n" << std::endl;
            
        }
        else if(expr->type == BOOL_LITERAL){
            ASTLiteralExpr *bool_expr = (ASTLiteralExpr *)expr;
            os << "BoolLiteral: {\n" << pad <<
                  "   value:\"" << std::boolalpha << bool_expr->boolValue.value() << std::noboolalpha << "\"\n" << pad <<
                  "}\n" << std::endl;
        }
        else if(expr->type == NUM_LITERAL){
            ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
            os << "NumLiteral: {\n" << pad <<
                  "   value:";
                  if(literal_expr->intValue.has_value()){
                      os << literal_expr->intValue.value() << std::endl;
                  }
                  else {
                      os << literal_expr->floatValue.value() << std::endl;
                  };
                  os << pad << "}\n" << std::endl;
        }
        else if(expr->type == REGEX_LITERAL){
            ASTLiteralExpr *literal_expr = (ASTLiteralExpr *)expr;
            os << "RegexLiteral: {\n" << pad
               << "   pattern:/" << literal_expr->regexPattern.value_or("")
               << "/" << literal_expr->regexFlags.value_or("") << "\n" << pad
               << "}\n" << std::endl;
        };
        
    }

    void ASTDumper::consumeDecl(ASTDecl *stmt){
        printDecl(stmt, 0);
    }

    void ASTDumper::consumeStmt(ASTStmt *stmt){
        printStmt(stmt,0);
    }
}
