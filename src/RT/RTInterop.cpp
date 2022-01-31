#include "starbytes/interop.h"
#include "starbytes/RT/RTCode.h"
#include "llvm/Support/Path.h"
#include <llvm/ADT/StringRef.h>
#include <unicode/ustring.h>
#include <llvm/ADT/ArrayRef.h>
#include <new>

#if defined(__ELF__) | defined(__MACH__)
#define UNIX_DLFCN
#include <dlfcn.h>
#else
#include <windows.h>
#endif

typedef struct __StarbytesNativeModule StarbytesNativeModule;


namespace starbytes::Runtime {


    StarbytesNativeModule * starbytes_native_mod_load(llvm::StringRef path);

}

using namespace starbytes::Runtime;



struct __StarbytesNativeModule {
    std::vector<StarbytesFuncDesc> desc;
#if defined(UNIX_DLFCN)
    void *dl_handle;
#else
    HMODULE mod;
#endif
};

StarbytesNativeModule *StarbytesNativeModuleCreate(){
    return new __StarbytesNativeModule;
};

void StarbytesNativeModuleAddDesc(StarbytesNativeModule *module,StarbytesFuncDesc *desc){
    module->desc.push_back(*desc);
    
};


//}



namespace starbytes::Runtime {

typedef StarbytesNativeModule *(*NativeModuleEntryPoint)();

StarbytesNativeModule * starbytes_native_mod_load(llvm::StringRef path){
    StarbytesNativeModule *m;
    #ifdef UNIX_DLFCN
        auto handle = dlopen(path.data(),RTLD_NOW);
        NativeModuleEntryPoint entry = (NativeModuleEntryPoint)dlsym(handle,STR_WRAP(starbytesModuleMain));
        m = entry();
        m->dl_handle = handle;
    #else
        auto handle = LoadLibraryA(path.data());
        auto entry = (NativeModuleEntryPoint) GetProcAddress(handle,STR_WRAP(starbytesModuleMain));
        m = entry();
        m->mod = handle;
    #endif
    return m;
}

StarbytesFuncCallback starbytes_native_mod_load_function(StarbytesNativeModule * mod,llvm::StringRef name){
    for(auto & d : mod->desc){
        if(d.name == name){
            return d.callback;
        };
    };
    return nullptr;
}

void starbytes_native_mod_close(StarbytesNativeModule * mod){
#ifdef UNIX_DLFCN
    dlclose(mod->dl_handle);
#else
    FreeLibrary(mod->mod);
#endif
}

}


StarbytesClassType StarbytesMakeClass(const char *name){
    llvm::ArrayRef<uint8_t> raw((uint8_t *)name,strlen(name));
    StarbytesClassType t = StarbytesFuncRefType();
    for(auto b = raw.begin();b != raw.end();b++){
        unsigned A = (unsigned)*b;
        t = (t + ~A) >> 1;
    }
    return t;
}



