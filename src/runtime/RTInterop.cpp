#include "starbytes/interop.h"
#include "starbytes/compiler/RTCode.h"
#include "starbytes/base/ADT.h"
#include <new>
#include <algorithm>
#include <cstring>
#include <string>

#if defined(__ELF__) | defined(__MACH__)
#define UNIX_DLFCN
#include <dlfcn.h>
#else
#include <windows.h>
#endif

typedef struct __StarbytesNativeModule StarbytesNativeModule;

CString CStringMake(const char *buf){
    auto len = std::strlen(buf);
    auto new_buf = new char[len + 1];
    std::copy(buf,buf + len,new_buf);
    new_buf[len] = '\0';
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

StarbytesNativeModule * starbytes_native_mod_load(string_ref path){
    StarbytesNativeModule *m = nullptr;
    auto pathStr = std::string(path.getBuffer(),path.size());
    #ifdef UNIX_DLFCN
        auto handle = dlopen(pathStr.c_str(),RTLD_NOW);
        if(!handle){
            return nullptr;
        }
        auto entry = (NativeModuleEntryPoint)dlsym(handle,STR_WRAP(starbytesModuleMain));
        if(!entry){
            dlclose(handle);
            return nullptr;
        }
        m = entry();
        if(!m){
            dlclose(handle);
            return nullptr;
        }
        m->dl_handle = handle;
    #else
        auto handle = LoadLibraryA(pathStr.c_str());
        if(!handle){
            return nullptr;
        }
        auto entry = (NativeModuleEntryPoint) GetProcAddress(handle,STR_WRAP(starbytesModuleMain));
        if(!entry){
            FreeLibrary(handle);
            return nullptr;
        }
        m = entry();
        if(!m){
            FreeLibrary(handle);
            return nullptr;
        }
        m->mod = handle;
    #endif
    return m;
}

StarbytesFuncCallback starbytes_native_mod_load_function(StarbytesNativeModule * mod,string_ref name){
    if(!mod){
        return nullptr;
    }
    for(auto & d : mod->desc){
        if(name == d.name){
            return d.callback;
        };
    };
    return nullptr;
}

void starbytes_native_mod_close(StarbytesNativeModule * mod){
if(!mod){
    return;
}
#ifdef UNIX_DLFCN
    dlclose(mod->dl_handle);
#else
    FreeLibrary(mod->mod);
#endif
    delete mod;
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


