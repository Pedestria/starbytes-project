#include "ASTNodes.def"
#include <string>
#include <cstdint>
#include <vector>
#include <istream>
#include <map>

#include <starbytes/interop.h>

#ifndef STARBYTES_RT_RTCODE_H
#define STARBYTES_RT_RTCODE_H

namespace starbytes {
namespace Runtime {


typedef unsigned char RTCode;

#define CODE_MODULE_END 0x00
#define CODE_RTVAR 0x01
#define CODE_RTFUNC 0x02
#define CODE_RTCLASS_DEF 0x03
#define CODE_RTOBJCREATE 0x04
#define CODE_RTINTOBJCREATE 0x05
#define CODE_RTEXPR 0x06
#define CODE_RTIVKFUNC 0x06
#define CODE_RTRETURN 0x07
#define CODE_RTFUNCBLOCK_BEGIN 0x08
#define CODE_RTFUNCBLOCK_END 0x09
#define CODE_RTBLOCK_BEGIN 0x08
#define CODE_RTBLOCK_END 0x09
#define CODE_RTVAR_REF 0x0A
#define CODE_RTOBJVAR_REF 0x0B
#define CODE_CONDITIONAL 0x0C
#define CODE_CONDITIONAL_END 0x0D
#define CODE_RTFUNC_REF 0x0E
#define CODE_UNARY_OPERATOR 0x0F
#define CODE_BINARY_OPERATOR 0x10

/// Condtional Type
#define COND_TYPE_IF 0x0 // if()
#define COND_TYPE_ELSE 0x1 // else
#define COND_TYPE_LOOPIF 0x2 // while()

/// Runtime Internal Object Type
#define RTINTOBJ_STR 0x1 // "Hello World"
#define RTINTOBJ_ARRAY 0x2 // ["a","b","c"]
#define RTINTOBJ_DICTIONARY 0x3 // {"a":"b","c":"d"}
#define RTINTOBJ_BOOL 0x4 // true
#define RTINTOBJ_NUM 0x5 // 1

/// Unary Operator Type

#define UNARY_OP_PLUS 0x0 // ++
#define UNARY_OP_MINUS 0x1 // --
#define UNARY_OP_NOT 0x2 // !

/// Binary Operator Type

#define BINARY_OP_PLUS 0x00 // +
#define BINARY_OP_MINUS 0x01 // -
#define BINARY_OP_PLUS_EQUAL 0x02 // +=
#define BINARY_MINUS_EQUAL 0x03 // -=
#define BINARY_EQUAL_EQUAL 0x04 // ==
#define BINARY_NOT_EQUAL 0x05 // !=

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
    RTID name;
    unsigned invocations = 0;
    std::vector<RTID> argsTemplate;
    /// Position of CODE_RTBLOCK_BEGIN
    std::istream::pos_type block_start_pos;
};

RTCODE_STREAM_OBJECT(RTFuncTemplate)


struct RTClass {
    RTID name;
    std::map<RTVar,StarbytesObject> properties;
    std::vector<RTFuncTemplate> methods;
};


RTCODE_STREAM_OBJECT(RTClass)

/// Use C Object in RTObject.c
RTCODE_STREAM_OBJECT(StarbytesObject);




}
}

#endif
