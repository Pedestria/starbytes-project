#include <string>
#include <cstdint>
#include <vector>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringMap.h>

#ifndef STARBYTES_RT_RTCODE_H
#define STARBYTES_RT_RTCODE_H
namespace starbytes {
namespace Runtime {


typedef unsigned char RTCode;

#define CODE_MODULE_END 0x00
#define CODE_RTVAR 0x01
#define CODE_RTFUNC 0x02
#define CODE_RTCLASS 0x03
#define CODE_RTOBJCREATE 0x04
#define CODE_RTINTOBJCREATE 0x05
#define CODE_RTEXPR 0x06
#define CODE_RTIVKFUNC 0x06
#define CODE_RTIVKOBJFUNC 0x07
#define CODE_RTBLOCK_BEGIN 0x08
#define CODE_RTBLOCK_END 0x09
#define CODE_RTVAR_REF 0x0A

#define RTINTOBJ_STR 0x1
#define RTINTOBJ_ARRAY 0x2
#define RTINITOBJ_DICTIONARY 0x3


#define RTCODE_STREAM_OBJECT(object) \
std::ostream & operator <<(std::ostream & os,object * obj); \
std::istream & operator >>(std::istream & is,object * obj);

struct RTCodeContext {
    
};

struct RTID {
    size_t len;
    const char *value;
};

RTCODE_STREAM_OBJECT(RTID)

struct RTVar {
    RTID id;
    bool hasInitValue;
};

RTCODE_STREAM_OBJECT(RTVar)


struct RTFuncTemplate {
    std::vector<RTID> argsTemplate;
    
    
};

RTCODE_STREAM_OBJECT(RTFuncTemplate)



struct RTClassTemplate {
    RTID name;
    std::vector<RTFuncTemplate> instMethods;
    
    std::vector<RTFuncTemplate> classMethods;
};

RTCODE_STREAM_OBJECT(RTClassTemplate)

struct RTObject {
    bool isInternal = false;

    llvm::StringMap<RTObject *> props = {};

    RTClassTemplate *classOf = nullptr;
};

RTCODE_STREAM_OBJECT(RTObject);

struct RTInternalObject : public RTObject {
    struct IntegerParams {
        int value;
    };
    struct StringParams {
        std::string str;
    };
    struct ArrayParams {
        std::vector<RTObject *> data;
    };
public:
    unsigned type = 0;
    void *data = nullptr;
    
};

RTCODE_STREAM_OBJECT(RTInternalObject);





};
};

#endif
