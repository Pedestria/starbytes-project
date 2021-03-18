#include "Suggestions.h"

STARBYTES_STD_NAMESPACE

LDKSuggestionTy LDKCompletionParams::static_type = LDKSuggestionTy::Completion;
LDKSuggestionTy LDKHoverParams::static_type = LDKSuggestionTy::Hover;

void provide_completion_suggestion(const LDKCompletionParams & params,const LDKCompletion & result){

};

void provide_hover_suggestion(const LDKHoverParams & params,const LDKHover & result){

};


template<>
void provideSuggestion(LDKSuggestion<LDKCompletion> & suggest_obj,LDKSuggestionTy type,LDKCompletionParams params){
    if(LDK_PARAM_IS(params,LDKCompletionParams)){
        suggest_obj.type = type;
        provide_completion_suggestion(params,suggest_obj.suggestion);
    }
};

template<>
void provideSuggestion(LDKSuggestion<LDKHover> & suggest_obj,LDKSuggestionTy type,LDKHoverParams params){
    if(LDK_PARAM_IS(params,LDKHover)){
        suggest_obj.type = type;
        provide_hover_suggestion(params,suggest_obj.suggestion);
    }
};


NAMESPACE_END