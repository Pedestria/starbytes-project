#ifndef STARBYTES_BASE_CODEVIEW_H
#define STARBYTES_BASE_CODEVIEW_H

#include <string>
#include <vector>

#include "starbytes/base/Diagnostic.h"

namespace starbytes {

class CodeView {
    std::string sourceName;
    std::vector<std::string> lines;
public:
    CodeView() = default;
    CodeView(std::string sourceName,std::string sourceText);
    void setSource(std::string sourceName,std::string sourceText);
    bool hasSource() const;
    const std::string &getSourceName() const;
    std::string renderRegion(const Region &region,const std::string &annotation = "",unsigned contextLines = 1) const;
};

}

#endif
