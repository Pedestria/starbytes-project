#include "starbytes/Base/Module.h"
#include <filesystem>



STARBYTES_STD_NAMESPACE

Foundation::Unsafe<ModuleSearchResult> ModuleSearch::search(Foundation::StrRef module){
    for(const auto & dir : opts.modules_dirs){
        std::filesystem::directory_iterator it(dir);
        
        for(auto entry : it)
            if(entry.is_regular_file()){
                auto p = entry.path();
                if(p.has_filename() && module == p.filename().string() && p.has_extension() && (p.extension() == ".stbxm" || p.extension() == ".smscst")){
                    std::string ext;
                    if(p.extension() == ".stbxm"){
                        ext = ".smscst";
                    }
                    else if(p.extension() == ".smscst"){
                        ext = ".stbxm";
                    };
                    p.replace_extension(ext);
                    std::filesystem::directory_entry next_entry(p);
                    if(next_entry.exists()){
                        p.replace_extension();
                        return ModuleSearchResult(p.string());
                    }
                    else {
                       return Foundation::Error("Cannot complete resolution of module: " + std::string(module.data()));
                    }
                }
            };
    };

    return Foundation::Error("Could not find module: " + module.toStr());
};

NAMESPACE_END
