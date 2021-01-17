#include "starbytes/AST/AST.h"
#include "starbytes/Base/Base.h"
#include "Scope.h"
#include "starbytes/AST/ASTTraveler.h"
#include "starbytes/Semantics/StarbytesDecl.h"
#include "starbytes/Base/Logging.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/ArrayRef.h>

#ifndef SEMANTICS_SEMANTICS_MAIN_H
#define SEMANTICS_SEMANTICS_MAIN_H

STARBYTES_SEMANTICS_NAMESPACE

using namespace AST;

#define SEMANTICA_FUNC(name) friend ASTVisitorResponse at##name(ASTTravelContext & context,SemanticA * sem)
#define VISITOR_END return reply;

    struct SemanticASettings {
        bool isLDK;
        const ModuleSearch & m_search;
        ArrayRef<StringRef> modules_to_link;
        SemanticASettings(bool _isLDK,const ModuleSearch & _m_search,ArrayRef<StringRef> & vector_ref):isLDK(_isLDK),m_search(_m_search),modules_to_link(vector_ref){};
    };

    TYPED_ENUM DiagnosticTy {
        Error,Warning,Advice
    };
    struct SemanticADiagnostic {
        DiagnosticTy type;
        std::string message;
        std::string file;
        SrcLocation pos;
        std::string get(){
            return message;
        };
    };

    SemanticADiagnostic && makeSemanticADiagnostic(DiagnosticTy type,std::string message,std::string file_n,SrcLocation loc);

    class DiagnosticLogger : public Foundation::Buffered_Logger<SemanticADiagnostic> {
        public:
        void removeDiagonisticAtIndex(unsigned idx);
        void clearBufAndLogAll(){
            return clearBuffer();
        };
        DiagnosticLogger(){};
        ~DiagnosticLogger(){};
        DiagnosticLogger(const DiagnosticLogger &) = delete;
        DiagnosticLogger operator=(DiagnosticLogger &&) = delete;
    };


        
    class STBType;
    struct STBObjectMethod;
    struct STBObjectProperty;
    
    class SemanticA : public ASTTraveler<SemanticA> {
        using Tree = AbstractSyntaxTree;
        private:
            SemanticASettings & settings;
            Tree * tree;
            ScopeStore store;
            std::vector<std::string> modules;
            DiagnosticLogger err_stream;
            void createScope(std::string & name);
            void registerSymbolInScope(std::string & scope,SemanticSymbol *symbol);
            void registerSymbolinExactCurrentScope(SemanticSymbol *symbol);

            template<typename _Node>
            friend inline void construct_methods_and_props(std::vector<STBObjectMethod> *methods,std::vector<STBObjectProperty> *props,_Node *node,SemanticA *& sem);
            SEMANTICA_FUNC(VarDecl);
            SEMANTICA_FUNC(ConstDecl);

        public:
            void freeSymbolStores();
            void initialize();
            void analyzeFileForModule(Tree *& tree_ptr);
            //Has SemanticA finished with NO errors!
            bool finish();
            ScopeStore finishAndDumpStore();
            SemanticA() = delete;
            SemanticA(const SemanticA &) = delete;
            SemanticA operator=(SemanticA &&) = delete;
            SemanticA(SemanticASettings & _settings);
            ~SemanticA(){};
    };

NAMESPACE_END

#endif