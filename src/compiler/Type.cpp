#include "starbytes/compiler/Type.h"
#include "starbytes/compiler/ASTExpr.h"

namespace starbytes {

    ASTType * VOID_TYPE = ASTType::Create("Void",nullptr,false);
    ASTType * STRING_TYPE = ASTType::Create("String",nullptr,false);
    ASTType * ARRAY_TYPE = ASTType::Create("Array",nullptr,false);
    ASTType * DICTIONARY_TYPE = ASTType::Create("Dict",nullptr,false);
    ASTType * BOOL_TYPE  = ASTType::Create("Bool",nullptr,false);
    ASTType * INT_TYPE = ASTType::Create("Int",nullptr,false);
    ASTType * FLOAT_TYPE = ASTType::Create("Float",nullptr,false);
    ASTType * REGEX_TYPE = ASTType::Create("Regex",nullptr,false);
    ASTType * ANY_TYPE = ASTType::Create("Any",nullptr,false);
    ASTType * TASK_TYPE = ASTType::Create("Task",nullptr,false);

    ASTType *ASTType::Create(string_ref name,ASTStmt *parentNode,bool isPlaceholder,bool isAlias){
        auto obj = new ASTType();
        obj->isAlias = isAlias;
        obj->isOptional = false;
        obj->isThrowable = false;
        obj->name = name.getBuffer();
        obj->parentNode = parentNode;
        obj->isPlaceholder = isPlaceholder;
        return obj;
    }

    void ASTType::addTypeParam(ASTType *param){
        typeParams.push_back(param);
    }

    string_ref ASTType::getName() const{
        return name;
    }

    ASTStmt *ASTType::getParentNode() const{
        return parentNode;
    };

    bool ASTType::nameMatches(ASTType *other){
        return name == other->name;
    }

    bool ASTType::match(ASTType *other,std::function<void(std::string message)> log){
        if(!other){
            log("Type comparison failed against null type.");
            return false;
        }
        if(isGenericParam || other->isGenericParam){
            return true;
        }
        bool first_m = name == other->name;
        if(name == "Any" || other->name == "Any"){
            return true;
        }
        if(!first_m){
            log(fmtString("Type `{0}` does not match type `{1}`",*this,*other));
            return false;
        };

        if(!isOptional && other->isOptional){
            log(fmtString("Type `{0}` does not accept optional type `{1}`",*this,*other));
            return false;
        }

        if(!isThrowable && other->isThrowable){
            log(fmtString("Type `{0}` does not accept throwable type `{1}`",*this,*other));
            return false;
        }
        
        if(typeParams.size() != other->typeParams.size()){
            if(!typeParams.empty() || !other->typeParams.empty()){
                log(fmtString("Type parameter arity mismatch between `{0}` and `{1}`",*this,*other));
                return false;
            }
        }

        for(size_t i = 0;i < typeParams.size();++i){
            auto *lhsParam = typeParams[i];
            auto *rhsParam = other->typeParams[i];
            if(!lhsParam || !rhsParam){
                log("Type parameter comparison failed due to null type parameter.");
                return false;
            }
            if(!lhsParam->match(rhsParam,log)){
                return false;
            }
        }
        return first_m;
    }

    ASTType::~ASTType(){
        if(!typeParams.empty()){
            for(auto & type : typeParams){
                delete type;
            };
            typeParams.clear();
        };
    }
}
