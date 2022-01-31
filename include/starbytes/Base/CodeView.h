#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Optional.h>
#include <string>

#ifndef STARBYTES_BASE_CODEVIEW_H
#define STARBYTES_BASE_CODEVIEW_H

namespace starbytes {

    struct SrcLoc {
        unsigned startCol;
        unsigned startLine;
        unsigned endCol;
        unsigned endLine;
    };

    struct CodeViewSrc {
        llvm::StringRef name;
        std::string code;
    };

    void generateCodeView(CodeViewSrc &src,SrcLoc & loc);
}

#endif
