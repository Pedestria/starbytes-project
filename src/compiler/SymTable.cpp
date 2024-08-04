#include "starbytes/compiler/SymTable.h"
#include "starbytes/compiler/SemanticA.h"
#include <sstream>

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

void renderScope(std::shared_ptr<ASTScope> scope,std::ostream & out){
    SymbolTableCode code = DECLARE_SCOPE;
    out.write((char *)&code,sizeof(code));
    SymTableID id {scope->name};
    out << id;
    id = {scope->parentScope->name};
    out << id;
}

void importScope(ASTScope **scope,array_ref<std::shared_ptr<ASTScope>> importedModuleScopes,std::istream & in){
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
        auto _parent_scope = std::find_if(importedModuleScopes.begin(),importedModuleScopes.end(),[&](std::shared_ptr<ASTScope> object){
            return object->name == id.id;
        });
        (*scope)->parentScope = *_parent_scope;
    }
}

void importType(ASTType **type,std::istream & in){
    SymbolTableCode code;
    in.read((char *)&code,sizeof(code));
    SymTableID id{};
    in >> id;
    /// Recrete type (Parent node is not nesccary. Only when code needs to be viewed will it show location)
    *type = ASTType::Create(id.id,nullptr);
};
void exportType(ASTType *type,std::ostream & out){
    SymbolTableCode c = DECLARE_TYPE;
    out.write((char *)&c,sizeof(c));
    SymTableID id {type->getName().getBuffer()};
    out << id;
};


std::shared_ptr<Semantics::SymbolTable> Semantics::SymbolTable::importPublic(std::istream & input){
    
    std::vector<ASTScope *> importedModuleScopes;
    
}

void Semantics::SymbolTable::serializePublic(std::ostream & out){
    /// First Serialize Dependencies.
    ///
    std::vector<std::shared_ptr<ASTScope>> exportedModuleScopes;
    
    auto exportScopeIfNeeded = [&](std::shared_ptr<ASTScope> & subject){
        auto scopeExported = std::find_if(exportedModuleScopes.begin(),exportedModuleScopes.end(),[&](std::shared_ptr<ASTScope> scope){
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

void Semantics::SymbolTable::importModule(string_ref moduleName){
    deps.push_back(moduleName.getBuffer());
}

void Semantics::SymbolTable::addSymbolInScope(Entry *entry, std::shared_ptr<ASTScope> scope){
    body.insert(std::make_pair(entry,scope));
}

bool Semantics::SymbolTable::symbolExists(string_ref symbolName,std::shared_ptr<ASTScope> scope){
    for(auto & sym : body){
        if(sym.first->name == symbolName && sym.second == scope){
            return true;
        };
    };
    return false;
}

auto Semantics::SymbolTable::indexOf(string_ref symbolName,std::shared_ptr<ASTScope> scope) -> decltype(body)::iterator {
    
    return std::find_if(body.begin(),body.end(),[&](decltype(body)::value_type it){
        return it.first->name == symbolName && it.second == scope;
    });
    
}

Semantics::SymbolTable::Entry * Semantics::STableContext::findEntry(string_ref symbolName,SemanticsContext & ctxt,std::shared_ptr<ASTScope> scope){
    unsigned entryCount = 0;
    Semantics::SymbolTable::Entry *ent = nullptr;
    for(auto & pair : main->body){
        // std::cout << "SCOPE PTR 1:" << pair.getSecond() << std::endl << "SCOPE PTR 2" << scope << std::endl;
        if(pair.first->name == symbolName && pair.second == scope){
            ent = pair.first;
            ++entryCount;
        };
    };
    
    for(auto & table : otherTables){
        for(auto & pair : table->body){
            if(pair.first->name == symbolName && pair.second == scope){
                ent = pair.first;
                ++entryCount;
            };
        };
    };
    
    if(entryCount > 1){
        std::ostringstream out;
        out <<"Multiple symbol defined with the same name (`" << symbolName << "`) in a scope.";
        auto res = out.str();
        ctxt.errStream.push(SemanticADiagnostic::create(res,ctxt.currentStmt,Diagnostic::Error));
        return nullptr;
    }
    else if(entryCount == 0){
         std::ostringstream out;
        out <<"Undefined symbol `" << symbolName << "`";
        auto res = out.str();
        ctxt.errStream.push(SemanticADiagnostic::create(res,ctxt.currentStmt,Diagnostic::Error));
        return nullptr;
    };
    
    return ent;
}

Semantics::SymbolTable::~SymbolTable(){
    for(auto & e : body){
        delete e.first;
    };
    body.clear();
}

}
