#include "starbytes/base/DoxygenDoc.h"

#include <cctype>
#include <sstream>
#include <utility>

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

bool isTagPrefix(const std::string &value) {
    if(value.empty()) {
        return false;
    }
    return value[0] == '@' || value[0] == '\\';
}

std::pair<std::string,std::string> splitNameAndDescription(const std::string &rest) {
    auto trimmed = trim(rest);
    if(trimmed.empty()) {
        return {"",""};
    }
    auto sep = trimmed.find_first_of(" \t");
    if(sep == std::string::npos) {
        return {trimmed,""};
    }
    return {trim(trimmed.substr(0,sep)), trim(trimmed.substr(sep + 1))};
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

    enum class ContinuationTarget {
        None,
        Param,
        TParam,
        Throws,
        Return,
        Note,
        Warning,
        See,
        Deprecated,
        Since,
        Example
    };
    ContinuationTarget continuation = ContinuationTarget::None;

    auto appendContinuation = [&](ContinuationTarget target,const std::string &text) {
        auto appendText = [&](std::string &dest) {
            if(dest.empty()) {
                dest = text;
            }
            else {
                dest += "\n" + text;
            }
        };

        switch(target) {
            case ContinuationTarget::Param:
                if(!doc.params.empty()) {
                    appendText(doc.params.back().description);
                }
                break;
            case ContinuationTarget::TParam:
                if(!doc.tparams.empty()) {
                    appendText(doc.tparams.back().description);
                }
                break;
            case ContinuationTarget::Throws:
                if(!doc.throwsDocs.empty()) {
                    appendText(doc.throwsDocs.back().description);
                }
                break;
            case ContinuationTarget::Return:
                appendText(doc.returns);
                break;
            case ContinuationTarget::Note:
                if(!doc.notes.empty()) {
                    appendText(doc.notes.back());
                }
                break;
            case ContinuationTarget::Warning:
                if(!doc.warnings.empty()) {
                    appendText(doc.warnings.back());
                }
                break;
            case ContinuationTarget::See:
                if(!doc.sees.empty()) {
                    appendText(doc.sees.back());
                }
                break;
            case ContinuationTarget::Deprecated:
                appendText(doc.deprecated);
                break;
            case ContinuationTarget::Since:
                appendText(doc.since);
                break;
            case ContinuationTarget::Example:
                if(!doc.examples.empty()) {
                    appendText(doc.examples.back());
                }
                break;
            case ContinuationTarget::None:
            default:
                break;
        }
    };

    auto parseNamedTag = [&](const std::string &normalized,
                             const std::string &tag,
                             std::vector<DoxygenDoc::NamedDoc> &out,
                             ContinuationTarget next) -> bool {
        if(!startsWith(normalized,tag)) {
            return false;
        }
        auto parsed = splitNameAndDescription(normalized.substr(tag.size()));
        out.push_back({parsed.first,parsed.second});
        continuation = next;
        return true;
    };

    auto parseSingleTag = [&](const std::string &normalized,
                              const std::string &tag,
                              std::string &dest,
                              ContinuationTarget next) -> bool {
        if(!startsWith(normalized,tag)) {
            return false;
        }
        dest = trim(normalized.substr(tag.size()));
        continuation = next;
        return true;
    };

    auto parseListTag = [&](const std::string &normalized,
                            const std::string &tag,
                            std::vector<std::string> &dest,
                            ContinuationTarget next) -> bool {
        if(!startsWith(normalized,tag)) {
            return false;
        }
        dest.push_back(trim(normalized.substr(tag.size())));
        continuation = next;
        return true;
    };

    while(std::getline(stream, line)) {
        auto normalized = trim(line);
        if(normalized.empty()) {
            continuation = ContinuationTarget::None;
            pushDetailLine(doc, "");
            continue;
        }

        if((continuation != ContinuationTarget::None) && !isTagPrefix(normalized)) {
            appendContinuation(continuation,normalized);
            continue;
        }

        if(parseNamedTag(normalized,"@param",doc.params,ContinuationTarget::Param) ||
           parseNamedTag(normalized,"\\param",doc.params,ContinuationTarget::Param)) {
            continue;
        }

        if(parseNamedTag(normalized,"@tparam",doc.tparams,ContinuationTarget::TParam) ||
           parseNamedTag(normalized,"\\tparam",doc.tparams,ContinuationTarget::TParam)) {
            continue;
        }

        if(parseNamedTag(normalized,"@throws",doc.throwsDocs,ContinuationTarget::Throws) ||
           parseNamedTag(normalized,"\\throws",doc.throwsDocs,ContinuationTarget::Throws) ||
           parseNamedTag(normalized,"@exception",doc.throwsDocs,ContinuationTarget::Throws) ||
           parseNamedTag(normalized,"\\exception",doc.throwsDocs,ContinuationTarget::Throws)) {
            continue;
        }

        if(parseSingleTag(normalized,"@brief",doc.summary,ContinuationTarget::None) ||
           parseSingleTag(normalized,"\\brief",doc.summary,ContinuationTarget::None)) {
            summarySet = !doc.summary.empty();
            continue;
        }

        if(parseSingleTag(normalized,"@return",doc.returns,ContinuationTarget::Return) ||
           parseSingleTag(normalized,"\\return",doc.returns,ContinuationTarget::Return) ||
           parseSingleTag(normalized,"@returns",doc.returns,ContinuationTarget::Return) ||
           parseSingleTag(normalized,"\\returns",doc.returns,ContinuationTarget::Return)) {
            continue;
        }

        if(parseListTag(normalized,"@note",doc.notes,ContinuationTarget::Note) ||
           parseListTag(normalized,"\\note",doc.notes,ContinuationTarget::Note)) {
            continue;
        }

        if(parseListTag(normalized,"@warning",doc.warnings,ContinuationTarget::Warning) ||
           parseListTag(normalized,"\\warning",doc.warnings,ContinuationTarget::Warning)) {
            continue;
        }

        if(parseListTag(normalized,"@see",doc.sees,ContinuationTarget::See) ||
           parseListTag(normalized,"\\see",doc.sees,ContinuationTarget::See)) {
            continue;
        }

        if(parseSingleTag(normalized,"@deprecated",doc.deprecated,ContinuationTarget::Deprecated) ||
           parseSingleTag(normalized,"\\deprecated",doc.deprecated,ContinuationTarget::Deprecated)) {
            continue;
        }

        if(parseSingleTag(normalized,"@since",doc.since,ContinuationTarget::Since) ||
           parseSingleTag(normalized,"\\since",doc.since,ContinuationTarget::Since)) {
            continue;
        }

        if(startsWith(normalized,"@example") || startsWith(normalized,"\\example")) {
            auto split = normalized.find_first_of(" \t");
            auto body = split == std::string::npos ? std::string() : trim(normalized.substr(split + 1));
            doc.examples.push_back(body);
            continuation = ContinuationTarget::Example;
            continue;
        }

        if(startsWith(normalized,"@code") || startsWith(normalized,"\\code")) {
            std::string codeBlock;
            while(std::getline(stream,line)) {
                auto codeLine = trimRight(line);
                auto maybeTag = trim(codeLine);
                if(startsWith(maybeTag,"@endcode") || startsWith(maybeTag,"\\endcode")) {
                    break;
                }
                if(!codeBlock.empty()) {
                    codeBlock += "\n";
                }
                codeBlock += codeLine;
            }
            if(!codeBlock.empty()) {
                doc.examples.push_back("```text\n" + codeBlock + "\n```");
            }
            continuation = ContinuationTarget::None;
            continue;
        }

        if(!summarySet) {
            doc.summary = normalized;
            summarySet = true;
        }
        else {
            continuation = ContinuationTarget::None;
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

    if(!tparams.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Type Parameters**\n";
        for(const auto &param : tparams) {
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

    if(!throwsDocs.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Throws**\n";
        for(const auto &throwDoc : throwsDocs) {
            markdown += "- `" + throwDoc.name + "`";
            if(!throwDoc.description.empty()) {
                markdown += ": " + throwDoc.description;
            }
            markdown += "\n";
        }
        if(!markdown.empty() && markdown.back() == '\n') {
            markdown.pop_back();
        }
    }

    if(!notes.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Notes**\n";
        for(const auto &item : notes) {
            markdown += "- " + item + "\n";
        }
        if(!markdown.empty() && markdown.back() == '\n') {
            markdown.pop_back();
        }
    }

    if(!warnings.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Warnings**\n";
        for(const auto &item : warnings) {
            markdown += "- " + item + "\n";
        }
        if(!markdown.empty() && markdown.back() == '\n') {
            markdown.pop_back();
        }
    }

    if(!sees.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**See Also**\n";
        for(const auto &item : sees) {
            markdown += "- " + item + "\n";
        }
        if(!markdown.empty() && markdown.back() == '\n') {
            markdown.pop_back();
        }
    }

    if(!since.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Since**\n";
        markdown += since;
    }

    if(!deprecated.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Deprecated**\n";
        markdown += deprecated;
    }

    if(!examples.empty()) {
        if(!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += "**Examples**\n";
        for(size_t i = 0; i < examples.size(); ++i) {
            if(i > 0) {
                markdown += "\n";
            }
            if(startsWith(examples[i],"```")) {
                markdown += examples[i];
            }
            else {
                markdown += "- " + examples[i];
            }
            if(i + 1 < examples.size()) {
                markdown += "\n";
            }
        }
    }

    return markdown;
}

}
