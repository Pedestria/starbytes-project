#include "../tools/lsp/ServerMain.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

void appendMessage(std::stringstream &stream, const std::string &json) {
    stream << "Content-Length: " << json.size() << "\r\n\r\n" << json;
}

bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    std::stringstream input;
    std::stringstream output;

    appendMessage(input, R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb","languageId":"starbytes","version":1,"text":"/// Adds two integers.\nfunc add(a:Int,b:Int) Int {\n  return a + b\n}\n/* Primary counter for calls. */\ndecl alpha:Int = 1\ndecl greeting:String = \" Hello \"\ndecl numbers:Array<Int> = [1,2,3]\nadd(alpha, 2)\nalpha\ngreeting.trim()\nnumbers.push(4)\n/// Computes square.\n/// @param value Input value.\n/// @returns Squared value.\nfunc square(value:Int) Int {\n  return value * value\n}\nsquare(4)\nsecure(decl captured:String? = \"ok\") catch (err:String) {\n  print(err)\n}\n"}}})");
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

    if(!requireContains("\"id\":1","initialize-id") || !requireContains("\"capabilities\"","initialize-capabilities")) return 1;
    if(!requireContains("\"id\":2","completion-id") || !requireContains("\"completionProvider\"","completion-provider")) return 1;
    if(!requireContains("\"id\":3","hover-id") || !requireContains("\"contents\"","hover-contents") || !requireContains("Primary counter for calls.","hover-doc")) return 1;
    if(!requireContains("\"id\":4","definition-id") || !requireContains("\"uri\":\"file:///tmp/lsp-test.starb\"","definition-uri")) return 1;
    if(!requireContains("\"id\":5","semantic-full-id") || !requireContains("\"resultId\"","semantic-full-result-id")) return 1;
    if(!requireContains("\"id\":6","semantic-range-id") || !requireContains("\"data\":[","semantic-range-data")) return 1;
    if(!requireContains("\"id\":7","member-completion-id") || !requireContains("\"label\":\"trim\"","member-completion-trim")) return 1;
    if(!requireContains("\"id\":8","member-hover-id") || !requireContains("func trim() String","member-hover-signature")) return 1;
    if(!requireContains("\"id\":9","member-definition-id") || !requireContains("builtins.starbint","member-definition-uri")) return 1;
    if(!requireContains("\"id\":10","signature-help-id") || !requireContains("push(value:T)","signature-help-push")) return 1;
    if(!requireContains("\"source\":\"starbytes-compiler\"","compiler-diagnostics-source")) return 1;
    if(!requireContains("\"id\":12","doxygen-hover-id") || !requireContains("**Parameters**","doxygen-hover-params") || !requireContains("**Returns**","doxygen-hover-returns")) return 1;
    if(!requireContains("\"id\":13","completion-resolve-id") || !requireContains("func trim() String","completion-resolve-signature")) return 1;
    if(!requireContains("\"id\":14","text-document-diagnostic-id") || !requireContains("\"kind\":\"full\"","text-document-diagnostic-kind")) return 1;
    if(!requireContains("\"id\":15","workspace-diagnostic-id") || !requireContains("\"items\"","workspace-diagnostic-items")) return 1;
    if(!requireContains("\"id\":17","catch-hover-id") || !requireContains("catch err:String","catch-hover-signature")) return 1;
    if(!requireContains("\"id\":18","catch-definition-id") || !requireContains("\"uri\":\"file:///tmp/lsp-test.starb\"","catch-definition-uri")) return 1;
    if(!requireContains("\"id\":16","shutdown-id") || !requireContains("\"result\":null","shutdown-null")) return 1;

    return 0;
}
