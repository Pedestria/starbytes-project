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
#define CODE_RTCALL_DIRECT 0x2A
#define CODE_RTCALL_BUILTIN_MEMBER 0x2B
#define CODE_RTMEMBER_GET_FIELD_SLOT 0x2C
#define CODE_RTMEMBER_SET_FIELD_SLOT 0x2D

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

typedef uint8_t RTBuiltinMemberId;
#define RTBUILTIN_MEMBER_INVALID 0x00
#define RTBUILTIN_MEMBER_STRING_IS_EMPTY 0x01
#define RTBUILTIN_MEMBER_STRING_AT 0x02
#define RTBUILTIN_MEMBER_STRING_SLICE 0x03
#define RTBUILTIN_MEMBER_STRING_CONTAINS 0x04
#define RTBUILTIN_MEMBER_STRING_STARTS_WITH 0x05
#define RTBUILTIN_MEMBER_STRING_ENDS_WITH 0x06
#define RTBUILTIN_MEMBER_STRING_INDEX_OF 0x07
#define RTBUILTIN_MEMBER_STRING_LAST_INDEX_OF 0x08
#define RTBUILTIN_MEMBER_STRING_LOWER 0x09
#define RTBUILTIN_MEMBER_STRING_UPPER 0x0A
#define RTBUILTIN_MEMBER_STRING_TRIM 0x0B
#define RTBUILTIN_MEMBER_STRING_REPLACE 0x0C
#define RTBUILTIN_MEMBER_STRING_SPLIT 0x0D
#define RTBUILTIN_MEMBER_STRING_REPEAT 0x0E
#define RTBUILTIN_MEMBER_REGEX_MATCH 0x0F
#define RTBUILTIN_MEMBER_REGEX_FIND_ALL 0x10
#define RTBUILTIN_MEMBER_REGEX_REPLACE 0x11
#define RTBUILTIN_MEMBER_ARRAY_IS_EMPTY 0x12
#define RTBUILTIN_MEMBER_ARRAY_PUSH 0x13
#define RTBUILTIN_MEMBER_ARRAY_POP 0x14
#define RTBUILTIN_MEMBER_ARRAY_AT 0x15
#define RTBUILTIN_MEMBER_ARRAY_SET 0x16
#define RTBUILTIN_MEMBER_ARRAY_INSERT 0x17
#define RTBUILTIN_MEMBER_ARRAY_REMOVE_AT 0x18
#define RTBUILTIN_MEMBER_ARRAY_CLEAR 0x19
#define RTBUILTIN_MEMBER_ARRAY_CONTAINS 0x1A
#define RTBUILTIN_MEMBER_ARRAY_INDEX_OF 0x1B
#define RTBUILTIN_MEMBER_ARRAY_SLICE 0x1C
#define RTBUILTIN_MEMBER_ARRAY_JOIN 0x1D
#define RTBUILTIN_MEMBER_ARRAY_COPY 0x1E
#define RTBUILTIN_MEMBER_ARRAY_REVERSE 0x1F
#define RTBUILTIN_MEMBER_DICT_IS_EMPTY 0x20
#define RTBUILTIN_MEMBER_DICT_HAS 0x21
#define RTBUILTIN_MEMBER_DICT_GET 0x22
#define RTBUILTIN_MEMBER_DICT_REMOVE 0x23
#define RTBUILTIN_MEMBER_DICT_SET 0x24
#define RTBUILTIN_MEMBER_DICT_KEYS 0x25
#define RTBUILTIN_MEMBER_DICT_VALUES 0x26
#define RTBUILTIN_MEMBER_DICT_CLEAR 0x27
#define RTBUILTIN_MEMBER_DICT_COPY 0x28

typedef uint8_t RTV2Opcode;
#define RTV2_OP_NOP 0x00
#define RTV2_OP_MOVE 0x01
#define RTV2_OP_LOAD_I64_CONST 0x02
#define RTV2_OP_LOAD_F64_CONST 0x03
#define RTV2_OP_NUM_CAST 0x04
#define RTV2_OP_BINARY 0x05
#define RTV2_OP_COMPARE 0x06
#define RTV2_OP_ARRAY_GET 0x07
#define RTV2_OP_ARRAY_SET 0x08
#define RTV2_OP_CALL_DIRECT 0x09
#define RTV2_OP_INTRINSIC_SQRT 0x0A
#define RTV2_OP_JUMP 0x0B
#define RTV2_OP_JUMP_IF_FALSE 0x0C
#define RTV2_OP_V1_STMT 0x0D
#define RTV2_OP_RETURN 0x0E

struct RTV2Instruction {
    RTV2Opcode opcode = RTV2_OP_NOP;
    uint8_t kind = 0;
    uint8_t aux = 0;
    uint8_t flags = 0;
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t d = 0;
};

struct RTV2CallSite {
    std::string targetName;
    std::vector<uint32_t> argSlots;
};

struct RTV2FunctionImage {
    std::vector<RTV2Instruction> instructions;
    std::vector<int64_t> i64Consts;
    std::vector<double> f64Consts;
    std::vector<RTV2CallSite> callSites;
    std::vector<std::vector<char>> fallbackStmtBlobs;
};

#define RTCODE_STREAM_OBJECT(object) \
std::ostream & operator <<(std::ostream & os,object * obj); \
std::istream & operator >>(std::istream & is,object * obj);

struct RTCodeContext {
    
};

constexpr uint16_t RTBYTECODE_VERSION_LEGACY_V1 = 0;
constexpr uint16_t RTBYTECODE_VERSION_V1 = 1;
constexpr uint16_t RTBYTECODE_VERSION_V2 = 2;
constexpr size_t RTModuleHeaderByteCount = 8;

struct RTModuleHeaderInfo {
    uint16_t bytecodeVersion = RTBYTECODE_VERSION_LEGACY_V1;
    bool hasExplicitHeader = false;
};

struct RTID {
    size_t len;
    const char *value;
};

RTCODE_STREAM_OBJECT(RTID)

void writeRTModuleHeader(std::ostream &os,uint16_t bytecodeVersion = RTBYTECODE_VERSION_V1);
RTModuleHeaderInfo prepareRTModuleStream(std::istream &is);

bool rtBuiltinMemberIdForName(const std::string &name,RTBuiltinMemberId &idOut);
const char *rtBuiltinMemberName(RTBuiltinMemberId id);

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
    std::vector<char> decodedBody;
    RTV2FunctionImage v2Image;
    bool hasV2Image = false;
    std::string v2DecodeError;
};

RTCODE_STREAM_OBJECT(RTFuncTemplate)

void addRTInternalAttribute(RTFuncTemplate &func,const std::string &name);
bool hasRTInternalAttribute(const RTFuncTemplate &func,const std::string &name);
bool writeRTV2FunctionImage(std::ostream &os,const RTV2FunctionImage &image);
bool readRTV2FunctionImage(const char *data,size_t size,RTV2FunctionImage &imageOut,std::string *errorOut = nullptr);


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
