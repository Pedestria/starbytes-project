#include "starbytes/Base/Base.h"

#ifndef LANGDEVKIT_SUGGESTIONS_H
#define LANGDEVKIT_SUGGESTIONS_H

STARBYTES_STD_NAMESPACE

TYPED_ENUM LDKSuggestionTy:int {
    Hover,Completion,Diagonistic,WorkspaceFix
};

struct LDKParams {
    LDKSuggestionTy type;
};
struct LDKCompletionParams : LDKParams {
    static LDKSuggestionTy static_type;
};
struct LDKHoverParams : LDKParams {
    static LDKSuggestionTy static_type;
};
template<typename __TestTy>
inline bool ldk_params_is(LDKParams & ty){
    return ty.type == __TestTy::static_type;
};

#define LDK_PARAM_IS(ref,type) ldk_params_is<type>(ref)

template<typename __Suggestion_Ty>
struct LDKSuggestion {
    LDKSuggestionTy type;
    __Suggestion_Ty suggestion;
};

struct LDKCompletion {
   
};

struct LDKHover {
    
};

template<typename _Suggestion_Ty,typename __param_ty>
void provideSuggestion(LDKSuggestion<_Suggestion_Ty> & suggest_obj,LDKSuggestionTy type,__param_ty params);

template<>
void provideSuggestion(LDKSuggestion<LDKCompletion> & suggest_obj,LDKSuggestionTy type,LDKCompletionParams params);

template<>
void provideSuggestion(LDKSuggestion<LDKHover> & suggest_obj,LDKSuggestionTy type,LDKHoverParams params);

NAMESPACE_END



#endif