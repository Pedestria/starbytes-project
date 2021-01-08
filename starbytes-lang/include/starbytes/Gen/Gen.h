#include "starbytes/Base/Base.h"
#include "starbytes/AST/AST.h"
#include "starbytes/AST/ASTTraveler.h"
#include "starbytes/Base/Module.h"
#include <fstream>

#ifndef GEN_GEN_H
#define GEN_GEN_H

STARBYTES_GEN_NAMESPACE

using namespace AST;

struct CodeGenROpts {
    const ModuleSearch & m_search;
    Foundation::ArrRef<std::string> modules_to_link;
    CodeGenROpts(const ModuleSearch & _m_search,Foundation::ArrRef<std::string> & m_link):m_search(_m_search),modules_to_link(m_link){};
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

void generateToBCProgram(Foundation::ArrRef<AST::AbstractSyntaxTree *> module_sources,std::ofstream & out,CodeGenROpts & opts);

#define VISITOR_RETURN ASTVisitorResponse response;response.success = true;return response;

NAMESPACE_END

#endif

