#include "Semantics/SemanticsEngine.h"
#include "Parser/AST.h"
#include "Parser/Document.h"
#include "Parser/Parser.h"
#include <string>
#include <vector>

using namespace Starbytes;

void SymbolStore::addSymbol(SymbolStoreEntry *symbol){
    symbols.push_back(symbol);
}

bool SymbolStore::verifySymbolTS(std::string symbol,SymbolType symbol_type){
    for(auto sym : symbols){
        if(sym->symbol == symbol && sym->type == symbol_type){
            return true;
        }
    }
    return false;
}

bool SymbolStore::verifySymbolTIS(std::string symbol){
    for(auto sym : symbols){
        if(sym->symbol == symbol){
            return true;
        }
    }
    return false;
}

SymbolType SymbolStore::getSymbolTypeFromSymbol(std::string symbol){
    for(auto sym : symbols){
        if(sym->symbol == symbol){
            return sym->type;
        }
    }
    return SymbolType::Unresolved;
}


bool inStoreScope(SymbolStore * store,std::vector<std::string> *scopes){
    for(auto scope : *scopes){
        if(scope == store->name){
            return true;
        }
    }
    return false;
}


void SemanticAnalyzer::createSymbolStore(std::string name,Scope scope_type){
    stores.push_back(new SymbolStore(name,scope_type));
}

void SemanticAnalyzer::addSymbolToStore(std::string store_name,SymbolStoreEntry* symbol){
    for(auto store : stores){
        if(store->name == store_name){
            store->addSymbol(symbol);
            break;
        }
    }
}

SymbolType SemanticAnalyzer::getSymbolTypeFromSymbolInCurrentStores(std::string symbol){
    for(auto store: stores){
        if(inStoreScope(store,&currentStoreScopes)){
            if(store->verifySymbolTIS(symbol)){
                return store->getSymbolTypeFromSymbol(symbol);
            }
        }
    }
    return SymbolType::Unresolved;
}

bool SemanticAnalyzer::verifySymbolInCurrentStores(std::string _symbol,SymbolType symbol_type,DocumentPosition *position){
    for(auto store : stores){
        if(inStoreScope(store,&currentStoreScopes)){
            if(store->verifySymbolTS(_symbol,symbol_type)){
                return true;
            }
        }
    }
    return false;
}

void SemanticAnalyzer::freeSymbolStores(){
    for(auto store : stores){
        auto symbols = *(store->getSymbols());
        symbols.clear();
        delete store;
    }
};

void SemanticAnalyzer::addToCurrentScopes(std::string scope_name){
    currentStoreScopes.push_back(scope_name);
}

void SemanticAnalyzer::removeFromCurrentScopes(std::string scope_name){
    for(int i = 0; i < currentStoreScopes.size();++i){
        if(currentStoreScopes[i] == scope_name){
            currentStoreScopes.erase(currentStoreScopes.begin()+i);
        }
    }
};
//
//
//
//
//
//

std::string StarbytesSemanticError(std::string message,DocumentPosition position){
    return message.append("\nSemantic Engine Error at {Line:"+std::to_string(position.line)+"\nStart Character:"+std::to_string(position.start)+"\nEnd Character:"+std::to_string(position.end)+"\n}");
}


bool isLiteralNodeType(ASTType * type){
    switch (*type) {
    case Starbytes::AST::ASTType::StringLiteral :
        return true;
        break;
    case Starbytes::AST::ASTType::NumericLiteral :
        return true;
        break;
    case Starbytes::AST::ASTType::BooleanLiteral :
        return true;
        break;
    default:
        return false;
        break;
    }
}


void SemanticAnalyzer::check(bool semanticTokens,std::vector<SemanticToken *> * semTokArray) {
    outputSemanticTok = semanticTokens;
    if(semTokArray && semanticTokens){
        semanticTokArray = semTokArray;
    }

    createSymbolStore("GLOBAL",Scope::Global);
    addToCurrentScopes("GLOBAL");
    exactCurrentStoreScope = "GLOBAL";

    for(auto node : syntaxTree->nodes){
        checkStatement(node);
    }

};

void SemanticAnalyzer::checkStatement(ASTStatement *node){
    switch (node->type) {
    case AST::ASTType::FunctionDeclaration:
        checkFunctionDeclaration((ASTFunctionDeclaration *)node);
        break;
    case AST::ASTType::ClassDeclaration :
        checkClassDeclaration((ASTClassDeclaration *) node);
        break;
    case AST::ASTType::ScopeDeclaration:
        checkScopeDeclaration((ASTScopeDeclaration *) node);
        break;
    case AST::ASTType::VariableDeclaration:
        checkVariableDeclaration((ASTVariableDeclaration*) node);
        break;
    case AST::ASTType::ConstantDeclaration:
        checkConstantDeclaration((ASTConstantDeclaration *) node);
    default:
        throw StarbytesSemanticError("Unknown Node Type:" +std::to_string(int(node->type)),node->BeginFold);
        break;
    }
}

