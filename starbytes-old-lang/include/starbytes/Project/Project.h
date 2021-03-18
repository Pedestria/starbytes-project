
#include <sstream>
#include <string>
#include <vector>
#include "starbytes/Base/Base.h"

#ifndef PROJECT_PROJECT_H
#define PROJECT_PROJECT_H

STARBYTES_STD_NAMESPACE

    class DependencyTree;

    struct StarbytesCompiledModule {
        //BYTE CODE CALL STACK!
        std::string name;
        bool to_dest;
        std::string dest;
    };

    struct ProjectCompileResult{
        std::vector<StarbytesCompiledModule> & compiled_programs;
        DependencyTree *& original_tree;
        ProjectCompileResult(std::vector<StarbytesCompiledModule> & progs, DependencyTree *& tree):compiled_programs(progs),original_tree(tree){};
    };

    ProjectCompileResult buildTreeFromConfig(std::string & module_config_file);


NAMESPACE_END

#endif