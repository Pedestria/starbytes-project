#include "starbytes/compiler/SymTable.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const char *message) {
    std::cerr << "SymbolLookupStressTest failure: " << message << '\n';
    return 1;
}

}

int main() {
    using namespace starbytes;

    auto table = std::make_unique<Semantics::SymbolTable>();
    std::shared_ptr<ASTScope> namespaceScope(new ASTScope{"LookupScope", ASTScope::Namespace, ASTScopeGlobal});
    namespaceScope->generateHashID();
    std::shared_ptr<ASTScope> nestedScope(new ASTScope{"NestedLookupScope", ASTScope::Namespace, namespaceScope});
    nestedScope->generateHashID();

    constexpr int symbolCount = 8000;
    for(int i = 0; i < symbolCount; ++i) {
        auto *entry = table->allocate<Semantics::SymbolTable::Entry>();
        auto *var = table->allocate<Semantics::SymbolTable::Var>();
        entry->name = "sym_" + std::to_string(i);
        entry->emittedName = "LookupScope.sym_" + std::to_string(i);
        entry->type = Semantics::SymbolTable::Entry::Var;
        entry->data = var;
        var->name = entry->name;
        var->type = INT_TYPE;
        var->isReadonly = (i % 5) == 0;

        table->addSymbolInScope(entry, (i % 2) == 0 ? namespaceScope : nestedScope);
    }

    Semantics::STableContext context;
    context.main = std::move(table);

    if(!context.main->symbolExists("sym_0", namespaceScope)) {
        return fail("expected symbol to exist in namespace scope");
    }

    auto *exactEntry = context.findEntryInExactScopeNoDiag("sym_3", nestedScope);
    if(!exactEntry || exactEntry->name != "sym_3") {
        return fail("failed exact scope lookup");
    }

    auto *emittedEntry = context.findEntryByEmittedNoDiag("LookupScope.sym_6");
    if(!emittedEntry || emittedEntry->name != "sym_6") {
        return fail("failed emitted-name lookup");
    }

    // Lookup-heavy pass intended to exercise phase-3 indexed symbol resolution.
    unsigned long long checksum = 0;
    for(int pass = 0; pass < 150; ++pass) {
        for(int i = 0; i < symbolCount; ++i) {
            auto queryName = std::string("sym_") + std::to_string(i);
            auto *entry = context.findEntryNoDiag(string_ref(queryName), nestedScope);
            if(!entry) {
                return fail("failed scoped lookup in stress pass");
            }
            checksum += static_cast<unsigned long long>(entry->name.size());
        }
    }

    if(checksum == 0) {
        return fail("lookup checksum was unexpectedly zero");
    }

    return 0;
}
