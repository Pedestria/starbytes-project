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
            
            struct Function {
                ASTType *returnType;
                llvm::StringMap<ASTType *> paramMap;
            };
            struct Class {
                ASTType *type;
                llvm::SmallVector<Function *,2> instMethods;
            };
            struct Var {
                ASTType *type;
            };
            
            struct Entry {
                void *data;
                std::string name;
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
        public:
            void addSymbolInScope(Entry *entry,ASTScope * scope);
            bool symbolExists(llvm::StringRef symbolName,ASTScope *scope);
            ~SymbolTable();
        };

        struct STableContext {
            std::unique_ptr<SymbolTable> main;
            std::vector<SymbolTable *> otherTables;
            bool hasTable(SymbolTable *ptr);
            SymbolTable::Entry * findEntry(llvm::StringRef symbolName,SemanticsContext & ctxt,ASTScope *scope);
        };
    }

}

#endif
