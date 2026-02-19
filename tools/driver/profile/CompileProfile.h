#include <cstdint>
#include <iosfwd>
#include <string>

#ifndef STARBYTES_DRIVER_PROFILE_COMPILEPROFILE_H
#define STARBYTES_DRIVER_PROFILE_COMPILEPROFILE_H

namespace starbytes::driver::profile {

struct CompileProfileData {
    bool enabled = false;
    uint64_t totalNs = 0;
    uint64_t moduleGraphNs = 0;
    uint64_t moduleFileLoadNs = 0;
    uint64_t importScanNs = 0;
    uint64_t importResolveNs = 0;
    uint64_t parseTotalNs = 0;
    uint64_t lexNs = 0;
    uint64_t syntaxNs = 0;
    uint64_t semanticNs = 0;
    uint64_t consumerNs = 0;
    uint64_t parserSourceBytes = 0;
    uint64_t parserTokenCount = 0;
    uint64_t parserStatementCount = 0;
    uint64_t parserFileCount = 0;
    uint64_t genFinishNs = 0;
    uint64_t runtimeExecNs = 0;
    uint64_t moduleCount = 0;
    uint64_t sourceCount = 0;
    std::string command;
    std::string input;
    std::string moduleName;
};

void printCompileProfile(std::ostream &out,const CompileProfileData &profile,bool success);

}

#endif
