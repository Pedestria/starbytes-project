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
    appendMessage(input, R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb","languageId":"starbytes","version":1,"text":"decl alpha:Int = 1\nfunc add(a:Int,b:Int) Int {\n  return a + b\n}\nadd(alpha, 2)\n"}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":0,"character":4}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":4,"character":5}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":4,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"},"position":{"line":4,"character":1}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":5,"method":"textDocument/semanticTokens/full","params":{"textDocument":{"uri":"file:///tmp/lsp-test.starb"}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":6,"method":"shutdown","params":null})");
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
    if(!contains(result, "\"id\":3") || !contains(result, "\"contents\"")) {
        return 1;
    }
    if(!contains(result, "\"id\":4") || !contains(result, "\"uri\":\"file:///tmp/lsp-test.starb\"")) {
        return 1;
    }
    if(!contains(result, "\"id\":5") || !contains(result, "\"resultId\"")) {
        return 1;
    }
    if(!contains(result, "\"id\":6") || !contains(result, "\"result\":null")) {
        return 1;
    }

    return 0;
}
