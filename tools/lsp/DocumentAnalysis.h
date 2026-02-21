#ifndef STARBYTES_LSP_DOCUMENTANALYSIS_H
#define STARBYTES_LSP_DOCUMENTANALYSIS_H

#include "SymbolTypes.h"
#include "starbytes/base/Diagnostic.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace starbytes::lsp {

struct BuiltinApiIndex {
  std::string uri;
  std::unordered_map<std::string, SymbolEntry> membersByQualifiedName;
  std::unordered_map<std::string, std::unordered_map<std::string, SymbolEntry>> membersByType;
  std::unordered_map<std::string, SymbolEntry> topLevelByName;
};

struct CompilerDiagnosticEntry {
  Region region;
  int severity = 1;
  std::string message;
  std::string id;
  std::string code;
  std::string phase;
  std::string source = "starbytes-compiler";
  std::string producerSource;
  std::vector<Diagnostic::RelatedSpan> relatedSpans;
  std::vector<std::string> notes;
  std::vector<Diagnostic::FixIt> fixits;
};

bool isBuiltinsInterfaceUri(const std::string &uri);
int completionKindFromSymbolKind(int symbolKind);

std::vector<CompilerDiagnosticEntry> collectCompilerDiagnosticsForText(const std::string &sourceName,
                                                                       const std::string &text);
std::vector<SymbolEntry> collectSymbolsFromText(const std::string &text);
BuiltinApiIndex buildBuiltinApiIndex(const std::string &uri, const std::string &text);

bool extractReceiverBeforeOffset(const std::string &text, size_t memberStartOffset, std::string &receiverOut);
bool extractMemberCompletionContext(const std::string &text,
                                    size_t cursorOffset,
                                    std::string &receiverOut,
                                    std::string &memberPrefixOut);
std::optional<std::string> inferBuiltinTypeForReceiver(const std::string &text,
                                                       const std::string &receiver,
                                                       unsigned maxLine,
                                                       const BuiltinApiIndex &index);

bool signaturePartsFromSignature(const std::string &signature,
                                 std::string &nameOut,
                                 std::string &paramsOut,
                                 std::string &returnOut);
bool findFunctionSignatureInText(const std::string &text,
                                 const std::string &functionName,
                                 std::string &nameOut,
                                 std::string &paramsOut,
                                 std::string &returnOut);

std::vector<SemanticTokenEntry> collectSemanticTokenEntries(const std::string &text,
                                                            unsigned rangeStartLine,
                                                            unsigned rangeEndLine,
                                                            bool useRange);
std::vector<unsigned> encodeSemanticTokens(const std::vector<SemanticTokenEntry> &tokens);

}

#endif
