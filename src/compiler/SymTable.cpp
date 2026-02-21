#include "starbytes/compiler/SymTable.h"
#include "starbytes/compiler/SemanticA.h"
#include <algorithm>
#include <cstdlib>
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

namespace {

bool useLegacyLinearSymbolLookup() {
    static bool initialized = false;
    static bool useLegacy = false;
    if(!initialized) {
        const char *env = std::getenv("STARBYTES_DISABLE_SYMBOL_INDEX");
        useLegacy = env && env[0] != '\0' && env[0] != '0';
        initialized = true;
    }
    return useLegacy;
}

}

struct SymTableID {
    std::string id;
};

inline std::ostream &operator<<(std::ostream & out,SymTableID & id){
    auto size = id.id.size();
    out.write((char *)&size,sizeof(size));
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
    SymTableID id {type->getName().str()};
    out << id;
};


std::shared_ptr<Semantics::SymbolTable> Semantics::SymbolTable::importPublic(std::istream & input){
    (void)input;
    return std::make_shared<Semantics::SymbolTable>();
}

void Semantics::SymbolTable::serializePublic(std::ostream & out){
    /// First Serialize Dependencies.
    ///
    std::vector<std::shared_ptr<ASTScope>> exportedModuleScopes;
    
    auto exportScopeIfNeeded = [&](std::shared_ptr<ASTScope> & subject){
        if(!subject || subject == ASTScopeGlobal){
            return;
        }
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
        for(;_scope != nullptr && _scope != ASTScopeGlobal;_scope = _scope->parentScope){
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
    auto moduleNameStr = moduleName.str();
    if(std::find(deps.begin(),deps.end(),moduleNameStr) == deps.end()){
        deps.push_back(std::move(moduleNameStr));
    }
}

void Semantics::SymbolTable::addSymbolInScope(Entry *entry, std::shared_ptr<ASTScope> scope){
    body.insert(std::make_pair(entry,scope));
    if(scope){
        entriesByScope[scope.get()][entry->name].push_back(entry);
    }
    if(!entry->emittedName.empty()){
        entriesByEmittedName[entry->emittedName].push_back(entry);
    }
}

const std::vector<Semantics::SymbolTable::Entry *> *Semantics::SymbolTable::findEntriesInExactScope(
    string_ref symbolName,
    std::shared_ptr<ASTScope> scope) const{
    if(!scope){
        return nullptr;
    }
    auto scopeIt = entriesByScope.find(scope.get());
    if(scopeIt == entriesByScope.end()){
        return nullptr;
    }
    auto symIt = scopeIt->second.find(symbolName.str());
    if(symIt == scopeIt->second.end()){
        return nullptr;
    }
    return &symIt->second;
}

const std::vector<Semantics::SymbolTable::Entry *> *Semantics::SymbolTable::findEntriesByEmittedName(
    string_ref emittedName) const{
    auto it = entriesByEmittedName.find(emittedName.str());
    if(it == entriesByEmittedName.end()){
        return nullptr;
    }
    return &it->second;
}

bool Semantics::SymbolTable::symbolExists(string_ref symbolName,std::shared_ptr<ASTScope> scope){
    if(useLegacyLinearSymbolLookup()){
        for (auto s = scope; s != nullptr; s = s->parentScope) {
            for (auto & sym : body) {
                if (sym.first->name == symbolName && sym.second == s) {
                    return true;
                }
            }
        }
        return false;
    }
    for (auto s = scope; s != nullptr; s = s->parentScope) {
        auto *entries = findEntriesInExactScope(symbolName,s);
        if(entries && !entries->empty()){
            return true;
        }
    }
    return false;
}

auto Semantics::SymbolTable::indexOf(string_ref symbolName,std::shared_ptr<ASTScope> scope) -> decltype(body)::iterator {
    if(useLegacyLinearSymbolLookup()){
        return std::find_if(body.begin(),body.end(),[&](decltype(body)::value_type it){
            return it.first->name == symbolName && it.second == scope;
        });
    }
    auto *entries = findEntriesInExactScope(symbolName,scope);
    if(entries && !entries->empty()){
        auto it = body.find(entries->front());
        if(it != body.end()){
            return it;
        }
    }
    return body.end();
}

Semantics::SymbolTable::Entry * Semantics::STableContext::findEntry(string_ref symbolName,SemanticsContext & ctxt,std::shared_ptr<ASTScope> scope){
    if(useLegacyLinearSymbolLookup()){
        for(auto currentScope = scope; currentScope != nullptr; currentScope = currentScope->parentScope){
            unsigned entryCount = 0;
            Semantics::SymbolTable::Entry *ent = nullptr;

            if(main){
                for(auto &pair : main->body){
                    if(pair.first->name == symbolName && pair.second == currentScope){
                        ent = pair.first;
                        ++entryCount;
                    }
                }
            }
            for(auto &table : otherTables){
                for(auto &pair : table->body){
                    if(pair.first->name == symbolName && pair.second == currentScope){
                        ent = pair.first;
                        ++entryCount;
                    }
                }
            }

            if(entryCount > 1){
                std::ostringstream out;
                out <<"Multiple symbol defined with the same name (`" << symbolName << "`) in a scope.";
                auto res = out.str();
                ctxt.errStream.push(SemanticADiagnostic::create(res,ctxt.currentStmt,Diagnostic::Error));
                return nullptr;
            }
            if(entryCount == 1){
                return ent;
            }
        }

        std::ostringstream out;
        out <<"Undefined symbol `" << symbolName << "`";
        auto res = out.str();
        ctxt.errStream.push(SemanticADiagnostic::create(res,ctxt.currentStmt,Diagnostic::Error));
        return nullptr;
    }

    for(auto currentScope = scope; currentScope != nullptr; currentScope = currentScope->parentScope){
        std::vector<Semantics::SymbolTable::Entry *> matches;
        matches.reserve(4);

        if(main){
            auto *entries = main->findEntriesInExactScope(symbolName,currentScope);
            if(entries){
                matches.insert(matches.end(),entries->begin(),entries->end());
            }
        }
        for(auto &table : otherTables){
            auto *entries = table->findEntriesInExactScope(symbolName,currentScope);
            if(entries){
                matches.insert(matches.end(),entries->begin(),entries->end());
            }
        }

        if(matches.size() > 1){
            std::ostringstream out;
            out <<"Multiple symbol defined with the same name (`" << symbolName << "`) in a scope.";
            auto res = out.str();
            ctxt.errStream.push(SemanticADiagnostic::create(res,ctxt.currentStmt,Diagnostic::Error));
            return nullptr;
        }
        if(matches.size() == 1){
            return matches.front();
        }
    }

    std::ostringstream out;
    out <<"Undefined symbol `" << symbolName << "`";
    auto res = out.str();
    ctxt.errStream.push(SemanticADiagnostic::create(res,ctxt.currentStmt,Diagnostic::Error));
    return nullptr;
}

Semantics::SymbolTable::Entry * Semantics::STableContext::findEntryNoDiag(string_ref symbolName,std::shared_ptr<ASTScope> scope){
    if(useLegacyLinearSymbolLookup()){
        for(auto currentScope = scope; currentScope != nullptr; currentScope = currentScope->parentScope){
            if(main){
                for(auto &pair : main->body){
                    if(pair.first->name == symbolName && pair.second == currentScope){
                        return pair.first;
                    }
                }
            }
            for(auto &table : otherTables){
                for(auto &pair : table->body){
                    if(pair.first->name == symbolName && pair.second == currentScope){
                        return pair.first;
                    }
                }
            }
        }
        return nullptr;
    }

    for(auto currentScope = scope; currentScope != nullptr; currentScope = currentScope->parentScope){
        if(main){
            auto *entries = main->findEntriesInExactScope(symbolName,currentScope);
            if(entries && !entries->empty()){
                return entries->front();
            }
        }
        for(auto &table : otherTables){
            auto *entries = table->findEntriesInExactScope(symbolName,currentScope);
            if(entries && !entries->empty()){
                return entries->front();
            }
        }
    }
    return nullptr;
}

Semantics::SymbolTable::Entry * Semantics::STableContext::findEntryInExactScopeNoDiag(string_ref symbolName,std::shared_ptr<ASTScope> scope){
    if(useLegacyLinearSymbolLookup()){
        if(main){
            for(auto &pair : main->body){
                if(pair.first->name == symbolName && pair.second == scope){
                    return pair.first;
                }
            }
        }
        for(auto &table : otherTables){
            for(auto &pair : table->body){
                if(pair.first->name == symbolName && pair.second == scope){
                    return pair.first;
                }
            }
        }
        return nullptr;
    }

    if(main){
        auto *entries = main->findEntriesInExactScope(symbolName,scope);
        if(entries && !entries->empty()){
            return entries->front();
        }
    }
    for(auto &table : otherTables){
        auto *entries = table->findEntriesInExactScope(symbolName,scope);
        if(entries && !entries->empty()){
            return entries->front();
        }
    }
    return nullptr;
}

Semantics::SymbolTable::Entry * Semantics::STableContext::findEntryByEmittedNoDiag(string_ref emittedName){
    if(useLegacyLinearSymbolLookup()){
        if(main){
            for(auto &pair : main->body){
                if(pair.first->emittedName == emittedName){
                    return pair.first;
                }
            }
        }
        for(auto &table : otherTables){
            for(auto &pair : table->body){
                if(pair.first->emittedName == emittedName){
                    return pair.first;
                }
            }
        }
        return nullptr;
    }

    if(main){
        auto *entries = main->findEntriesByEmittedName(emittedName);
        if(entries && !entries->empty()){
            return entries->front();
        }
    }
    for(auto &table : otherTables){
        auto *entries = table->findEntriesByEmittedName(emittedName);
        if(entries && !entries->empty()){
            return entries->front();
        }
    }
    return nullptr;
}

Semantics::SymbolTable::~SymbolTable(){
    body.clear();
    entriesByScope.clear();
    entriesByEmittedName.clear();
    for(auto it = ownedAllocations.rbegin(); it != ownedAllocations.rend(); ++it){
        if(it->second && it->first){
            it->second(it->first);
        }
    }
    ownedAllocations.clear();
}

}
