#include "starbytes/interop.h"
#include "starbytes/RT/RTCode.h"
#include "llvm/Support/Path.h"
#include <llvm/ADT/StringRef.h>
#include <new>

#ifdef __ELF__
#include <dlfcn.h>
#endif

namespace starbytes::Runtime {

    void runtime_object_delete(RTObject *obj);
    void runtime_object_ref_inc(RTObject *obj);
    void runtime_object_ref_dec(RTObject *obj);

    StarbytesNativeModule * starbytes_native_mod_load(llvm::StringRef path);

}

using namespace starbytes::Runtime;



extern "C"{

struct __StarbytesObject {
    RTObject *object;
};

StarbytesStr * StarbytesStrCreate(){
    auto data = new __StarbytesObject;
    RTInternalObject *obj = new RTInternalObject();
    obj->type = RTINTOBJ_STR;
    auto params = new RTInternalObject::StringParams();
    obj->data = params;
    data->object = obj;
    return data;
}

void StarbytesStrDestroy(StarbytesStr *str){
    assert(str->object->isInternal);
    RTInternalObject *obj = (RTInternalObject *)str->object;
    assert(obj->type == RTINTOBJ_STR);
    runtime_object_delete(obj);
}

struct __StarbytesFuncArgs {
    RTObject **start;
    RTObject **current;
    unsigned len;
};

StarbytesObject * StarbytesFuncArgsGetArg(StarbytesFuncArgs *args){
    assert(args->current != args->start + args->len && "No more args!");
    args->len += 1;
    args->current += 1;
    auto obj = new __StarbytesObject;
    obj->object = *args->current;
    return obj;
}

void StarbytesObjectDestroy(StarbytesObject *obj){
    runtime_object_delete(obj->object);
}

struct __StarbytesNativeModule {
    std::vector<StarbytesFuncDesc> desc;
    #ifdef __ELF__
        void *dl_handle;
    #endif
};


}



namespace starbytes::Runtime {

typedef StarbytesNativeModule *(*NativeModuleEntryPoint)();

StarbytesNativeModule * starbytes_native_mod_load(llvm::StringRef path){
    StarbytesNativeModule *m;
    #ifdef __ELF__
        auto handle = dlopen(path.data(),RTLD_NOW);
        NativeModuleEntryPoint entry = (NativeModuleEntryPoint)dlsym(handle,STR_WRAP(starbytesModuleMain));
        m = entry();
        m->dl_handle = handle;
    #endif

    #ifdef __MACHO__ 
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
    dlclose(mod->dl_handle);
}

}



