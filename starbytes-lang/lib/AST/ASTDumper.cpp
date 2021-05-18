#include "starbytes/AST/ASTDumper.h"
#include <iostream>
#include <sstream>

namespace starbytes {

    std::string padString(unsigned amount){
        std::string str = "";
        while (amount > 0) {
            str += "    ";
            --amount;
        }
        return std::move(str);
    };

    void formatIdentifier(std::ostream & os,ASTIdentifier *id,unsigned level){
        auto pad = padString(level);
        os << 
        "Identifier : {\n" <<
        pad << "   value:\"" << id->val << "\"\n" <<
        pad << "}" << std::endl;
    };

    ASTDumper::ASTDumper(std::ostream & os):os(os){

    };
    
    bool ASTDumper::acceptsSymbolTableContext(){
        return false;
    };

    std::unique_ptr<ASTDumper> ASTDumper::CreateStdoutASTDumper(){
        return std::unique_ptr<ASTDumper>(new ASTDumper(std::cout));
    };

    void ASTDumper::printDecl(ASTDecl *decl,unsigned level){
        auto pad = padString(level);
        if(decl->type == IMPORT_DECL){
            os <<
            "ImportDecl : {\n" << pad <<
            "   module_name:" << std::flush; 
            formatIdentifier(os,(ASTIdentifier *)decl->declProps[0].dataPtr,level+1);
            os << pad << "}" << std::endl;
        }
        else if(decl->type == VAR_DECL){
            os << 
            "VarDecl : {\n" << pad << 
            "   vars: [\n" << std::flush;
            auto n_level = level + 1;
            auto _pad = padString(n_level);
            for(auto & var : decl->declProps){
                os << _pad << "VarSpec : {\n" << _pad <<
                "   identifier:" << std::flush;
                // std::cout << "VarSpec Access" << std::endl;
                auto varSpec = (ASTDecl *)var.dataPtr;
                // std::cout << "VarSpec Access Available" << std::endl;
                auto & id = varSpec->declProps[0];
                // std::cout << "VarSpec ID Access Available" << std::endl;
                formatIdentifier(os,(ASTIdentifier *)id.dataPtr,n_level + 1);
                
                if(varSpec->declProps.size() > 1){
                    os << _pad << "   initialVal:" << std::flush;
                    auto expr = varSpec->declProps[1].dataPtr;
                    
                    printStmt(expr,n_level + 1);
                }
                
                os << _pad << "}" << std::endl;
            };
            os << pad << "  ]\n" << pad << "}" << std::endl;
        };
    };

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
                  "   id:" << std::flush;
            formatIdentifier(os,expr->id,level + 1);
            os << pad << "   args:[\n" << std::flush;
            auto n_level = level + 1;
            auto _pad = padString(n_level);
            for(auto arg : expr->exprArrayData){
                os << _pad << std::flush;
                printStmt(arg,n_level);
            };
            os << pad << "   ]\n" << pad << "}" << std::endl;
        }/// Literals
        else if(expr->type == STR_LITERAL){
            os << "StrLiteral: {\n" << pad <<
                  "   value:" << expr->literalValue << "\n" << pad <<
                  "}\n" << std::endl;
            
        };
        
    };

    void ASTDumper::consumeDecl(ASTDecl *stmt){
        printDecl(stmt, 0);
    };

    void ASTDumper::consumeStmt(ASTStmt *stmt){
        printStmt(stmt,0);
    };
}
