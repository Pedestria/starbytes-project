#include "starbytes/compiler/AST.h"
#include "starbytes/compiler/Parser.h"
#include "starbytes/compiler/SymTable.h"
#include "starbytes/compiler/Type.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

class NullConsumer final : public starbytes::ASTStreamConsumer {
public:
    bool acceptsSymbolTableContext() override {
        return true;
    }

    void consumeSTableContext(starbytes::Semantics::STableContext *) override {}
    void consumeStmt(starbytes::ASTStmt *) override {}
    void consumeDecl(starbytes::ASTDecl *) override {}
};

int fail(const char *message) {
    std::cerr << "SemanticUnusedBindingsTest failure: " << message << '\n';
    return 1;
}

std::shared_ptr<starbytes::Semantics::SymbolTable> makeImportedModuleTable(const std::string &functionName,
                                                                           starbytes::ASTType *returnType) {
    auto table = std::make_shared<starbytes::Semantics::SymbolTable>();
    auto *entry = table->allocate<starbytes::Semantics::SymbolTable::Entry>();
    auto *func = table->allocate<starbytes::Semantics::SymbolTable::Function>();
    entry->name = functionName;
    entry->emittedName = functionName;
    entry->type = starbytes::Semantics::SymbolTable::Entry::Function;
    entry->data = func;
    func->name = functionName;
    func->returnType = returnType;
    func->funcType = nullptr;
    table->addSymbolInScope(entry,starbytes::ASTScopeGlobal);
    return table;
}

size_t countMessageFragment(const std::string &text,
                            const std::string &fragment) {
    size_t count = 0;
    size_t cursor = 0;
    while(true) {
        cursor = text.find(fragment,cursor);
        if(cursor == std::string::npos) {
            break;
        }
        ++count;
        cursor += fragment.size();
    }
    return count;
}

} // namespace

int main() {
    const char *source = R"starb(
import Time
import Math

func sample(input:Int, unusedParam:Int, _ignored:Int) Int {
    decl usedLocal:Int = input
    decl unusedLocal:Int = 1
    decl _tick = Time.nowInt()
    return usedLocal
}

scope Tools {
    import Unicode
    import CmdLine

    func inner(value:Int, _ignored:Int) Int {
        decl _args = CmdLine.positionals()
        return value
    }
}
)starb";

    std::ostringstream diagOut;
    auto diagnostics = starbytes::DiagnosticHandler::createDefault(diagOut);
    if(!diagnostics) {
        return fail("unable to create DiagnosticHandler");
    }

    NullConsumer consumer;
    starbytes::Parser parser(consumer,std::move(diagnostics));
    auto parseContext = starbytes::ModuleParseContext::Create("SemanticUnusedBindingsTest");
    auto timeTable = makeImportedModuleTable("nowInt",starbytes::INT_TYPE);
    auto *arrayReturnType = starbytes::ASTType::Create(starbytes::ARRAY_TYPE->getName(),nullptr,false,false);
    arrayReturnType->addTypeParam(starbytes::ASTType::Create(starbytes::STRING_TYPE->getName(),nullptr,false,false));
    auto cmdlineTable = makeImportedModuleTable("positionals",arrayReturnType);
    parseContext.sTableContext.importTables.push_back(timeTable);
    parseContext.sTableContext.importTables.push_back(cmdlineTable);
    parseContext.sTableContext.otherTables.push_back(timeTable->createImportNamespaceOverlay("Time"));
    parseContext.sTableContext.otherTables.push_back(cmdlineTable->createImportNamespaceOverlay("CmdLine"));
    std::istringstream in(source);
    parser.parseFromStream(in,parseContext);
    if(!parser.finish()) {
        std::cerr << diagOut.str();
        return fail("parser reported unexpected hard diagnostics");
    }

    auto rendered = diagOut.str();

    if(countMessageFragment(rendered,"warning:") != 4) {
        std::cerr << rendered;
        return fail("expected exactly four semantic warnings");
    }
    if(countMessageFragment(rendered,"Imported module `Math` is unused.") != 1) {
        return fail("expected exactly one unused import warning for Math");
    }
    if(countMessageFragment(rendered,"Imported module `Unicode` is unused.") != 1) {
        return fail("expected exactly one unused import warning for Unicode");
    }
    if(countMessageFragment(rendered,"Parameter `unusedParam` is unused.") != 1) {
        return fail("expected exactly one unused parameter warning");
    }
    if(countMessageFragment(rendered,"Local binding `unusedLocal` is unused.") != 1) {
        return fail("expected exactly one unused local warning");
    }
    if(countMessageFragment(rendered,"Local binding `usedLocal` is unused.") != 0) {
        return fail("used local should not warn");
    }
    if(countMessageFragment(rendered,"Parameter `input` is unused.") != 0) {
        return fail("used parameter should not warn");
    }
    if(countMessageFragment(rendered,"Parameter `_ignored` is unused.") != 0) {
        return fail("underscore parameter should be suppressed");
    }
    if(countMessageFragment(rendered,"Imported module `Time` is unused.") != 0) {
        return fail("used import Time should not warn");
    }
    if(countMessageFragment(rendered,"Imported module `CmdLine` is unused.") != 0) {
        return fail("used import CmdLine should not warn");
    }

    return 0;
}
