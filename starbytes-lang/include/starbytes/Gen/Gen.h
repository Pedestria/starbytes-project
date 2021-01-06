#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTTraveler.h"
#include <fstream>

#ifndef GEN_GEN_H
#define GEN_GEN_H

STARBYTES_GEN_NAMESPACE

using namespace AST;

struct CodeGenROpts {
    std::vector<std::string> link_modules;
    std::vector<std::string> module_dirs;
    CodeGenROpts(std::vector<std::string> & _l_mds,std::vector<std::string> & _m_dirs):link_modules(_l_mds),module_dirs(_m_dirs){};
};
class CodeGenR : public ASTTraveler<CodeGenR> {
    public:
        using _program_out = std::ofstream;
        using _program_src = AST::AbstractSyntaxTree *;
        using _program_in = std::vector<_program_src> &;
        CodeGenR(_program_out & _output);
        void _generateAST(_program_src & src);
        _program_out & out;
};

void generateToBCProgram(std::vector<AST::AbstractSyntaxTree *> &module_sources,std::ofstream & out,CodeGenROpts & opts);

#define VISITOR_RETURN ASTVisitorResponse response;response.success = true;return response;

NAMESPACE_END

#endif

