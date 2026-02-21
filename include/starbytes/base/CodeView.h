#ifndef STARBYTES_BASE_CODEVIEW_H
#define STARBYTES_BASE_CODEVIEW_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "starbytes/base/Diagnostic.h"

namespace starbytes {

struct CodeViewSourceIndex {
    std::string sourceName;
    std::string sourceText;
    std::vector<size_t> lineStarts;
};

class CodeView {
    std::shared_ptr<const CodeViewSourceIndex> indexedSource;
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
