#ifndef STARBYTES_BASE_DOXYGENDOC_H
#define STARBYTES_BASE_DOXYGENDOC_H

#include <string>
#include <utility>
#include <vector>

namespace starbytes {

class DoxygenDoc final {
public:
    struct NamedDoc {
        std::string name;
        std::string description;
    };

    std::string summary;
    std::vector<std::string> details;
    std::vector<NamedDoc> params;
    std::vector<NamedDoc> tparams;
    std::vector<NamedDoc> throwsDocs;
    std::vector<std::string> notes;
    std::vector<std::string> warnings;
    std::vector<std::string> sees;
    std::vector<std::string> examples;
    std::string since;
    std::string deprecated;
    std::string returns;

    static DoxygenDoc parse(const std::string &rawComment);
    std::string renderMarkdown() const;
};

}

#endif
