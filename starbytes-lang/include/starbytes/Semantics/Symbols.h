
#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"

#ifndef SEMANTICS_SYMBOLS_H
#define SEMANTICS_SYMBOLS_H

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

    enum class SymbolType:int {
        Variable,Class,Function
    }; 
    struct SemanticSymbol {
        SymbolType type;
        std::string name;
        public:
        SemanticSymbol(SymbolType _type):type(_type){};
        //bool checkWithOther!
        ~SemanticSymbol(){};
    };

    class VariableSymbol : public SemanticSymbol{
        public:
        VariableSymbol(ASTVariableSpecifier *& node_ptr_loc):SemanticSymbol(SymbolType::Variable),loc__ptr(node_ptr_loc){};
        ~VariableSymbol(){};
        SemanticSymbol *value_type;
        ASTVariableSpecifier *& loc__ptr;
        bool checkWithOther(VariableSymbol *sym);
        static SymbolType stat_type;

    };

    class ClassSymbol : public SemanticSymbol{
        public:
        ClassSymbol(ASTClassDeclaration *& node_ptr_loc):SemanticSymbol(SymbolType::Class),loc_ptr(node_ptr_loc){};
        ~ClassSymbol(){};
        ASTClassDeclaration *& loc_ptr;
        bool checkWithOther(ClassSymbol *sym);
        static SymbolType stat_type;
        
    };

    ClassSymbol * create_class_symbol(std::string name,ASTClassDeclaration *&node_ptr);

    class FunctionSymbol : public SemanticSymbol{
        public:
        FunctionSymbol(ASTFunctionDeclaration *& node_ptr_loc):SemanticSymbol(SymbolType::Function),loc_ptr(node_ptr_loc){};
        ~FunctionSymbol(){};
        ASTFunctionDeclaration *& loc_ptr;
        bool checkWithOther(FunctionSymbol *sym);
        static SymbolType stat_type;
        
    };

    template<class _SymbolTy>
    bool symbol_is(SemanticSymbol *_symbol){
        if(_symbol->type == _SymbolTy::stat_type){
            return true;
        }
        else {
            return false;
        }
    }

    #define SEMANTIC_SYMBOL_IS(ptr,type) symbol_is<type>((SemanticSymbol *)ptr)
    #define ASSERT_SEMANTIC_SYMBOL(ptr,type) ((type *)ptr)
    
NAMESPACE_END


#endif