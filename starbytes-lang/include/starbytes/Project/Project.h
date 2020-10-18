
#include <sstream>
#include <string>
#include <vector>
#include "starbytes/Base/Base.h"
#include "starbytes/ByteCode/BCDef.h"

#ifndef PROJECT_PROJECT_H
#define PROJECT_PROJECT_H

STARBYTES_STD_NAMESPACE

    class DependencyTree;

    struct StarbytesCompiledModule {
        //BYTE CODE CALL STACK!
        ByteCode::BCProgram *program;
        bool to_dest;
        std::string dest;
    };

    DependencyTree * buildTreeFromConfig(std::string & module_config_file);


NAMESPACE_END

#endif