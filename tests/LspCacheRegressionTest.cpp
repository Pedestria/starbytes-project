#include "../tools/lsp/ServerMain.h"

#include <rapidjson/document.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void appendMessage(std::stringstream &stream, const std::string &json) {
    stream << "Content-Length: " << json.size() << "\r\n\r\n" << json;
}

bool parseLspOutputMessages(const std::string &output, std::vector<rapidjson::Document> &messagesOut) {
    size_t cursor = 0;
    while(cursor < output.size()) {
        auto headerPos = output.find("Content-Length:", cursor);
        if(headerPos == std::string::npos) {
            break;
        }

        auto lineEnd = output.find("\r\n", headerPos);
        if(lineEnd == std::string::npos) {
            return false;
        }
        auto valueBegin = headerPos + std::string("Content-Length:").size();
        auto valueRaw = output.substr(valueBegin, lineEnd - valueBegin);
        size_t length = 0;
        try {
            length = static_cast<size_t>(std::stoul(valueRaw));
        }
        catch(...) {
            return false;
        }

        auto bodyStart = output.find("\r\n\r\n", lineEnd);
        if(bodyStart == std::string::npos) {
            return false;
        }
        bodyStart += 4;
        if(bodyStart + length > output.size()) {
            return false;
        }
        auto body = output.substr(bodyStart, length);
        rapidjson::Document doc;
        doc.Parse(body.c_str());
        if(doc.HasParseError() || !doc.IsObject()) {
            return false;
        }
        messagesOut.push_back(std::move(doc));
        cursor = bodyStart + length;
    }
    return !messagesOut.empty();
}

const rapidjson::Document *findResponseById(const std::vector<rapidjson::Document> &messages, int id) {
    for(const auto &doc : messages) {
        if(!doc.HasMember("id") || !doc["id"].IsInt()) {
            continue;
        }
        if(doc["id"].GetInt() != id) {
            continue;
        }
        return &doc;
    }
    return nullptr;
}

bool ensure(bool cond, const char *message) {
    if(cond) {
        return true;
    }
    std::cerr << "LspCacheRegressionTest failure: " << message << '\n';
    return false;
}

struct DecodedSemanticToken {
    unsigned line = 0;
    unsigned start = 0;
    unsigned length = 0;
    unsigned type = 0;
};

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

std::vector<unsigned> readSemanticData(const rapidjson::Document &doc) {
    std::vector<unsigned> out;
    if(!doc.HasMember("result") || !doc["result"].IsObject()) {
        return out;
    }
    const auto &result = doc["result"];
    if(!result.HasMember("data") || !result["data"].IsArray()) {
        return out;
    }
    for(const auto &item : result["data"].GetArray()) {
        if(item.IsUint()) {
            out.push_back(item.GetUint());
        }
    }
  return out;
}

std::vector<DecodedSemanticToken> decodeSemanticData(const std::vector<unsigned> &encoded) {
    std::vector<DecodedSemanticToken> out;
    if(encoded.size() % 5 != 0) {
        return out;
    }

    unsigned line = 0;
    unsigned start = 0;
    bool first = true;
    for(size_t i = 0;i < encoded.size();i += 5) {
        unsigned deltaLine = encoded[i];
        unsigned deltaStart = encoded[i + 1];
        unsigned length = encoded[i + 2];
        unsigned type = encoded[i + 3];
        if(first) {
            line = deltaLine;
            start = deltaStart;
            first = false;
        }
        else {
            line += deltaLine;
            start = deltaLine == 0 ? start + deltaStart : deltaStart;
        }
        out.push_back({line, start, length, type});
    }
    return out;
}

