#include "starbytes/AST/Type.h"
#include "starbytes/AST/ASTExpr.h"
#include <llvm/Support/FormatVariadic.h>

namespace starbytes {

    ASTType * VOID_TYPE = ASTType::Create("Void",nullptr,false);
    ASTType * STRING_TYPE = ASTType::Create("String",nullptr,false);
    ASTType * ARRAY_TYPE = ASTType::Create("Array",nullptr,false);
    ASTType * DICTIONARY_TYPE = ASTType::Create("Dict",nullptr,false);
    ASTType * BOOL_TYPE  = ASTType::Create("Bool",nullptr,false);
    ASTType * INT_TYPE = ASTType::Create("Int",nullptr,false);

    ASTType *ASTType::Create(llvm::StringRef name,ASTStmt *parentNode,bool isPlaceholder,bool isAlias){
        auto obj = new ASTType();
        obj->isAlias = isAlias;
        obj->name = name;
        obj->parentNode = parentNode;
        obj->isPlaceholder = isPlaceholder;
        return obj;
    };

    void ASTType::addTypeParam(ASTType *param){
        typeParams.push_back(param);
    };

    llvm::StringRef ASTType::getName() const{
        return name;
    };

    bool ASTType::nameMatches(ASTType *other){
        return name == other->name;
    };

    bool ASTType::match(ASTType *other,llvm::function_ref<void(const llvm::formatv_object_base &)> log){
        bool first_m = name == other->name;
        if(!first_m){
            log(llvm::formatv("Type `{0}` does not match type `{1}`",*this,*other));
            return false;
        };
        
        bool second_m = true;
        if(!typeParams.empty()){
            for(auto & tParam : typeParams){
                if(!tParam->match(other,log)){
                    second_m = false;
                    break;
                };
            };
        };
        
        return first_m && second_m;
    };

    ASTType::~ASTType(){
        if(!typeParams.empty()){
            for(auto & type : typeParams){
                delete type;
            };
            typeParams.clear();
        };
    };
};
