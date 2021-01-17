#include "starbytes/Parser/Parser.h"
#include "starbytes/Base/Document.h"
#include "starbytes/Base/Module.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ThreadPool.h>


#ifndef CORE_CORE_H
#define CORE_CORE_H

STARBYTES_STD_NAMESPACE

struct DriverOpts {
    ArrayRef<StringRef> modules_to_link;
    const ModuleSearch & module_search;
    StringRef directory;
    StringRef out;
    bool dumpScopeStore;
    DriverOpts(std::string & dir,std::string &_out,const ModuleSearch & m_search,std::vector<StringRef> & mods,bool _output_scope_store):modules_to_link(mods),module_search(m_search),directory(dir),out(_out),dumpScopeStore(_output_scope_store){};
};
class Driver {
    DriverOpts & opts; 
    std::vector<std::string> srcs;
    public:
    Driver() = delete;
    Driver(DriverOpts &_opts):opts(_opts){
        
    };
    void doWork();
};

NAMESPACE_END

#endif
