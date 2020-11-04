#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/ByteCode/BCDef.h"

#ifndef GEN_GEN_H
#define GEN_GEN_H

STARBYTES_GEN_NAMESPACE

using namespace AST;

#define FRIEND_AST_VISITOR(name) friend void visit##name(name * _node,CodeGenR * _gen)

class CodeGenR {
    private:
        using _program_out = ByteCode::BCProgram *; 
        using _program_src = AST::AbstractSyntaxTree *;
        using _program_in = std::vector<_program_src> &;
        unsigned bc_args_count;
        _program_out result;
        _program_in module_sources;
        void _generateAST(_program_src & src);
        void _pushNodeToOut(ByteCode::BCUnit *unit);
        unsigned & flushArgsCount();
    public:
        CodeGenR(_program_in m_srcs);
        _program_out & generate();
        FRIEND_AST_VISITOR(ASTClassDeclaration);
        FRIEND_AST_VISITOR(ASTFunctionDeclaration);
        FRIEND_AST_VISITOR(ASTVariableDeclaration);
};

ByteCode::BCProgram * generateToBCProgram(std::vector<AST::AbstractSyntaxTree *> &module_sources);


NAMESPACE_END

#endif

