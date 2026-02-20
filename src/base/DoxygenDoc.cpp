#include "starbytes/base/DoxygenDoc.h"

#include <cctype>
#include <sstream>

namespace starbytes {

namespace {

std::string trimLeft(std::string value) {
    size_t i = 0;
    while(i < value.size() && std::isspace(static_cast<unsigned char>(value[i])) != 0) {
        ++i;
    }
    return value.substr(i);
}

std::string trimRight(std::string value) {
    while(!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }
    return value;
}

std::string trim(std::string value) {
    return trimRight(trimLeft(std::move(value)));
}

bool startsWith(const std::string &value, const std::string &prefix) {
    return value.rfind(prefix, 0) == 0;
}

void pushDetailLine(DoxygenDoc &doc, const std::string &line) {
    if(line.empty()) {
        if(!doc.details.empty() && !doc.details.back().empty()) {
            doc.details.push_back("");
        }
        return;
    }
    doc.details.push_back(line);
}

}

DoxygenDoc DoxygenDoc::parse(const std::string &rawComment) {
    DoxygenDoc doc;
    std::istringstream stream(rawComment);
    std::string line;
    bool summarySet = false;

    while(std::getline(stream, line)) {
        auto normalized = trim(line);
        if(normalized.empty()) {
            pushDetailLine(doc, "");
            continue;
        }

        auto parseTagWithName = [&](const std::string &tag) -> bool {
            if(!startsWith(normalized, tag)) {
                return false;
            }
            auto rest = trim(normalized.substr(tag.size()));
            if(rest.empty()) {
                doc.params.push_back({"param", ""});
                return true;
            }
            auto sep = rest.find_first_of(" \t");
            if(sep == std::string::npos) {
                doc.params.push_back({rest, ""});
                return true;
            }
            auto name = trim(rest.substr(0, sep));
            auto description = trim(rest.substr(sep + 1));
            doc.params.push_back({name, description});
            return true;
        };

        if(parseTagWithName("@param") || parseTagWithName("\\param")) {
            continue;
        }

        if(startsWith(normalized, "@return") || startsWith(normalized, "@returns") ||
           startsWith(normalized, "\\return") || startsWith(normalized, "\\returns")) {
            auto split = normalized.find_first_of(" \t");
            if(split == std::string::npos) {
                doc.returns.clear();
            }
            else {
                doc.returns = trim(normalized.substr(split + 1));
            }
            continue;
        }

        if(!summarySet) {
            doc.summary = normalized;
            summarySet = true;
        }
        else {
            pushDetailLine(doc, normalized);
        }
    }

    while(!doc.details.empty() && doc.details.back().empty()) {
        doc.details.pop_back();
    }
    return doc;
}

std::string DoxygenDoc::renderMarkdown() const {
    std::string markdown;
    if(!summary.empty()) {
        markdown += summary;
    }

    if(!details.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        for(size_t i = 0; i < details.size(); ++i) {
            if(i > 0) {
                markdown += "\n";
            }
            markdown += details[i];
        }
    }

    if(!params.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Parameters**\n";
        for(const auto &param : params) {
            markdown += "- `" + param.name + "`";
            if(!param.description.empty()) {
                markdown += ": " + param.description;
            }
            markdown += "\n";
        }
        if(!markdown.empty() && markdown.back() == '\n') {
            markdown.pop_back();
        }
    }

    if(!returns.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Returns**\n";
        markdown += returns;
    }

    return markdown;
}

}
