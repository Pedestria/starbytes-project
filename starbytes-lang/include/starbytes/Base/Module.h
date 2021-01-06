#include "Macros.h"
#include <vector>
#include <string>
#include "Unsafe.h"

#ifndef BASE_MODULE_H
#define BASE_MODULE_H
STARBYTES_STD_NAMESPACE

struct ModuleSearchOpts {
    std::vector<std::string> modules_dirs;
};

class ModuleSearchResult {
    std::string name;
    public:
    ModuleSearchResult(std::string n):name(std::move(n)){};
    std::string getSemScopeStoreForMod(){
        return name + ".smscst";
    };
    std::string getCompiledModule(){
        return name + ".stbxm";
    };
};

class ModuleSearch {
    ModuleSearchOpts & opts;
    public:
    // using search_result = ModuleSearchResult;
    ModuleSearch() = delete;
    ModuleSearch(ModuleSearchOpts & _opts):opts(_opts){};
    Foundation::Unsafe<ModuleSearchResult> search(std::string & module);
};

NAMESPACE_END
#endif