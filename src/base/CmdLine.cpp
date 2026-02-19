#include "starbytes/base/CmdLine.h"

namespace {

std::string normalizeOptionName(const std::string &value) {
    if(value.rfind("--",0) == 0) {
        return value.substr(2);
    }
    if(value.rfind("-",0) == 0) {
        return value.substr(1);
    }
    return value;
}

bool isOptionToken(const std::string &value) {
    return value.size() > 1 && value[0] == '-';
}

struct ParsedOptionToken {
    bool isOption = false;
    std::string key;
    bool hasInlineValue = false;
    std::string inlineValue;
};

ParsedOptionToken parseOptionToken(const std::string &token) {
    ParsedOptionToken parsed;
    if(token == "--" || !isOptionToken(token)) {
        return parsed;
    }

    parsed.isOption = true;
    auto keyStart = (token.rfind("--",0) == 0) ? 2u : 1u;
    if(keyStart >= token.size()) {
        return parsed;
    }

    auto body = token.substr(keyStart);
    auto equalPos = body.find('=');
    if(equalPos == std::string::npos) {
        parsed.key = body;
        return parsed;
    }

    parsed.key = body.substr(0,equalPos);
    parsed.hasInlineValue = true;
    parsed.inlineValue = body.substr(equalPos + 1);
    return parsed;
}

}

namespace starbytes::cl {

bool ParseResult::hasFlag(const std::string &name) const {
    return presentFlags.find(normalizeOptionName(name)) != presentFlags.end();
}

bool ParseResult::hasOptionValue(const std::string &name) const {
    auto found = optionValues.find(normalizeOptionName(name));
    return found != optionValues.end() && !found->second.empty();
}

std::optional<std::string> ParseResult::firstValue(const std::string &name) const {
    auto found = optionValues.find(normalizeOptionName(name));
    if(found == optionValues.end() || found->second.empty()) {
        return std::nullopt;
    }
    return found->second.front();
}

const std::vector<std::string> &ParseResult::values(const std::string &name) const {
    static const std::vector<std::string> kEmpty;
    auto found = optionValues.find(normalizeOptionName(name));
    if(found == optionValues.end()) {
        return kEmpty;
    }
    return found->second;
}

void Parser::addOption(const std::string &name,OptionKind kind,std::initializer_list<std::string> aliases) {
    auto canonical = normalizeOptionName(name);
    if(canonical.empty()) {
        return;
    }

    OptionRegistration registration;
    registration.canonicalName = canonical;
    registration.kind = kind;

    optionByAlias[canonical] = registration;
    for(const auto &alias : aliases) {
        auto normalizedAlias = normalizeOptionName(alias);
        if(normalizedAlias.empty()) {
            continue;
        }
        optionByAlias[normalizedAlias] = registration;
    }
}

void Parser::addCommand(const std::string &name) {
    if(name.empty()) {
        return;
    }
    commands.insert(name);
}

void Parser::addFlagOption(const std::string &name,std::initializer_list<std::string> aliases) {
    addOption(name,OptionKind::Flag,aliases);
}

void Parser::addValueOption(const std::string &name,std::initializer_list<std::string> aliases) {
    addOption(name,OptionKind::Value,aliases);
}

void Parser::addMultiValueOption(const std::string &name,std::initializer_list<std::string> aliases) {
    addOption(name,OptionKind::MultiValue,aliases);
}

ParseResult Parser::parse(int argc,const char *argv[]) const {
    ParseResult out;
    if(argc <= 0 || argv == nullptr) {
        out.ok = false;
        out.exitCode = 1;
        out.error = "Invalid argv input.";
        return out;
    }

    bool commandResolved = commands.empty();

    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i] ? argv[i] : "";

        if(arg == "--") {
            for(++i; i < argc; ++i) {
                out.passthroughArgs.push_back(argv[i] ? argv[i] : "");
            }
            out.ok = true;
            out.exitCode = 0;
            return out;
        }

        if(!commandResolved && out.positionals.empty() && !arg.empty() && arg[0] != '-') {
            if(commands.find(arg) != commands.end()) {
                out.command = arg;
                commandResolved = true;
                continue;
            }
            commandResolved = true;
        }

        auto parsedOption = parseOptionToken(arg);
        if(parsedOption.isOption) {
            if(parsedOption.key.empty()) {
                out.ok = false;
                out.exitCode = 1;
                out.error = "Unknown option: " + arg;
                return out;
            }

            auto registrationIt = optionByAlias.find(parsedOption.key);
            if(registrationIt == optionByAlias.end()) {
                out.ok = false;
                out.exitCode = 1;
                out.error = "Unknown option: " + arg;
                return out;
            }

            const auto &registration = registrationIt->second;
            if(registration.kind == OptionKind::Flag) {
                if(parsedOption.hasInlineValue) {
                    out.ok = false;
                    out.exitCode = 1;
                    out.error = "Option does not accept a value: " + arg;
                    return out;
                }
                out.presentFlags.insert(registration.canonicalName);
                continue;
            }

            std::string value;
            if(parsedOption.hasInlineValue) {
                value = parsedOption.inlineValue;
            }
            else {
                if(i + 1 >= argc) {
                    out.ok = false;
                    out.exitCode = 1;
                    out.error = "Missing value for option: " + arg;
                    return out;
                }
                ++i;
                value = argv[i] ? argv[i] : "";
            }

            out.optionValues[registration.canonicalName].push_back(std::move(value));
            continue;
        }

        out.positionals.push_back(std::move(arg));
    }

    out.ok = true;
    out.exitCode = 0;
    return out;
}

}