bool hasSemanticToken(const std::vector<DecodedSemanticToken> &tokens,
                      unsigned line,
                      unsigned start,
                      unsigned length,
                      unsigned type) {
    for(const auto &token : tokens) {
        if(token.line == line && token.start == start && token.length == length && token.type == type) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> readDiagnosticMessages(const rapidjson::Value &items) {
    std::vector<std::string> out;
    if(!items.IsArray()) {
        return out;
    }
    out.reserve(items.Size());
    for(const auto &entry : items.GetArray()) {
        if(!entry.IsObject() || !entry.HasMember("message") || !entry["message"].IsString()) {
            continue;
        }
        out.emplace_back(entry["message"].GetString());
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
    const std::string uri = "file:///tmp/lsp-cache-regression.starb";
    const std::string helperUri = "file:///tmp/Helper/main.starb";
    const std::string timeUri = "file:///tmp/lsp-time-regression.starb";
    const std::string invalidText = "decl value:Int =\nvalue\n";
    const std::string validText =
        "import Helper\n"
        "decl value:Int = 42\n"
        "func add(a:Int, b:Int) Int {\n"
        "  return a + b\n"
        "}\n"
        "add(value, 1)\n"
        "Helper.helperValue()\n";
    const std::string helperText =
        "func helperValue() Int {\n"
        "  return 7\n"
        "}\n";
    const std::string timeText =
        "import Time\n"
        "Time.timezoneUTC()\n";

    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":100,"method":"initialize","params":{"rootUri":")") +
                         rootUri + R"(","workspaceFolders":[{"uri":")" + rootUri + R"(","name":"starbytes-project"}]}})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")") +
                         helperUri + R"(","languageId":"starbytes","version":1,"text":")" + jsonEscape(helperText) + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")") +
                         uri + R"(","languageId":"starbytes","version":1,"text":")" + jsonEscape(invalidText) + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":101,"method":"textDocument/diagnostic","params":{"textDocument":{"uri":")") +
                         uri + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":102,"method":"textDocument/semanticTokens/full","params":{"textDocument":{"uri":")") +
                         uri + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":")") + uri +
                         R"(","version":2},"contentChanges":[{"text":")" + jsonEscape(validText) + R"("}]}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":103,"method":"textDocument/diagnostic","params":{"textDocument":{"uri":")") +
                         uri + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":104,"method":"textDocument/semanticTokens/full","params":{"textDocument":{"uri":")") +
                         uri + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")") +
                         timeUri + R"(","languageId":"starbytes","version":1,"text":")" + jsonEscape(timeText) + R"("}}})");
    appendMessage(input, std::string(R"({"jsonrpc":"2.0","id":105,"method":"textDocument/semanticTokens/full","params":{"textDocument":{"uri":")") +
                         timeUri + R"("}}})");
    appendMessage(input, R"({"jsonrpc":"2.0","id":199,"method":"shutdown","params":null})");
    appendMessage(input, R"({"jsonrpc":"2.0","method":"exit"})");

    starbytes::lsp::ServerOptions opts{input, output};
    starbytes::lsp::Server server(opts);
    server.run();

    std::vector<rapidjson::Document> messages;
    if(!ensure(parseLspOutputMessages(output.str(), messages), "failed to parse LSP output")) {
        return 1;
    }

    auto invalidDiag = findResponseById(messages, 101);
    if(!ensure(invalidDiag != nullptr, "missing invalid diagnostic response")) {
        return 1;
    }
    if(!ensure(invalidDiag->HasMember("result") && (*invalidDiag)["result"].IsObject(), "invalid diagnostic missing result")) {
        return 1;
    }
    const auto &invalidResult = (*invalidDiag)["result"];
    if(!ensure(invalidResult.HasMember("items") && invalidResult["items"].IsArray(), "invalid diagnostic items missing")) {
        return 1;
    }
    auto invalidDiagCount = invalidResult["items"].Size();
    if(!ensure(invalidDiagCount > 0, "invalid source should produce diagnostics")) {
        return 1;
    }

    auto validDiag = findResponseById(messages, 103);
    if(!ensure(validDiag != nullptr, "missing post-edit diagnostic response")) {
        return 1;
    }
    if(!ensure(validDiag->HasMember("result") && (*validDiag)["result"].IsObject(), "post-edit diagnostic missing result")) {
        return 1;
    }
    const auto &validResult = (*validDiag)["result"];
    if(!ensure(validResult.HasMember("items") && validResult["items"].IsArray(), "post-edit diagnostic items missing")) {
        return 1;
    }
    auto preEditMessages = readDiagnosticMessages(invalidResult["items"]);
    auto postEditMessages = readDiagnosticMessages(validResult["items"]);
    if(!ensure(preEditMessages != postEditMessages, "diagnostics should change after document edit")) {
        return 1;
    }

    auto invalidTokens = findResponseById(messages, 102);
    auto validTokens = findResponseById(messages, 104);
    if(!ensure(invalidTokens != nullptr, "missing pre-edit semantic token response")) {
        return 1;
    }
    if(!ensure(validTokens != nullptr, "missing post-edit semantic token response")) {
        return 1;
    }

    auto invalidData = readSemanticData(*invalidTokens);
    auto validData = readSemanticData(*validTokens);
    auto validDecoded = decodeSemanticData(validData);
    if(!ensure(!validData.empty(), "post-edit semantic token data should not be empty")) {
        return 1;
    }
    if(!ensure(invalidData != validData, "semantic tokens should change after document edit")) {
        return 1;
    }
    if(!ensure(hasSemanticToken(validDecoded, 6, 0, 6, 0), "imported module receiver should be namespace-highlighted")) {
        return 1;
    }
    if(!ensure(hasSemanticToken(validDecoded, 6, 7, 11, 2), "imported module member should be function-highlighted")) {
        return 1;
    }

    auto timeTokens = findResponseById(messages, 105);
    if(!ensure(timeTokens != nullptr, "missing stdlib semantic token response")) {
        return 1;
    }

    auto timeData = readSemanticData(*timeTokens);
    auto timeDecoded = decodeSemanticData(timeData);
    if(!ensure(hasSemanticToken(timeDecoded, 1, 0, 4, 0), "stdlib module receiver should be namespace-highlighted")) {
        return 1;
    }
    if(!ensure(hasSemanticToken(timeDecoded, 1, 5, 11, 2), "stdlib module member should be function-highlighted")) {
        return 1;
    }

    return 0;
}
