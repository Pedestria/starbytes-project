#include <llvm/Support/raw_ostream.h>
#include <deque>

#include "CodeView.h"

#ifndef STARBYTES_BASE_DIAGNOSTIC_H
#define STARBYTES_BASE_DIAGNOSTIC_H

namespace starbytes {

struct Diagnostic {
    bool resolved = false;
    virtual bool isError() = 0;
    virtual void format(llvm::raw_ostream & os,CodeViewSrc & src);
    virtual ~Diagnostic() = default;
};

class DiagnosticBufferedLogger {
    std::deque<Diagnostic *> buffer;
    using SELF = DiagnosticBufferedLogger;
    CodeViewSrc *codeViewSrc;
public:
    void setCodeViewSrc(CodeViewSrc &src) { codeViewSrc = &src;};
    bool empty();
    bool hasErrored();
    SELF & operator<<(Diagnostic *diagnostic);
    void logAll();
};

};

#endif
