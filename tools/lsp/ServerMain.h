#include <istream>
#include <ostream>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <rapidjson/document.h>

#include "DocumentAnalysis.h"
#include "SymbolCache.h"
#include "starbytes/linguistics/CodeActionEngine.h"
#include "starbytes/linguistics/Config.h"
#include "starbytes/linguistics/FormatterEngine.h"
#include "starbytes/linguistics/LintEngine.h"
#include "starbytes/linguistics/SuggestionEngine.h"

#ifndef STARBYTES_LSP_SERVERMAIN_H
#define STARBYTES_LSP_SERVERMAIN_H


namespace starbytes {

class ASTStmt;

namespace Semantics {
struct SymbolTable;
}

namespace lsp {

struct SemanticResolvedDocument;

struct ServerOptions {
  std::istream &in;
  std::ostream &os;
};

class Server {
  struct DocumentAnalysisCache {
    int version = -1;
    uint64_t textHash = 0;
    bool diagnosticsReady = false;
    std::vector<CompilerDiagnosticEntry> diagnostics;
    bool semanticTokensReady = false;
    std::vector<SemanticTokenEntry> semanticTokens;
    bool lintReady = false;
    std::vector<starbytes::linguistics::LintFinding> lintFindings;
    bool suggestionsReady = false;
    std::vector<starbytes::linguistics::Suggestion> suggestions;
    bool safeActionsReady = false;
    std::vector<starbytes::linguistics::CodeAction> safeActions;
    bool allActionsReady = false;
    std::vector<starbytes::linguistics::CodeAction> allActions;
    bool formatReady = false;
    bool formatOk = false;
    std::string formattedText;
    bool semanticResolvedReady = false;
    std::shared_ptr<SemanticResolvedDocument> semanticResolvedDocument;
  };

  struct DocumentState {
    std::string text;
    int version = 0;
    bool isOpen = false;
    uint64_t textHash = 0;
    DocumentAnalysisCache analysis;
  };

  struct BuiltinsIndexCache {
    bool valid = false;
    std::string uri;
    int version = 0;
    uint64_t textHash = 0;
    BuiltinApiIndex index;
  };

  struct SemanticSnapshot {
    std::vector<unsigned> data;
    std::string resultId;
  };

  std::istream &in;
  std::ostream &out;

  bool serverOn = true;
  bool initialized = false;
  bool shutdownRequested = false;
  std::string traceLevel = "off";

  unsigned long long semanticResultCounter = 1;
  std::vector<std::string> workspaceRoots;
  std::unordered_map<std::string, DocumentState> documents;
  std::unordered_map<std::string, SemanticSnapshot> semanticSnapshots;
  std::unordered_set<std::string> canceledRequestIds;
  SymbolCache symbolCache;
  BuiltinsIndexCache builtinsIndexCache;
  starbytes::linguistics::LinguisticsConfig linguisticsConfig = starbytes::linguistics::LinguisticsConfig::defaults();
  starbytes::linguistics::FormatterEngine formatterEngine;
  starbytes::linguistics::LintEngine lintEngine;
  starbytes::linguistics::SuggestionEngine suggestionEngine;
  starbytes::linguistics::CodeActionEngine codeActionEngine;
  bool linguisticsProfilingEnabled = false;
  uint64_t lspProfileLintNs = 0;
  uint64_t lspProfileSuggestionNs = 0;
  uint64_t lspProfileActionNs = 0;
  uint64_t lspProfileFormatNs = 0;
  uint64_t lspProfileDiagnosticsNs = 0;
  uint64_t lspProfileCodeActionNs = 0;
  uint64_t lspProfileFormattingNs = 0;

  bool readMessage(std::string &body);
  void writeMessage(rapidjson::Document &doc);
  void writeResult(const rapidjson::Value &id, rapidjson::Value &result);
  void writeError(const rapidjson::Value &id, int code, const char *message);
  void writeNotification(const char *method, rapidjson::Value &params);

  static std::string trim(const std::string &in);
  static bool isIdentifierChar(char c);
  static bool isIdentifierStart(char c);
  static uint64_t hashText(const std::string &text);
  static bool parseUriToPath(const std::string &uri, std::string &pathOut);
  static std::string pathToUri(const std::string &path);
  static size_t offsetFromPosition(const std::string &text, unsigned line, unsigned character);
  static void positionFromOffset(const std::string &text, size_t offset, unsigned &lineOut, unsigned &charOut);
  static bool wordRangeAtPosition(const std::string &text,
                                  unsigned line,
                                  unsigned character,
                                  std::string &wordOut,
                                  size_t &startOut,
                                  size_t &endOut);
  static std::string extractPrefixAtPosition(const std::string &text, unsigned line, unsigned character);
  static void appendOccurrences(const std::string &text,
                                const std::string &name,
                                std::vector<size_t> &offsetsOut,
                                size_t rangeStart = 0,
                                size_t rangeEnd = std::string::npos);

