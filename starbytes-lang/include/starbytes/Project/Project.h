
#include <sstream>
#include <string>
#include <vector>
#include "starbytes/Base/Base.h"

#ifndef PROJECT_PROJECT_H
#define PROJECT_PROJECT_H

STARBYTES_STD_NAMESPACE

    struct StarbytesCompiledModule {
        //BYTE CODE CALL STACK!
        std::string dest;
    };

    // std::vector<StarbytesModule *> * constructModule(std::string & module_config_file);
    std::vector<StarbytesCompiledModule *> * constructAndCompileModulesFromConfig(std::string & module_config_file);


NAMESPACE_END

#endif