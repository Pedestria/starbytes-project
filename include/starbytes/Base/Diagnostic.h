#include <llvm/Support/raw_ostream.h>
#include <deque>

#ifndef STARBYTES_BASE_DIAGNOSTIC_H
#define STARBYTES_BASE_DIAGNOSTIC_H

namespace starbytes {

struct Diagnostic {
    bool resolved = false;
    virtual bool isError() = 0;
    virtual void format(llvm::raw_ostream & os) = 0;
    virtual ~Diagnostic() = default;
};

class DiagnosticBufferedLogger {
    std::deque<Diagnostic *> buffer;
    using SELF = DiagnosticBufferedLogger;
public:
    bool empty();
    bool hasErrored();
    SELF & operator<<(Diagnostic *diagnostic);
    void logAll();
};

};

#endif
