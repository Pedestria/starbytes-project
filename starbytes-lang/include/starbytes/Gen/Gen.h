#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTTraveler.h"
#include <fstream>

#ifndef GEN_GEN_H
#define GEN_GEN_H

STARBYTES_GEN_NAMESPACE

using namespace AST;

class CodeGenR : public ASTTraveler<CodeGenR> {
    private:
        using _program_out = std::ofstream;
        using _program_src = AST::AbstractSyntaxTree *;
        using _program_in = std::vector<_program_src> &;
        unsigned bc_args_count;
        unsigned & flushArgsCount();
    public:
        CodeGenR(_program_out & _output);
        void _generateAST(_program_src & src);
        _program_out & out;
};

void generateToBCProgram(std::vector<AST::AbstractSyntaxTree *> &module_sources,std::ofstream & out);

#define VISITOR_RETURN ASTVisitorResponse response;response.success = true;return response;

NAMESPACE_END

#endif

