#include "starbytes/compiler/InterfaceGen.h"
#include "starbytes/compiler/ASTNodes.def"
#include "starbytes/compiler/Gen.h"


namespace starbytes {

void InterfaceGen::setContext(ModuleGenContext *genContext){
    this->genContext = genContext;
    out.open(std::filesystem::path(genContext->outputPath).append(genContext->name).concat(".starbint"));
}

void InterfaceGen::finish(){
    out.close();
}

void InterfaceGen::consumeSTableContext(Semantics::STableContext *context){
    
}

void InterfaceGen::consumeDecl(ASTDecl *stmt) {
    switch (stmt->type) {
        case FUNC_DECL: {
            auto node = (ASTFuncDecl *)stmt;
            out << KW_FUNC << " " << node->funcId->val << "(";
            
            unsigned idx = 0;
            for(auto & n : node->params){
                if(idx > 0){
                    out << ",";
                }
                out << n.first->val << ":" << n.second->getName();
                idx++;
            }
            out << ")" << node->returnType->getName() << std::endl;
            if(node->scope->type == ASTScope::Namespace || node->scope->type == ASTScope::Neutral){
                out << std::endl;
            }
            break;
        }
        case CLASS_DECL : {
            auto node = (ASTClassDecl*)stmt;
            out << KW_CLASS << " " << node->id->val << " " << "{";
            for(auto & field : node->fields){
                consumeDecl(field);
            }
            for(auto & func : node->methods){
                consumeDecl(func);
            }
            out << "}" << std::endl << std::endl;
            break;
        }
        case INTERFACE_DECL : {
            auto node = (ASTInterfaceDecl*)stmt;
            out << KW_INTERFACE << " " << node->id->val << " " << "{";
            for(auto & field : node->fields){
                consumeDecl(field);
            }
            for(auto & func : node->methods){
                consumeDecl(func);
            }
            out << "}" << std::endl << std::endl;
            break;
        }
        default: {
            break;
        }
    }
}

void InterfaceGen::consumeStmt(ASTStmt *stmt) {
    return;
}


};
