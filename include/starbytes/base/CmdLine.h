#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef STARBYTES_BASE_CMDLINE_H
#define STARBYTES_BASE_CMDLINE_H

namespace starbytes::cl {

enum class OptionKind {
    Flag,
    Value,
    MultiValue
};

struct ParseResult {
    bool ok = false;
    int exitCode = 1;
    std::string error;
    std::string command;
    std::vector<std::string> positionals;
    std::vector<std::string> passthroughArgs;
    std::unordered_set<std::string> presentFlags;
    std::unordered_map<std::string,std::vector<std::string>> optionValues;

    bool hasFlag(const std::string &name) const;
    bool hasOptionValue(const std::string &name) const;
    std::optional<std::string> firstValue(const std::string &name) const;
    const std::vector<std::string> &values(const std::string &name) const;
};

class Parser {
public:
    void addCommand(const std::string &name);
    void addFlagOption(const std::string &name,std::initializer_list<std::string> aliases = {});
    void addValueOption(const std::string &name,std::initializer_list<std::string> aliases = {});
    void addMultiValueOption(const std::string &name,std::initializer_list<std::string> aliases = {});

    ParseResult parse(int argc,const char *argv[]) const;

private:
    struct OptionRegistration {
        std::string canonicalName;
        OptionKind kind = OptionKind::Flag;
    };

    std::unordered_set<std::string> commands;
    std::unordered_map<std::string,OptionRegistration> optionByAlias;

    void addOption(const std::string &name,OptionKind kind,std::initializer_list<std::string> aliases);
};

}

#endif
