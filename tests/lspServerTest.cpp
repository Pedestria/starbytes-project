#include "../tools/lsp/ServerMain.h"

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
    appendMessage(input, R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb","languageId":"starbytes","version":1,"text":"/// Adds two integers.\nfunc add(a:Int,b:Int) Int {\n  return a + b\n}\n/* Primary counter for calls. */\ndecl alpha:Int = 1\nadd(alpha, 2)\nalpha\n"}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":5,"character":4}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":6,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":4,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":6,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":5,"method":"textDocument/semanticTokens/full","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":6,"method":"textDocument/semanticTokens/range","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"range":{"start":{"line":6,"character":0},"end":{"line":6,"character":13}}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":7,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":7,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":8,"method":"shutdown","params":null})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"exit"})");

    starbytes::lsp::ServerOptions opts{input, output};
    starbytes::lsp::Server server(opts);
    server.run();

    auto result = output.str();

    if(!contains(result, "\"id\":1") || !contains(result, "\"capabilities\"")) {
        return 1;
    }
    if(!contains(result, "\"id\":2") || !contains(result, "\"completionProvider\"")) {
        return 1;
    }
    if(!contains(result, "\"id\":3") || !contains(result, "\"contents\"") || !contains(result, "Adds two integers.")) {
        return 1;
    }
    if(!contains(result, "\"id\":4") || !contains(result, "\"uri\":\"file:///tmp/lsp-test.starb\"")) {
        return 1;
    }
    if(!contains(result, "\"id\":5") || !contains(result, "\"resultId\"")) {
        return 1;
    }
    if(!contains(result, "\"id\":6") || !contains(result, "\"data\":[6,0,3,2,0,0,4,5,3,0]")) {
        return 1;
    }
    if(!contains(result, "\"id\":7") || !contains(result, "Primary counter for calls.")) {
        return 1;
    }
    if(!contains(result, "\"id\":8") || !contains(result, "\"result\":null")) {
        return 1;
    }

    return 0;
}
