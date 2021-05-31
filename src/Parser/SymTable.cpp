#include "starbytes/Parser/SymTable.h"
#include "starbytes/Parser/SemanticA.h"

namespace starbytes {

void Semantics::SymbolTable::addSymbolInScope(Entry *entry, ASTScope *scope){
    body.insert(std::make_pair(entry,scope));
}

bool Semantics::SymbolTable::symbolExists(llvm::StringRef symbolName,ASTScope *scope){
    for(auto & sym : body){
        if(sym.getFirst()->name == symbolName && sym.getSecond() == scope){
            return true;
        };
    };
    return false;
}

Semantics::SymbolTable::Entry * Semantics::STableContext::findEntry(llvm::StringRef symbolName,SemanticsContext & ctxt,ASTScope *scope){
    unsigned entryCount = 0;
    Semantics::SymbolTable::Entry *ent = nullptr;
    for(auto & pair : main->body){
        // std::cout << "SCOPE PTR 1:" << pair.getSecond() << std::endl << "SCOPE PTR 2" << scope << std::endl;
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
}

Semantics::SymbolTable::~SymbolTable(){
    for(auto & e : body){
        delete e.getFirst();
    };
    body.clear();
}

}
