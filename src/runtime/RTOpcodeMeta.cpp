#include "RTOpcodeMeta.h"

namespace starbytes::Runtime {

bool isExpressionOpcode(RTCode code){
    return code == CODE_RTIVKFUNC
        || code == CODE_RTCALL_DIRECT
        || code == CODE_RTCALL_BUILTIN_MEMBER
        || code == CODE_UNARY_OPERATOR
        || code == CODE_RTTYPED_NEGATE
        || code == CODE_BINARY_OPERATOR
        || code == CODE_RTTYPED_BINARY
        || code == CODE_RTTYPED_COMPARE
        || code == CODE_RTTYPECHECK
        || code == CODE_RTTERNARY
        || code == CODE_RTCAST
        || code == CODE_RTTYPED_INTRINSIC
        || code == CODE_RTMEMBER_SET
        || code == CODE_RTMEMBER_SET_FIELD_SLOT
        || code == CODE_RTMEMBER_IVK
        || code == CODE_RTMEMBER_GET
        || code == CODE_RTMEMBER_GET_FIELD_SLOT
        || code == CODE_RTNEWOBJ
        || code == CODE_RTREGEX_LITERAL
        || code == CODE_RTVAR_SET
        || code == CODE_RTLOCAL_REF
        || code == CODE_RTTYPED_LOCAL_REF
        || code == CODE_RTLOCAL_SET
        || code == CODE_RTARRAY_LITERAL
        || code == CODE_RTINDEX_GET
        || code == CODE_RTTYPED_INDEX_GET
        || code == CODE_RTINDEX_SET
        || code == CODE_RTTYPED_INDEX_SET
        || code == CODE_RTDICT_LITERAL
        || code == CODE_RTOBJCREATE
        || code == CODE_RTINTOBJCREATE
        || code == CODE_RTVAR_REF
        || code == CODE_RTOBJVAR_REF
        || code == CODE_RTFUNC_REF;
}

bool siteKindForOpcode(RTCode code, RuntimeProfileSiteKind &kindOut){
    switch(code){
        case CODE_RTIVKFUNC:
        case CODE_RTCALL_DIRECT:
            kindOut = RuntimeProfileSiteKind::Call;
            return true;
        case CODE_RTCALL_BUILTIN_MEMBER:
        case CODE_RTMEMBER_GET:
        case CODE_RTMEMBER_GET_FIELD_SLOT:
        case CODE_RTMEMBER_SET:
        case CODE_RTMEMBER_SET_FIELD_SLOT:
        case CODE_RTMEMBER_IVK:
            kindOut = RuntimeProfileSiteKind::Member;
            return true;
        case CODE_RTINDEX_GET:
        case CODE_RTTYPED_INDEX_GET:
        case CODE_RTINDEX_SET:
        case CODE_RTTYPED_INDEX_SET:
            kindOut = RuntimeProfileSiteKind::Index;
            return true;
        case CODE_RTTERNARY:
            kindOut = RuntimeProfileSiteKind::Branch;
            return true;
        default:
            return false;
    }
}

}
