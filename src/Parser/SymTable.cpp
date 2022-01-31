#include "starbytes/Parser/SymTable.h"
#include "starbytes/Parser/SemanticA.h"

typedef uint8_t SymbolTableCode;

#define TABLE_START 0xA0
#define IMPORT_SYMTABLE 0x0B
#define DECLARE_SCOPE 0xC0
#define TYPE_EXPR 0xD0
#define DECLARE_TYPE 0xE0

#define VAR_ENTRY 0x01
#define FUNC_ENTRY 0x02
#define CLASS_ENTRY 0x03
#define INTERFACE_ENTRY 0x04

#define TABLE_END 0xFF


namespace starbytes {

struct SymTableID {
    std::string id;
};

inline std::ostream &operator<<(std::ostream & out,SymTableID & id){
    out.write((char *)id.id.size(),sizeof(id.id.size()));
    out.write(id.id.data(),id.id.size());
    return out;
};

inline std::istream & operator>>(std::istream & in,SymTableID & id){
    std::string::size_type s;
    in.read((char *)&s,sizeof(s));
    id.id.resize(s);
    in.read((char *)id.id.data(),s);
    return in;
}

void renderScope(ASTScope *scope,std::ostream & out){
    SymbolTableCode code = DECLARE_SCOPE;
    out.write((char *)&code,sizeof(code));
    SymTableID id {scope->name};
    out << id;
    id = {scope->parentScope->name};
    out << id;
}

void importScope(ASTScope **scope,llvm::ArrayRef<ASTScope *> importedModuleScopes,std::istream & in){
    SymbolTableCode code;
    in.read((char *)&code,sizeof(code));
    SymTableID id{};
    in >> id;
    *scope = new ASTScope {id.id,ASTScope::Namespace,nullptr};
    in >> id;
    if(id.id == ASTScopeGlobal->name){
        (*scope)->parentScope = ASTScopeGlobal;
    }
    else {
        auto _parent_scope = std::find_if(importedModuleScopes.begin(),importedModuleScopes.end(),[&](ASTScope * object){
            return object->name == id.id;
        });
        (*scope)->parentScope = *_parent_scope;
    }
}

void importType(ASTType **type,std::istream & in){

};
void exportType(ASTType *type,std::ostream & out){
    SymbolTableCode c = DECLARE_TYPE;
    out.write((char *)&c,sizeof(c));
    SymTableID id {type->getName().data()};
    out << id;
};


std::shared_ptr<Semantics::SymbolTable> Semantics::SymbolTable::importPublic(std::istream & input){
    
    std::vector<ASTScope *> importedModuleScopes;
    
}

void Semantics::SymbolTable::serializePublic(std::ostream & out){
    /// First Serialize Dependencies.
    ///
    std::vector<ASTScope *> exportedModuleScopes;
    
    auto exportScopeIfNeeded = [&](ASTScope *subject){
        auto scopeExported = std::find_if(exportedModuleScopes.begin(),exportedModuleScopes.end(),[&](ASTScope *scope){
            return subject == scope;
        });
        /// If scope has not been exported yet.
        if(scopeExported == exportedModuleScopes.end()){
            renderScope(subject,out);
            exportedModuleScopes.push_back(subject);
        }
    };
    
    auto depCount = deps.size();
    out.write((char *)&depCount,sizeof(depCount));
    for(auto & d : deps){
        SymTableID id {d};
        out << id;
    }
    /// Then Serialize Scopes and All Public Symbols.
    SymbolTableCode c = TABLE_START;
    out.write((char *)&c,sizeof(c));
    for(auto & sym : body){
        auto _scope = sym.second->parentScope;
        for(;_scope != ASTScopeGlobal;_scope = _scope->parentScope){
            exportScopeIfNeeded(_scope);
        }
        exportScopeIfNeeded(sym.second);
        
        if(sym.first->type == Entry::Class){
            c = CLASS_ENTRY;
            out.write((char *)&c,sizeof(c));
            auto cl = ((Class *)sym.first->data);
            exportType(cl->classType,out);
            auto method_n = cl->instMethods.size();
        }
        else if(sym.first->type == Entry::Function){
            c = FUNC_ENTRY;
            out.write((char *)&c,sizeof(c));
            auto fn = ((Function *)sym.first->data);
            
        }
        
    }
    c = TABLE_END;
    out.write((char *)&c,sizeof(c));
}

void Semantics::SymbolTable::importModule(llvm::StringRef moduleName){
    deps.push_back(std::string(moduleName));
}

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

auto Semantics::SymbolTable::indexOf(llvm::StringRef symbolName,ASTScope *scope) -> decltype(body)::iterator {
    
    return std::find_if(body.begin(),body.end(),[&](llvm::detail::DenseMapPair<Entry *,ASTScope *> it){
        return it.first->name == symbolName && it.second == scope;
    });
    
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
