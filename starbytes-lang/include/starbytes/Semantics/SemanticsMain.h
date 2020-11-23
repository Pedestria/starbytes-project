#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"
#include "Scope.h"
#include "starbytes/AST/ASTTraveler.h"
#include "starbytes/Semantics/StarbytesDecl.h"

#ifndef SEMANTICS_SEMANTICS_MAIN_H
#define SEMANTICS_SEMANTICS_MAIN_H

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

#define SEMANTICA_FUNC(name) ASTVisitorResponse at##name(ASTTravelContext * context)
        
    class STBType;
    struct STBObjectMethod;
    struct STBObjectProperty;
    class SemanticA : public ASTTraveler<SemanticA> {
        using Tree = AbstractSyntaxTree;
        using SemanticAError = std::string;
        private:
            Tree * tree;
            ScopeStore store;
            std::vector<std::string> modules;
            std::vector<SemanticAError> errors;
            void createScope(std::string & name);
            void registerSymbolInScope(std::string & scope,SemanticSymbol *symbol);
            void registerSymbolinExactCurrentScope(SemanticSymbol *symbol);

            template<typename _Node>
            friend inline void construct_methods_and_props(std::vector<STBObjectMethod> *methods,std::vector<STBObjectProperty> *props,_Node *node,SemanticA *& sem);
            

        public:
            void freeSymbolStores();
            void initialize();
            void analyzeFileForModule(Tree *& tree_ptr);
            void finish();
            SemanticA();
            ~SemanticA(){};
    };

    inline std::string StarbytesSemanticsError(std::string message,DocumentPosition & pos){
        return "SemanticsError:\n"+message.append("\nThis Error Has Occured at {Line:"+std::to_string(pos.line)+",Start:"+std::to_string(pos.start)+",End:"+std::to_string(pos.end)+"}");
    };

NAMESPACE_END

#endif