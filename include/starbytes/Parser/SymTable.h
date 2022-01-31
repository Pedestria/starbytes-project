#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringMap.h>
#include <vector>
#include "starbytes/AST/AST.h"

#ifndef STARBYTES_PARSER_SYMTABLE_H
#define STARBYTES_PARSER_SYMTABLE_H
namespace starbytes {

    struct SemanticsContext {
        DiagnosticBufferedLogger & errStream;
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
                llvm::StringMap<ASTType *> paramMap;
            };
            
            struct Class {
                ASTType *classType;
                llvm::SmallVector<Function *,2> instMethods;
                llvm::SmallVector<Var *,2> fields;
            };
            
            struct Entry {
                void *data;
                std::string name;
                SrcLoc interfacePos;
                SrcLoc sourcePos;
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
            llvm::DenseMap<Entry *,ASTScope *> body;
            llvm::SmallVector<std::string,2> deps;

        public:
            void importModule(llvm::StringRef moduleName);
            void addSymbolInScope(Entry *entry,ASTScope * scope);
            bool symbolExists(llvm::StringRef symbolName,ASTScope *scope);
            auto indexOf(llvm::StringRef symbolName,ASTScope *scope) -> decltype(body)::iterator;
            
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
            SymbolTable::Entry * findEntry(llvm::StringRef symbolName,SemanticsContext & ctxt,ASTScope *scope);
        };
    }

}

#endif
