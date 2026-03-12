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
#define CODE_RTNEWOBJ 0x11
#define CODE_RTMEMBER_GET 0x12
#define CODE_RTMEMBER_SET 0x13
#define CODE_RTMEMBER_IVK 0x14
#define CODE_RTSECURE_DECL 0x15
#define CODE_RTREGEX_LITERAL 0x16
#define CODE_RTVAR_SET 0x17
#define CODE_RTINDEX_GET 0x18
#define CODE_RTINDEX_SET 0x19
#define CODE_RTDICT_LITERAL 0x1A
#define CODE_RTTYPECHECK 0x1B
#define CODE_RTTERNARY 0x1C
#define CODE_RTCAST 0x1D
#define CODE_RTARRAY_LITERAL 0x1E
#define CODE_RTLOCAL_REF 0x1F
#define CODE_RTLOCAL_SET 0x20
#define CODE_RTLOCAL_DECL 0x21
#define CODE_RTSECURE_LOCAL_DECL 0x22
#define CODE_RTTYPED_LOCAL_REF 0x23
#define CODE_RTTYPED_BINARY 0x24
#define CODE_RTTYPED_NEGATE 0x25
#define CODE_RTTYPED_COMPARE 0x26
#define CODE_RTTYPED_INDEX_GET 0x27
#define CODE_RTTYPED_INDEX_SET 0x28
#define CODE_RTTYPED_INTRINSIC 0x29

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

#define UNARY_OP_PLUS 0x0 // +
#define UNARY_OP_MINUS 0x1 // -
#define UNARY_OP_NOT 0x2 // !
#define UNARY_OP_AWAIT 0x3 // await
#define UNARY_OP_BITWISE_NOT 0x4 // ~

/// Binary Operator Type

#define BINARY_OP_PLUS 0x00 // +
#define BINARY_OP_MINUS 0x01 // -
#define BINARY_OP_MUL 0x02 // *
#define BINARY_OP_DIV 0x03 // /
#define BINARY_OP_MOD 0x04 // %
#define BINARY_EQUAL_EQUAL 0x05 // ==
#define BINARY_NOT_EQUAL 0x06 // !=
#define BINARY_LESS 0x07 // <
#define BINARY_LESS_EQUAL 0x08 // <=
#define BINARY_GREATER 0x09 // >
#define BINARY_GREATER_EQUAL 0x0A // >=
#define BINARY_LOGIC_AND 0x0B // &&
#define BINARY_LOGIC_OR 0x0C // ||
#define BINARY_BITWISE_AND 0x0D // &
#define BINARY_BITWISE_OR 0x0E // |
#define BINARY_BITWISE_XOR 0x0F // ^
#define BINARY_SHIFT_LEFT 0x10 // <<
#define BINARY_SHIFT_RIGHT 0x11 // >>

typedef uint8_t RTTypedNumericKind;
#define RTTYPED_NUM_OBJECT 0x00
#define RTTYPED_NUM_INT 0x01
#define RTTYPED_NUM_LONG 0x02
#define RTTYPED_NUM_FLOAT 0x03
#define RTTYPED_NUM_DOUBLE 0x04

typedef uint8_t RTTypedBinaryOp;
#define RTTYPED_BINARY_ADD 0x00
#define RTTYPED_BINARY_SUB 0x01
#define RTTYPED_BINARY_MUL 0x02
#define RTTYPED_BINARY_DIV 0x03
#define RTTYPED_BINARY_MOD 0x04

typedef uint8_t RTTypedCompareOp;
#define RTTYPED_COMPARE_EQ 0x00
#define RTTYPED_COMPARE_NE 0x01
#define RTTYPED_COMPARE_LT 0x02
#define RTTYPED_COMPARE_LE 0x03
#define RTTYPED_COMPARE_GT 0x04
#define RTTYPED_COMPARE_GE 0x05

typedef uint8_t RTTypedIntrinsicOp;
#define RTTYPED_INTRINSIC_SQRT 0x00

typedef uint8_t RTQuickenedExprKind;
#define RTQUICKEN_EXPR_NONE 0x00
#define RTQUICKEN_EXPR_LOCAL_LOCAL_BINARY 0x01
#define RTQUICKEN_EXPR_LOCAL_LOCAL_COMPARE 0x02
#define RTQUICKEN_EXPR_LOCAL_INTRINSIC 0x03
#define RTQUICKEN_EXPR_LOCAL_LOCAL_INDEX_GET 0x04
#define RTQUICKEN_EXPR_LOCAL_LOCAL_LOCAL_INDEX_SET 0x05

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

typedef uint8_t RTAttrValueType;
#define RTATTR_VALUE_STRING 0x01
#define RTATTR_VALUE_NUMBER 0x02
#define RTATTR_VALUE_BOOL 0x03
#define RTATTR_VALUE_IDENTIFIER 0x04

struct RTAttributeArg {
    bool hasName = false;
    RTID name;
    RTID value;
    RTAttrValueType valueType = RTATTR_VALUE_IDENTIFIER;
};

RTCODE_STREAM_OBJECT(RTAttributeArg)

struct RTAttribute {
    RTID name;
    std::vector<RTAttributeArg> args;
};

RTCODE_STREAM_OBJECT(RTAttribute)

struct RTVar {
    RTID id;
    bool hasInitValue;
    std::vector<RTAttribute> attributes;
};

RTCODE_STREAM_OBJECT(RTVar)

struct RTQuickenedExpr {
    RTQuickenedExprKind kind = RTQUICKEN_EXPR_NONE;
    RTTypedNumericKind numericKind = RTTYPED_NUM_OBJECT;
    uint8_t op = 0;
    uint32_t slotA = 0;
    uint32_t slotB = 0;
    uint32_t slotC = 0;
    std::streamoff byteSize = 0;
    uint64_t executions = 0;
    uint64_t fallbackCount = 0;
};

struct RTFuncTemplate {
    RTID name;
    unsigned invocations = 0;
    std::vector<RTID> argsTemplate;
    std::vector<RTID> localSlotNames;
    std::vector<RTTypedNumericKind> slotKinds;
    std::vector<RTAttribute> attributes;
    bool isLazy = false;
    size_t blockByteSize = 0;
    /// Position of CODE_RTBLOCK_BEGIN
    std::istream::pos_type block_start_pos;
    std::map<std::streamoff, RTQuickenedExpr> quickenedExprs;
};

RTCODE_STREAM_OBJECT(RTFuncTemplate)


struct RTClass {
    RTID name;
    std::vector<RTAttribute> attributes;
    bool hasSuperClass = false;
    RTID superClassName;
    std::vector<RTVar> fields;
    std::vector<RTFuncTemplate> methods;
    std::vector<RTFuncTemplate> constructors;
    bool hasFieldInitFunc = false;
    RTID fieldInitFuncName;
};


RTCODE_STREAM_OBJECT(RTClass)

/// Use C Object in RTObject.c
RTCODE_STREAM_OBJECT(StarbytesObject);




}
}

#endif
