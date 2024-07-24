#include "ASTStmt.h"
#include "starbytes/base/Diagnostic.h"
#include <functional>

#ifndef STARBYTES_AST_TYPE_H
#define STARBYTES_AST_TYPE_H

namespace starbytes {

    class ASTIdentifier;

    class ASTType {
        std::string name;
        
        ASTStmt *parentNode;
    public:
        static ASTType *Create(string_ref name,ASTStmt *parentNode,bool isPlaceholder = true,bool isAlias = false);
        
        bool isPlaceholder;
        
        bool isAlias;
        
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

template<>
struct FormatProvider<starbytes::ASTType> {
    static void format(std::ostream & os,starbytes::ASTType &object ){
           os << object.getName();
           os.flush();
        if(!object.typeParams.empty()){
                typedef decltype(object.typeParams)::iterator TypeParamIt;
            os << "<";
                
              TypeParamIt it = object.typeParams.begin();
             while(it != object.typeParams.end()){
                     if(it != object.typeParams.begin() && it != (object.typeParams.end() + 1))
                         os << ",";
                     starbytes::ASTType & typeid_ref = **it;
                     format(os,typeid_ref);
                 };
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
