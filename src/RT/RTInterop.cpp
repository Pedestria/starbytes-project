#include "starbytes/interop.h"
#include "starbytes/RT/RTCode.h"
#include "starbytes/Base/ADT.h"
#include <new>

#if defined(__ELF__) | defined(__MACH__)
#define UNIX_DLFCN
#include <dlfcn.h>
#else
#include <windows.h>
#endif

typedef struct __StarbytesNativeModule StarbytesNativeModule;

CString CStringMake(const char *buf){
    auto len = std::strlen(buf);
    auto new_buf = new char[len];
    std::copy(buf,buf + len,new_buf);
    return new_buf;
}

void CStringFree(const char *buf){
    delete [] buf;
}


namespace starbytes::Runtime {


    StarbytesNativeModule * starbytes_native_mod_load(string_ref path);

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
    starbytes::array_ref<uint8_t> raw((uint8_t *)name,strlen(name));
    StarbytesClassType t = StarbytesFuncRefType();
    for(auto b = raw.begin();b != raw.end();b++){
        unsigned A = (unsigned)*b;
        t = (t + ~A) >> 1;
    }
    return t;
}





