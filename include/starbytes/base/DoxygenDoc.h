#ifndef STARBYTES_BASE_DOXYGENDOC_H
#define STARBYTES_BASE_DOXYGENDOC_H

#include <string>
#include <utility>
#include <vector>

namespace starbytes {

class DoxygenDoc final {
public:
    struct ParamDoc {
        std::string name;
        std::string description;
    };

    std::string summary;
    std::vector<std::string> details;
    std::vector<ParamDoc> params;
    std::string returns;

    static DoxygenDoc parse(const std::string &rawComment);
    std::string renderMarkdown() const;
};

}

#endif
