#include "Macros.h"
#include <vector>
#include <string>
#include "Unsafe.h"
#include "ADT.h"

#ifndef BASE_MODULE_H
#define BASE_MODULE_H
STARBYTES_STD_NAMESPACE

struct ModuleSearchOpts {
    Foundation::ArrRef<std::string> modules_dirs;
    ModuleSearchOpts(Foundation::ArrRef<std::string> _m_dirs):modules_dirs(_m_dirs){};
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
    Foundation::Unsafe<ModuleSearchResult> search(Foundation::StrRef str);
};

NAMESPACE_END
#endif
