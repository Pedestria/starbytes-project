#include "starbytes/AST/ASTStmt.h"
#include "starbytes/Base/Diagnostic.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/FunctionExtras.h>
#include <llvm/Support/FormatVariadic.h>

#include <functional>

#ifndef STARBYTES_AST_TYPE_H
#define STARBYTES_AST_TYPE_H

namespace starbytes {

    class ASTIdentifier;

    class ASTType {
        std::string name;
        
        ASTStmt *parentNode;
    public:
        static ASTType *Create(llvm::StringRef name,ASTStmt *parentNode,bool isPlaceholder = true,bool isAlias = false);
        
        bool isPlaceholder;
        
        bool isAlias;
        
        llvm::SmallVector<ASTType *,3> typeParams;
        
        void addTypeParam(ASTType *type);
        
        bool nameMatches(ASTType *other);
        
        bool match(ASTType *other,llvm::function_ref<void(const llvm::formatv_object_base &)> log);
        
        llvm::StringRef getName() const;
        
        ~ASTType();
    };

}

namespace llvm {

    template<>
    struct format_provider<starbytes::ASTType> {
        static void format(const starbytes::ASTType &V,raw_ostream &Stream, StringRef Style){
            Stream << V.getName();
            Stream.flush();
            if(!V.typeParams.empty()){
                typedef decltype(V.typeParams)::const_iterator TypeParamIt;
                Stream << "<";
                
                TypeParamIt it = V.typeParams.begin();
                while(it != V.typeParams.end()){
                    if(it != V.typeParams.begin() && it != (V.typeParams.end() + 1))
                        Stream << ",";
                    const starbytes::ASTType & typeid_ref = **it;
                    format(typeid_ref,Stream,Style);
                };
                Stream << ">";
            };
        };
    };
};

#endif
