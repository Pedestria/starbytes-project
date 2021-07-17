#include <starbytes/interop.h>

#include <fstream>

STARBYTES_FUNC(fileOpen){
    StarbytesStr *file = StarbytesFuncArgsGetArg(args);
    StarbytesStr *mode = StarbytesFuncArgsGetArg(args);

    auto fileVal = StarbytesStrGetBuffer(file);
    auto modeVal = StarbytesStrGetBuffer(mode);

    auto in = new std::ifstream(fileVal);

    StarbytesObjectDesc desc;
    desc.name = "Stream";
    desc.customData = in;
    desc.customDataSize = sizeof(std::ifstream);

    return StarbytesObjectCreateFromDesc(&desc);
};


STARBYTES_NATIVE_MOD_MAIN(){
    auto module = StarbytesNativeModuleCreate();
    STARBYTES_FUNC_DESC(fileOpen,2);
    StarbytesNativeModuleAddDesc(module,&fileOpen_desc);
    return module;
};

