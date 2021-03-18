#include "starbytes/Semantics/ScopeStoreIO.h"
#include "starbytes/Semantics/Symbols.h"
#include <fstream>

/**
ScopeStore IO

File Format: 

```
ScopeStoreInfo struct;

List[Scope] {
    List[Semantic Symbol]
}
```

*/

STARBYTES_SEMANTICS_NAMESPACE

struct ScopeStoreInfo {
    std::string libname;
    size_t scope_count;
};

struct ScopeInfo {
    std::string name;
    size_t symbol_count;
};

struct SemanticSymInfo {
    Semantics::SymbolType type;
};

void getScopeStore(llvm::StringRef file,ScopeStore & store_ref){
    
};

void dumpScopeStore(llvm::StringRef file,ScopeStore & store_ref,llvm::StringRef lib_name){
    std::ofstream out(file.str(),std::ios::out);
    llvm::MutableArrayRef<Scope *> scopes (store_ref.scopes);
    ScopeStoreInfo info {lib_name.str(),scopes.size()};

    out.write((char *)&info,sizeof(info));

    auto it_1 = scopes.begin();
    while(it_1 != scopes.end()){
        auto & scope_ref =  *it_1;
        
        ScopeInfo scope_info {scope_ref->name,scope_ref->symbols.size()};

        out.write((char *)&scope_info,sizeof(scope_info));

        llvm::MutableArrayRef<SemanticSymbol *> syms (scope_ref->symbols);
        auto it_2 = syms.begin();
        while(it_2 != syms.end()){
            auto & sym = *it_2;
            // TODO: Write SymInfo!
            // Write SemanticASymbol Data!
            ++it_2;
        };

        ++it_1;
    };
};

NAMESPACE_END