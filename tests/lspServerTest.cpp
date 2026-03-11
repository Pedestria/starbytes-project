#include "../tools/lsp/ServerMain.h"

#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

namespace {

void appendMessage(std::stringstream &stream, const std::string &json) {
    stream << "Content-Length: " << json.size() << "\r\n\r\n" << json;
}

bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string jsonEscape(const std::string &value) {
    std::string out;
    out.reserve(value.size() + 16);
    for(char ch : value) {
        switch(ch) {
            case '\"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    std::stringstream input;
    std::stringstream output;

    const auto rootUri = std::string("file://") + std::filesystem::current_path().string();
    const std::string cmdLineUri = "file:///tmp/lsp-cmdline-test.starb";
    const std::string renameUri = "file:///tmp/lsp-rename-test.starb";
    const std::string cmdLineText =
        "import CmdLine\n"
        "decl args = CmdLine.positionals()\n"
        "CmdLine.argCount()\n"
        "args\n"
        "args.\n";
    const std::string renameText =
        "func first(arg:Int) Int {\n"
        "  decl local:Int = arg\n"
        "  return local\n"
        "}\n"
        "func second(local:Int) Int {\n"
        "  return local\n"
        "}\n";

    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"rootUri":")") +
                         rootUri + R"(","workspaceFolders":[{"uri":")" + rootUri + R"(","name":"starbytes-project"}]}})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///tmp/Helper/main.starb","languageId":"starbytes","version":1,"text":"/// Shared helper docs.\nfunc helperValue() Int {\n  return 7\n}\n"}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb","languageId":"starbytes","version":1,"text":"/// Adds two integers.\nfunc add(a:Int,b:Int) Int {\n  return a + b\n}\n/* Primary counter for calls. */\ndecl alpha = 1   \ndecl greeting:String = \" Hello \"\ndecl numbers:Array<Int> = [1,2,3]\nadd(alpha, 2)\nalpha\ngreeting.trim()\nnumbers.push(4)\n/// Computes square.\n/// @param value Input value.\n/// @returns Squared value.\nfunc square(value:Int) Int {\n  return value * value\n}\nsquare(4)\nsecure(decl captured:String? = \"ok\") catch (err:String) {\n  print(err)\n}\nclass Base {\n}\ninterface Runnable {\n}\nclass Worker : Base, Runnable {\n}\ndecl worker = new Worker()\nworker\nimport Helper\nscope Tools {\n  /// Scope helper docs.\n  func ping() Int {\n    return 1\n  }\n}\nclass Box {\n  decl value:Int = 0\n  /// Reads the boxed value.\n  func read() Int {\n    return self.value\n  }\n}\nfunc scoped(alpha:Int) Int {\n  decl beta:Int = alpha\n  return beta\n}\nHelper.helperValue()\nTools.ping()\ndecl box = new Box()\nbox.read()\nclass Message {\n}\nclass Response {\n}\ndef HandlerAlias = (msg:Message) Response\nscope AliasScope {\n  def ScopedResponse = Response\n  def ScopedAlias = ScopedResponse\n}\ndecl handlerValue:HandlerAlias\n"}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")") +
                         cmdLineUri + R"(","languageId":"starbytes","version":1,"text":")" + jsonEscape(cmdLineText) + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")") +
                         renameUri + R"(","languageId":"starbytes","version":1,"text":")" + jsonEscape(renameText) + R"("}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":5,"character":4}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":9,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":4,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":9,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":5,"method":"textDocument/semanticTokens/full","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":6,"method":"textDocument/semanticTokens/range","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"range":{"start":{"line":9,"character":0},"end":{"line":9,"character":5}}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":7,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":10,"character":11}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":8,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":10,"character":10}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":9,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":10,"character":10}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":10,"method":"textDocument/signatureHelp","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":11,"character":13}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":12,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":18,"character":2}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":13,"method":"completionItem/resolve","params":{"label":"trim","kind":2}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":14,"method":"textDocument/diagnostic","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":15,"method":"workspace/diagnostic","params":{}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":17,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":20,"character":8}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":18,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":20,"character":8}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":19,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":26,"character":8}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":20,"method":"textDocument/formatting","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"options":{"tabSize":2,"insertSpaces":true}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":21,"method":"textDocument/codeAction","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"range":{"start":{"line":5,"character":0},"end":{"line":5,"character":24}},"context":{"diagnostics":[]}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":22,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":45,"character":20}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":23,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":46,"character":10}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":24,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":48,"character":10}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":25,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":49,"character":7}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":26,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":41,"character":17}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":27,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":51,"character":5}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":28,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":46,"character":10}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":29,"method":"textDocument/references","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":46,"character":10},"context":{"includeDeclaration":true}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":30,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":48,"character":10}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":31,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":49,"character":7}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":32,"method":"textDocument/references","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":38,"character":9},"context":{"includeDeclaration":true}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":33,"method":"textDocument/implementation","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":24,"character":11}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":34,"method":"textDocument/implementation","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":22,"character":8}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":35,"method":"textDocument/references","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":48,"character":10},"context":{"includeDeclaration":true}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":36,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":46,"character":12}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":37,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":48,"character":10}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":38,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":49,"character":8}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":39,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":51,"character":6}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":40,"method":"completionItem/resolve","params":{"label":"beta","kind":6,"detail":"variable","data":{"uri":"file:///tmp/lsp-test.starb","symbolLine":45,"symbolStart":7,"symbolLength":4,"symbolKind":13,"detail":"variable","signature":"decl beta:Int","documentation":"","containerName":"","isMember":false}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":41,"method":"textDocument/diagnostic","params":{"textDocument":{"uri":")") +
                         cmdLineUri + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":42,"method":"textDocument/hover","params":{"textDocument":{"uri":")") +
                         cmdLineUri + R"("},"position":{"line":2,"character":10}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":43,"method":"textDocument/hover","params":{"textDocument":{"uri":")") +
                         cmdLineUri + R"("},"position":{"line":3,"character":1}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":44,"method":"textDocument/completion","params":{"textDocument":{"uri":")") +
                         cmdLineUri + R"("},"position":{"line":4,"character":5}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":45,"method":"textDocument/declaration","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":51,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":46,"method":"textDocument/typeDefinition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":51,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":47,"method":"textDocument/typeDefinition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":56,"character":6}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":48,"method":"textDocument/typeDefinition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":59,"character":8}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":49,"method":"textDocument/prepareRename","params":{"textDocument":{"uri":")") +
                         renameUri + R"("},"position":{"line":2,"character":10}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":50,"method":"textDocument/rename","params":{"textDocument":{"uri":")") +
                         renameUri + R"("},"position":{"line":2,"character":10},"newName":"renamed"}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":51,"method":"textDocument/prepareRename","params":{"textDocument":{"uri":")") +
                         cmdLineUri + R"("},"position":{"line":2,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":52,"method":"textDocument/rename","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":48,"character":10},"newName":"helperAnswer"}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":16,"method":"shutdown","params":null})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"exit"})");

    starbytes::lsp::ServerOptions opts{input, output};
    starbytes::lsp::Server server(opts);
    server.run();

    auto result = output.str();
    auto requireContains = [&](const std::string &needle, const char *label) -> bool {
        if(contains(result, needle)) {
            return true;
        }
        std::cerr << "Missing check [" << label << "]: " << needle << "\n";
        std::cerr << result << std::endl;
        return false;
    };
    auto requireNotContains = [&](const std::string &needle, const char *label) -> bool {
        if(!contains(result, needle)) {
            return true;
        }
        std::cerr << "Unexpected match [" << label << "]: " << needle << "\n";
        std::cerr << result << std::endl;
        return false;
    };

    if(!requireContains("\"id\":1","initialize-id") || !requireContains("\"capabilities\"","initialize-capabilities")) return 1;
    if(!requireContains("\"id\":2","completion-id") || !requireContains("\"completionProvider\"","completion-provider")) return 1;
    if(!requireContains("\"id\":3","hover-id") || !requireContains("\"contents\"","hover-contents")
       || !requireContains("Primary counter for calls.","hover-doc")
       || !requireContains("decl alpha:Int","hover-inferred-type")) return 1;
    if(!requireContains("\"id\":4","definition-id") || !requireContains("\"uri\":\"file:///tmp/lsp-test.starb\"","definition-uri")) return 1;
    if(!requireContains("\"id\":5","semantic-full-id") || !requireContains("\"resultId\"","semantic-full-result-id")) return 1;
    if(!requireContains("\"id\":6","semantic-range-id") || !requireContains("\"data\":[","semantic-range-data")) return 1;
    if(!requireContains("\"id\":7","member-completion-id") || !requireContains("\"label\":\"trim\"","member-completion-trim")) return 1;
    if(!requireContains("\"id\":8","member-hover-id") || !requireContains("func trim() String","member-hover-signature")) return 1;
    if(!requireContains("\"id\":9","member-definition-id") || !requireContains("builtins.starbint","member-definition-uri")) return 1;
    if(!requireContains("\"id\":10","signature-help-id") || !requireContains("push(value:T)","signature-help-push")) return 1;
    if(!requireContains("\"source\":\"starbytes-compiler\"","compiler-diagnostics-source")) return 1;
    if(!requireContains("\"source\":\"starbytes-linguistics\"","linguistics-diagnostics-source")) return 1;
    if(!requireContains("\"code\":\"SB-","compiler-diagnostics-code-family")) return 1;
    if(!requireContains("\"phase\":\"","compiler-diagnostics-phase")) return 1;
    if(!requireContains("\"id\":12","doxygen-hover-id") || !requireContains("**Parameters**","doxygen-hover-params") || !requireContains("**Returns**","doxygen-hover-returns")) return 1;
    if(!requireContains("\"id\":13","completion-resolve-id") || !requireContains("func trim() String","completion-resolve-signature")) return 1;
    if(!requireContains("\"id\":14","text-document-diagnostic-id") || !requireContains("\"kind\":\"full\"","text-document-diagnostic-kind")) return 1;
    if(!requireContains("\"id\":15","workspace-diagnostic-id") || !requireContains("\"items\"","workspace-diagnostic-items")) return 1;
    if(!requireContains("\"id\":17","catch-hover-id") || !requireContains("catch err:String","catch-hover-signature")) return 1;
    if(!requireContains("\"id\":18","catch-definition-id") || !requireContains("\"uri\":\"file:///tmp/lsp-test.starb\"","catch-definition-uri")) return 1;
    if(!requireContains("\"id\":19","inheritance-hover-id") || !requireContains("**Inheritance**","inheritance-hover-section")
       || !requireContains("Superclasses","inheritance-hover-superclasses")
       || !requireContains("Interfaces","inheritance-hover-interfaces")
       || !requireContains("`Base`","inheritance-hover-base")
       || !requireContains("`Runnable`","inheritance-hover-interface")) return 1;
    if(!requireContains("\"id\":20","formatting-id") || !requireContains("\"newText\"","formatting-new-text")) return 1;
    if(!requireContains("\"id\":21","code-action-id") || !requireContains("\"kind\":\"quickfix\"","code-action-kind")
       || !requireContains("Trim trailing whitespace","code-action-trim-title")) return 1;
    if(!requireContains("\"id\":22","function-param-hover-id") || !requireContains("param alpha:Int","function-param-hover-signature")) return 1;
    if(!requireContains("\"id\":23","local-hover-id") || !requireContains("decl beta:Int","local-hover-signature")) return 1;
    if(!requireContains("\"id\":24","imported-module-hover-id") || !requireContains("func helperValue() Int","imported-module-hover-signature")
       || !requireContains("Shared helper docs.","imported-module-hover-doc")) return 1;
    if(!requireContains("\"id\":25","scope-hover-id") || !requireContains("func ping() Int","scope-hover-signature")
       || !requireContains("Scope helper docs.","scope-hover-doc")) return 1;
    if(!requireContains("\"id\":26","class-field-hover-id") || !requireContains("decl value:Int","class-field-hover-signature")) return 1;
    if(!requireContains("\"id\":27","class-method-hover-id") || !requireContains("func read() Int","class-method-hover-signature")
       || !requireContains("Reads the boxed value.","class-method-hover-doc")) return 1;
    if(!requireContains("\"id\":28","local-definition-id") || !requireContains("\"line\":45,\"character\":7","local-definition-range")) return 1;
    if(!requireContains("\"id\":29","local-references-id") || !requireContains("\"line\":45,\"character\":7","local-references-decl")
       || !requireContains("\"line\":46,\"character\":9","local-references-use")) return 1;
    if(!requireContains("\"id\":30","imported-definition-id") || !requireContains("\"uri\":\"file:///tmp/Helper/main.starb\"","imported-definition-uri")
       || !requireContains("\"line\":1,\"character\":5","imported-definition-range")) return 1;
    if(!requireContains("\"id\":31","scope-definition-id") || !requireContains("\"line\":33,\"character\":7","scope-definition-range")) return 1;
    if(!requireContains("\"id\":32","field-references-id") || !requireContains("\"line\":38,\"character\":7","field-references-decl")
       || !requireContains("\"line\":41,\"character\":16","field-references-use")) return 1;
    if(!requireContains("\"id\":33","interface-implementation-id") || !requireContains("\"line\":26,\"character\":6","interface-implementation-range")) return 1;
    if(!requireContains("\"id\":34","class-implementation-id") || !requireContains("\"line\":26,\"character\":6","class-implementation-range")) return 1;
    if(!requireContains("\"id\":35","cross-file-references-id") || !requireContains("\"uri\":\"file:///tmp/Helper/main.starb\"","cross-file-references-uri")
       || !requireContains("\"line\":48,\"character\":7","cross-file-references-use")) return 1;
    if(!requireContains("\"id\":36","scoped-completion-local-id") || !requireContains("\"label\":\"beta\"","scoped-completion-local-label")) return 1;
    if(!requireContains("\"id\":37","scoped-completion-module-id") || !requireContains("\"label\":\"helperValue\"","scoped-completion-module-label")) return 1;
    if(!requireContains("\"id\":38","scoped-completion-scope-id") || !requireContains("\"label\":\"ping\"","scoped-completion-scope-label")) return 1;
    if(!requireContains("\"id\":39","scoped-completion-member-id") || !requireContains("\"label\":\"read\"","scoped-completion-member-label")) return 1;
    if(!requireContains("\"id\":40","scoped-completion-resolve-id") || !requireContains("decl beta:Int","scoped-completion-resolve-signature")) return 1;
    if(!requireContains("\"id\":41","cmdline-diagnostic-id")) return 1;
    if(!requireContains("\"id\":42","cmdline-hover-id") || !requireContains("func argCount() Int","cmdline-hover-signature")) return 1;
    if(!requireContains("\"id\":43","cmdline-var-hover-id") || !requireContains("decl args:Array<String>","cmdline-var-hover-signature")) return 1;
    if(!requireContains("\"id\":44","cmdline-member-completion-id") || !requireContains("\"label\":\"push\"","cmdline-member-completion-push")) return 1;
    if(!requireContains("\"id\":45","declaration-id") || !requireContains("\"line\":50,\"character\":5","declaration-range")) return 1;
    if(!requireContains("\"id\":46","type-definition-id") || !requireContains("\"line\":37,\"character\":6","type-definition-range")) return 1;
    if(!requireContains("\"id\":47","function-alias-type-definition-id") || !requireContains("\"line\":52,\"character\":6","function-alias-message-range")
       || !requireContains("\"line\":54,\"character\":6","function-alias-response-range")) return 1;
    if(!requireContains("\"id\":48","qualified-alias-type-definition-id") || !requireContains("\"line\":58,\"character\":6","qualified-alias-scope-range")
       || !requireContains("\"line\":54,\"character\":6","qualified-alias-response-range")) return 1;
    if(!requireContains("\"id\":49","prepare-rename-local-id") || !requireContains("\"placeholder\":\"local\"","prepare-rename-local-placeholder")
       || !requireContains("\"line\":2,\"character\":9","prepare-rename-local-range")) return 1;
    if(!requireContains("\"id\":50","rename-local-id") || !requireContains("\"file:///tmp/lsp-rename-test.starb\"","rename-local-uri")
       || !requireContains("\"line\":1,\"character\":7","rename-local-decl-edit")
       || !requireContains("\"line\":2,\"character\":9","rename-local-use-edit")
       || !requireNotContains("\"line\":4,\"character\":12},\"end\":{\"line\":4,\"character\":17}},\"newText\":\"renamed\"","rename-local-param-not-edited")
       || !requireNotContains("\"line\":5,\"character\":9},\"end\":{\"line\":5,\"character\":14}},\"newText\":\"renamed\"","rename-local-shadow-use-not-edited")) return 1;
    if(!requireContains("\"id\":51","prepare-rename-module-id") || !requireContains("\"result\":null","prepare-rename-module-null")) return 1;
    if(!requireContains("\"id\":52","rename-cross-file-id") || !requireContains("\"file:///tmp/Helper/main.starb\"","rename-cross-file-helper-uri")
       || !requireContains("\"file:///tmp/lsp-test.starb\"","rename-cross-file-main-uri")
       || !requireContains("\"line\":1,\"character\":5","rename-cross-file-helper-edit")
       || !requireContains("\"line\":48,\"character\":7","rename-cross-file-main-edit")) return 1;
    if(contains(result, "Undefined symbol `CmdLine`")) {
        std::cerr << "CmdLine import should resolve in LSP diagnostics.\n";
        std::cerr << result << std::endl;
        return 1;
    }
    if(!requireContains("\"id\":16","shutdown-id") || !requireContains("\"result\":null","shutdown-null")) return 1;

    return 0;
}
