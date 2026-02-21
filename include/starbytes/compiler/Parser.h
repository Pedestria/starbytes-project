#include <istream>
#include <memory>
#include <cstdint>
#include "Lexer.h"
#include "SyntaxA.h"
#include "SemanticA.h"
#include "AST.h"

#ifndef STARBYTES_PARSER_PARSER_H
#define STARBYTES_PARSER_PARSER_H
namespace starbytes {
    struct ModuleParseContext {
        std::string name;
        StringInterner stringStorage;
        Semantics::STableContext sTableContext;
        static ModuleParseContext Create(string_ref name);
    };

    class Parser {
    public:
        struct ProfileData {
            uint64_t totalNs = 0;
            uint64_t lexNs = 0;
            uint64_t syntaxNs = 0;
            uint64_t semanticNs = 0;
            uint64_t consumerNs = 0;
            uint64_t sourceBytes = 0;
            uint64_t tokenCount = 0;
            uint64_t statementCount = 0;
            uint64_t fileCount = 0;
        };

    private:
        std::unique_ptr<DiagnosticHandler> diagnosticHandler;

        std::unique_ptr<Syntax::Lexer> lexer;
        std::unique_ptr<Syntax::SyntaxA> syntaxA;
        std::vector<Syntax::Tok> tokenStream;
        std::unique_ptr<SemanticA> semanticA;
        ASTStreamConsumer & astConsumer;
        bool profilingEnabled = false;
        bool infer64BitNumbers = false;
        ProfileData profileData;
    public:
        void parseFromStream(std::istream & in,ModuleParseContext &moduleParseContext);
        bool finish();
        void setProfilingEnabled(bool enabled);
        void setInfer64BitNumbers(bool enabled);
        bool getInfer64BitNumbers() const;
        const ProfileData & getProfileData() const;
        void resetProfileData();
        DiagnosticHandler *getDiagnosticHandler();
        Parser(ASTStreamConsumer & astConsumer,std::unique_ptr<DiagnosticHandler> diagnostics = nullptr);
    };
};

#endif
