#pragma once
#include <sstream>
#include <string>
#include <vector>

namespace Starbytes {

    struct StarbytesCompiledModule {
        std::ostringstream out;
        std::string dest;
    };


class StarbytesModule{
    public:
        StarbytesModule();

};
std::vector<StarbytesModule *> * constructModule(std::string & module_config_file);
std::vector<StarbytesCompiledModule *> * constructAndCompileModulesFromConfig(std::string & module_config_file);


}