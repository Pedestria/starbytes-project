#include "starbytes/Base/Module.h"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>



STARBYTES_STD_NAMESPACE

const std::string starbytes_compiled_module_ext = ".stbxm";
const std::string semantica_scope_store_ext = ".smscst";

llvm::ErrorOr<ModuleSearchResult> ModuleSearch::search(llvm::StringRef module){
    for(auto dir : opts.modules_dirs){
        llvm::Twine twine_str(dir);
        std::error_code error;
        llvm::sys::fs::directory_iterator it(twine_str,error);
        llvm::sys::fs::directory_iterator end_it(it);
        while(it != end_it){
            if(error){
                return error;
                break;
            };
            auto entry = *it;
            if(entry.type() == llvm::sys::fs::file_type::regular_file){
                llvm::Twine twine_path = entry.path();
                if(llvm::sys::path::has_extension(twine_path)){
                    llvm::StringRef path = twine_path.getSingleStringRef();
                    llvm::StringRef ext = llvm::sys::path::extension(path);
                    llvm::StringRef filename = llvm::sys::path::filename(path);
                    if(filename == module && (ext == starbytes_compiled_module_ext || ext == semantica_scope_store_ext)){
                        return path;
                    };
                        

                };
            }
            
            it.increment(error);
        };
        
    };

    return std::errc::not_a_directory;
};

NAMESPACE_END
