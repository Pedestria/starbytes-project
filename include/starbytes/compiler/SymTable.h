
#include <vector>
#include "AST.h"

#ifndef STARBYTES_PARSER_SYMTABLE_H
#define STARBYTES_PARSER_SYMTABLE_H
namespace starbytes {

    struct SemanticsContext {
        DiagnosticHandler & errStream;
        ASTStmt *currentStmt;
    };

    namespace Semantics  {
    
        struct SymbolTable {
            
            struct Var {
                ASTType *type;
            };
            
            struct Function {
                ASTType *funcType;
                ASTType *returnType;
                string_map<ASTType *> paramMap;
            };
            
            struct Class {
                ASTType *classType;
                std::vector<Function *> instMethods;
                std::vector<Var *> fields;
            };
            
            struct Entry {
                void *data;
                std::string name;
                Region interfacePos;
                Region sourcePos;
                typedef enum : int {
                    Var,
                    Class,
                    Interface,
                    Scope,
                    Function
                } Ty;
                Ty type;
            };
        private:
            
            friend struct STableContext;
            std::map<Entry *,std::shared_ptr<ASTScope>> body;
            std::vector<std::string> deps;

        public:
            void importModule(string_ref moduleName);
            void addSymbolInScope(Entry *entry,std::shared_ptr<ASTScope> scope);
            bool symbolExists(string_ref symbolName,std::shared_ptr<ASTScope> scope);
            auto indexOf(string_ref symbolName,std::shared_ptr<ASTScope> scope) -> decltype(body)::iterator;
            
            /// IO Methods
            
            static std::shared_ptr<SymbolTable> importPublic(std::istream & input);
            /// Upon export of the SymbolTable only publically visible symbols will be serialized.
            void serializePublic(std::ostream & out);
            
            ~SymbolTable();
        };

        struct STableContext {
            std::unique_ptr<SymbolTable> main;
            std::vector<std::shared_ptr<SymbolTable>> otherTables;
            bool hasTable(SymbolTable *ptr);
            SymbolTable::Entry * findEntry(string_ref symbolName,SemanticsContext & ctxt,std::shared_ptr<ASTScope> scope);
        };
    }

}

#endif
