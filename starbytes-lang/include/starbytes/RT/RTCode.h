#include <string>
#include <cstdint>
#include <vector>

#ifndef STARBYTES_RTCODE_H
#define STARBYTES_RTCODE_H
namespace starbytes {
namespace Runtime {


typedef unsigned char RTCode;

#define CODE_MODULE_END 0x00
#define CODE_RTVAR 0x01
#define CODE_RTFUNC 0x02
#define CODE_RTCLASS 0x03
#define CODE_RTOBJCREATE 0x04
#define CODE_RTINTOBJCREATE 0x05
#define CODE_RTIVKFUNC 0x06
#define CODE_RTIVKOBJFUNC 0x07


#define RTCODE_STREAM_OBJECT(object) \
std::ostream & operator <<(std::ostream & os,object & obj); \
std::istream & operator >>(std::istream & is,object & obj);

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

struct RTInternalObject {
    typedef enum : int {
        Integer,
        Float,
        String,
        Boolean
    } Type;
    Type type;
    void *data;
};


struct RTObject {
    RTClassTemplate *classOf;
};




};
};

#endif