void SemanticAnalyzer::checkVariableDeclaration(ASTVariableDeclaration *node){
    for(auto declrtr : node->specifiers){
        if(declrtr->initializer != nullptr){
            if(isLiteralNodeType(&declrtr->initializer->type)){
                SymbolStoreEntry *entry = new SymbolStoreEntry();
                VariableOptions *options = new VariableOptions();
                switch (declrtr->initializer->type) {
                case AST::ASTType::StringLiteral:
                    options->type_reference = "String";
                    break;
                case AST::ASTType::NumericLiteral :
                    options->type_reference = "Number";
                    break;
                case AST::ASTType::BooleanLiteral :
                    options->type_reference = "Boolean";
                    break;
                default:
                    throw StarbytesSemanticError("Unknown Node Type!",declrtr->initializer->BeginFold);
                    break;
                }
                if(declrtr->id->type == AST::ASTType::TypecastIdentifier){
                    if(options->type_reference == ((ASTTypeCastIdentifier *)declrtr->id)->tid->value){
                        entry->symbol = ((ASTTypeCastIdentifier *)declrtr->id)->id->value;
                    } else {
                        throw StarbytesSemanticError("Type `"+options->type_reference+"` is NOT equal to Type `"+((ASTTypeCastIdentifier *)declrtr->id)->tid->value+"`", ((ASTTypeCastIdentifier *)declrtr->id)->tid->BeginFold);
                    }
                }
                entry->type = SymbolType::Variable;
                entry->options = options;
                addSymbolToStore(exactCurrentStoreScope,entry);
            }
            else {
                SymbolStoreEntry *entry = new SymbolStoreEntry();
                VariableOptions *options = new VariableOptions();
                options->type_reference = *getTypeOnExpression(declrtr->initializer);
                if(declrtr->id->type == AST::ASTType::TypecastIdentifier){
                    if(options->type_reference == ((ASTTypeCastIdentifier *)declrtr->id)->tid->value){
                        entry->symbol = ((ASTTypeCastIdentifier *)declrtr->id)->id->value;
                    } else {
                        throw StarbytesSemanticError("Type `"+options->type_reference+"` is NOT equal to Type `"+((ASTTypeCastIdentifier *)declrtr->id)->tid->value+"`", ((ASTTypeCastIdentifier *)declrtr->id)->tid->BeginFold);
                    }
                }
                entry->type = SymbolType::Variable;
                entry->options = options;
                addSymbolToStore(exactCurrentStoreScope,entry);


            }
        }
        else{
            if(declrtr->id->type == AST::ASTType::TypecastIdentifier){
                //Checks to see if Type Exists!
                if(getSymbolTypeFromSymbolInCurrentStores(((ASTTypeCastIdentifier *)declrtr->id)->tid->value) != SymbolType::Unresolved){
                    SymbolStoreEntry *entry = new SymbolStoreEntry();
                    VariableOptions *options = new VariableOptions();
                    options->type_reference = ((ASTTypeCastIdentifier *) declrtr->id)->tid->value;
                    entry->symbol = ((ASTTypeCastIdentifier *) declrtr->id)->id->value;
                    entry->type = SymbolType::Variable;
                    entry->options = options;
                    addSymbolToStore(exactCurrentStoreScope,entry);
                }
            }
            else if(declrtr->id->type == AST::ASTType::Identifier){
                throw StarbytesSemanticError("Cannot infer type without intializer!",declrtr->BeginFold);
            }
            else{
                throw StarbytesSemanticError("Unknown Node Type:"+std::to_string(int(declrtr->id->type)),declrtr->BeginFold);
            }
        }
    }
}

void SemanticAnalyzer::checkFunctionDeclaration(ASTFunctionDeclaration *node){
    if(verifySymbolInCurrentStores(node->id->value,SymbolType::Function,&node->id->position)){
        throw StarbytesSemanticError("Function ID's identical! ID:`"+node->id->value+"`",node->id->position);
    }
    else{
        std::string priorScope = exactCurrentStoreScope;
        createSymbolStore(node->id->value,Scope::FunctionBlockStatement);
        addToCurrentScopes(node->id->value);
        if(node->isGeneric){
            checkTypeArgumentsDeclaration(node->typeargs);
        }

        if(!node->params.empty()){
            checkFuncArguments(&node->params);
        }

        exactCurrentStoreScope = node->id->value;
        for(auto child_node : node->body->nodes){
            checkStatement(node);
        }
        exactCurrentStoreScope = priorScope;
        removeFromCurrentScopes(node->id->value);
    }
}
