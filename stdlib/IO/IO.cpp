#include <starbytes/interop.h>

#include <fstream>

STARBYTES_FUNC(fileOpen){
    StarbytesStr file = StarbytesFuncArgsGetArg(args);
    StarbytesStr mode = StarbytesFuncArgsGetArg(args);

    auto fileVal = StarbytesStrGetBuffer(file);
    auto modeVal = StarbytesStrGetBuffer(mode);

    auto in = new std::ifstream(fileVal);

    std::string name = STARBYTES_INSTANCE_FUNC(Stream,open);
    
    StarbytesClassType t = StarbytesMakeClass("Stream");

    return StarbytesObjectNew(t);
};


STARBYTES_NATIVE_MOD_MAIN(){
    auto module = StarbytesNativeModuleCreate();
    StarbytesFuncDesc d;
    d.name = CStringMake("fileOpen");
    d.argCount = 2;
    d.callback = fileOpen;
    StarbytesNativeModuleAddDesc(module,&d);
    return module;
};

