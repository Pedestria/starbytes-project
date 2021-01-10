#include "Macros.h"
#include <vector>
#include <string>

#ifdef _WIN32
#undef min
#undef max
#endif

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/ErrorOr.h>

#ifndef BASE_MODULE_H
#define BASE_MODULE_H
STARBYTES_STD_NAMESPACE
using namespace llvm;

struct ModuleSearchOpts {
    ArrayRef<std::string> modules_dirs;
    ModuleSearchOpts(ArrayRef<std::string> _m_dirs):modules_dirs(_m_dirs){};
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
    ErrorOr<ModuleSearchResult> search(StringRef str);
};

NAMESPACE_END
#endif
