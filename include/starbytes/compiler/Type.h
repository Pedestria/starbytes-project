#include "ASTStmt.h"
#include "starbytes/base/Diagnostic.h"
#include <functional>

#ifndef STARBYTES_AST_TYPE_H
#define STARBYTES_AST_TYPE_H

namespace starbytes {

    class ASTIdentifier;

    class ASTType {
        string_ref name;
        
        ASTStmt *parentNode;
    public:
        static ASTType *Create(string_ref name,ASTStmt *parentNode,bool isPlaceholder = true,bool isAlias = false);
        
        bool isPlaceholder;
        
        bool isAlias;

        bool isGenericParam = false;

        bool isOptional = false;

        bool isThrowable = false;
        
        std::vector<ASTType *> typeParams;
        
        void addTypeParam(ASTType *type);
        
        bool nameMatches(ASTType *other);
        
        bool match(ASTType *other,std::function<void(std::string message)>);
        
        string_ref getName() const;
        
        ASTStmt *getParentNode() const;
        
        ~ASTType();
    };

extern ASTType * VOID_TYPE;
extern ASTType * STRING_TYPE;
extern ASTType * ARRAY_TYPE;
extern ASTType * DICTIONARY_TYPE;
extern ASTType * BOOL_TYPE;
extern ASTType * INT_TYPE;
extern ASTType * FLOAT_TYPE;
extern ASTType * REGEX_TYPE;
extern ASTType * ANY_TYPE;
extern ASTType * TASK_TYPE;
extern ASTType * FUNCTION_TYPE;

    template<>
    struct FormatProvider<starbytes::ASTType> {
    static void format(std::ostream & os,starbytes::ASTType &object ){
           os << object.getName();
           if(object.isOptional){
               os << "?";
           }
           if(object.isThrowable){
               os << "!";
           }
           os.flush();
        if(!object.typeParams.empty()){
            os << "<";
            for(size_t i = 0;i < object.typeParams.size();++i){
                if(i > 0){
                    os << ",";
                }
                starbytes::ASTType & typeRef = *object.typeParams[i];
                format(os,typeRef);
            }
            os << ">";
        };
    };
};

}

// namespace llvm {

//     
//         };
//     };
// };

#endif
