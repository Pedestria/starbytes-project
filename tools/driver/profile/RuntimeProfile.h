#include "starbytes/runtime/RTEngine.h"

#include <cstddef>
#include <iosfwd>
#include <string>

#ifndef STARBYTES_DRIVER_PROFILE_RUNTIMEPROFILE_H
#define STARBYTES_DRIVER_PROFILE_RUNTIMEPROFILE_H

namespace starbytes::driver::profile {

struct RuntimeProfileReport {
    bool enabled = false;
    bool success = false;
    std::string command;
    std::string input;
    std::string moduleName;
    std::string runtimeError;
    Runtime::RuntimeProfileData runtime;
};

void printRuntimeProfileJson(std::ostream &out,const RuntimeProfileReport &report);
void printRuntimeProfileSummary(std::ostream &out,
                                const RuntimeProfileReport &report,
                                size_t maxSubsystems = 5,
                                size_t maxFunctions = 8);

}

#endif