  void loadWorkspaceDocuments();
  bool getBuiltinsDocument(std::string &uriOut, std::string &textOut);
  bool getDocumentTextByUri(const std::string &uri, std::string &textOut);
  void setDocumentTextByUri(const std::string &uri, const std::string &text, int version, bool isOpen);
  void removeOpenDocumentByUri(const std::string &uri);
  void publishDiagnostics(const std::string &uri);
  void appendLinguisticsDiagnosticsJson(const std::string &uri,
                                        const std::string &text,
                                        rapidjson::Value &diagnostics,
                                        rapidjson::Document::AllocatorType &alloc);
  void maybeApplyIncrementalChange(std::string &text, const rapidjson::Value &change);
  void configureSymbolCache();
  const std::vector<CompilerDiagnosticEntry> &getCompilerDiagnosticsForDocument(const std::string &uri,
                                                                                DocumentState &state);
  std::vector<CompilerDiagnosticEntry> buildCompilerDiagnosticsFromSemanticContext(const std::string &uri,
                                                                                   DocumentState &state);
  const std::vector<SemanticTokenEntry> &getSemanticTokensForDocument(const std::string &uri, DocumentState &state);
  const std::vector<starbytes::linguistics::LintFinding> &getLintFindingsForDocument(const std::string &uri,
                                                                                      DocumentState &state);
  const std::vector<starbytes::linguistics::Suggestion> &getSuggestionsForDocument(const std::string &uri,
                                                                                     DocumentState &state);
  const std::vector<starbytes::linguistics::CodeAction> &getCodeActionsForDocument(const std::string &uri,
                                                                                     DocumentState &state,
                                                                                     bool safeOnly);
  bool getFormattedTextForDocument(const std::string &uri, DocumentState &state, std::string &formattedTextOut);
  void maybeLogLspProfileSample(const char *label, uint64_t elapsedNs, size_t itemCount = 0);
  void maybeLogLspProfileSummary();
  const BuiltinApiIndex *getBuiltinsApiIndex();
  std::vector<SymbolEntry> collectSymbolsForUri(const std::string &uri, const std::string &text);
  void invalidateSymbolCacheForUri(const std::string &uri);
  void refreshAnalysisState(DocumentState &state);
  const SemanticResolvedDocument *getSemanticResolvedDocumentForUri(const std::string &uri, DocumentState &state);
  std::shared_ptr<SemanticResolvedDocument> buildSemanticResolvedDocumentForUri(
      const std::string &uri,
      DocumentState &state,
      std::unordered_set<std::string> &activeUris);
  std::vector<SemanticTokenEntry> buildSemanticTokensFromSemanticCache(const std::string &uri, DocumentState &state);
  bool findModuleDocumentByName(const std::string &moduleName,
                                const std::string &anchorUri,
                                std::string &uriOut,
                                std::string &textOut);
  bool resolveHoverSymbol(const std::string &uri,
                          const std::string &text,
                          unsigned line,
                          unsigned character,
                          const std::string &word,
                          size_t wordStart,
                          std::string &resolvedUriOut,
                          SymbolEntry &symbolOut);

  void handleInitialize(rapidjson::Document &request);
  void handleInitialized(rapidjson::Document &request);
  void handleShutdown(rapidjson::Document &request);
  void handleCancelRequest(rapidjson::Document &request);
  void handleSetTrace(rapidjson::Document &request);
  void handleLogTrace(rapidjson::Document &request);
  void handleWorkDoneProgressCreate(rapidjson::Document &request);
  void handleCompletion(rapidjson::Document &request);
  void handleCompletionResolve(rapidjson::Document &request);
  void handleHover(rapidjson::Document &request);
  void handleDefinition(rapidjson::Document &request);
  void handleDeclaration(rapidjson::Document &request);
  void handleTypeDefinition(rapidjson::Document &request);
  void handleImplementation(rapidjson::Document &request);
  void handleReferences(rapidjson::Document &request);
  void handleDocumentSymbol(rapidjson::Document &request);
  void handleWorkspaceSymbol(rapidjson::Document &request);
  void handlePrepareRename(rapidjson::Document &request);
  void handleRename(rapidjson::Document &request);
  void handleSignatureHelp(rapidjson::Document &request);
  void handleDocumentHighlight(rapidjson::Document &request);
  void handleFormatting(rapidjson::Document &request);
  void handleRangeFormatting(rapidjson::Document &request);
  void handleFoldingRange(rapidjson::Document &request);
  void handleSelectionRange(rapidjson::Document &request);
  void handleCodeAction(rapidjson::Document &request);
  void handleSemanticTokens(rapidjson::Document &request);
  void handleSemanticTokensRange(rapidjson::Document &request);
  void handleSemanticTokensDelta(rapidjson::Document &request);
  void handleDidOpen(rapidjson::Document &request);
  void handleDidChange(rapidjson::Document &request);
  void handleDidSave(rapidjson::Document &request);
  void handleDidClose(rapidjson::Document &request);
  void processRequest(rapidjson::Document &request);
  void processNotification(rapidjson::Document &request);
public:
  explicit Server(starbytes::lsp::ServerOptions &options);
  void run();
};

};

};

#endif // STARBYTES_LSP_SERVERMAIN_H
