#include "starbytes/linguistics/CodeActionEngine.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

namespace starbytes::linguistics {

namespace {

void sortEditsDeterministically(std::vector<TextEdit> &edits) {
    std::sort(edits.begin(), edits.end(), [](const TextEdit &lhs, const TextEdit &rhs) {
        if(lhs.span.start.line != rhs.span.start.line) {
            return lhs.span.start.line < rhs.span.start.line;
        }
        if(lhs.span.start.character != rhs.span.start.character) {
            return lhs.span.start.character < rhs.span.start.character;
        }
        if(lhs.span.end.line != rhs.span.end.line) {
            return lhs.span.end.line < rhs.span.end.line;
        }
        if(lhs.span.end.character != rhs.span.end.character) {
            return lhs.span.end.character < rhs.span.end.character;
        }
        return lhs.replacement < rhs.replacement;
    });
}

bool safeOnlyEnabled(const LinguisticsConfig &config, const CodeActionRequest &request) {
    return request.safeOnly || config.actions.preferSafeActions;
}

std::string editsKey(const std::vector<TextEdit> &edits) {
    std::string key;
    for(const auto &edit : edits) {
        key += std::to_string(edit.span.start.line) + ":" + std::to_string(edit.span.start.character)
            + ":" + std::to_string(edit.span.end.line) + ":" + std::to_string(edit.span.end.character)
            + ":" + edit.replacement + ";";
    }
    return key;
}

void sortActionsDeterministically(std::vector<CodeAction> &actions) {
    std::sort(actions.begin(), actions.end(), [](const CodeAction &lhs, const CodeAction &rhs) {
        if(lhs.preferred != rhs.preferred) {
            return lhs.preferred && !rhs.preferred;
        }
        if(lhs.isSafe != rhs.isSafe) {
            return lhs.isSafe && !rhs.isSafe;
        }
        if(lhs.kind != rhs.kind) {
            return lhs.kind < rhs.kind;
        }
        if(lhs.edits.empty() != rhs.edits.empty()) {
            return !lhs.edits.empty();
        }
        if(!lhs.edits.empty() && !rhs.edits.empty()) {
            const auto &left = lhs.edits.front().span.start;
            const auto &right = rhs.edits.front().span.start;
            if(left.line != right.line) {
                return left.line < right.line;
            }
            if(left.character != right.character) {
                return left.character < right.character;
            }
        }
        if(lhs.title != rhs.title) {
            return lhs.title < rhs.title;
        }
        return lhs.id < rhs.id;
    });
}

} // namespace

CodeActionResult CodeActionEngine::build(const std::vector<LintFinding> &findings,
                                         const std::vector<Suggestion> &suggestions,
                                         const LinguisticsConfig &config,
                                         const CodeActionRequest &request) const {
    CodeActionResult result;
    std::unordered_set<std::string> seenKeys;
    auto safeOnly = safeOnlyEnabled(config, request);

    for(const auto &finding : findings) {
        for(const auto &fix : finding.fixes) {
            CodeAction action;
            action.id = fix.id.empty() ? (finding.id + ".fix") : fix.id;
            action.title = fix.title.empty() ? ("Apply fix: " + finding.code) : fix.title;
            action.kind = "quickfix";
            action.diagnosticRefs.push_back(!finding.code.empty() ? finding.code : finding.id);
            action.edits = fix.edits;
            sortEditsDeterministically(action.edits);
            action.preferred = fix.preferred;
            action.isSafe = fix.isSafe;

            if(action.edits.empty()) {
                continue;
            }
            if(safeOnly && !action.isSafe) {
                continue;
            }

            auto dedupeKey = action.kind + "|" + editsKey(action.edits);
            if(!seenKeys.insert(dedupeKey).second) {
                continue;
            }
            result.actions.push_back(std::move(action));
        }
    }

    for(const auto &suggestion : suggestions) {
        if(suggestion.edits.empty()) {
            continue;
        }

        CodeAction action;
        action.id = suggestion.id + ".apply";
        action.title = suggestion.title.empty() ? "Apply suggestion" : suggestion.title;
        action.kind = (suggestion.kind == SuggestionKind::Refactor) ? "refactor.rewrite" : "quickfix";
        action.diagnosticRefs.push_back(suggestion.id);
        action.edits = suggestion.edits;
        sortEditsDeterministically(action.edits);
        action.preferred = suggestion.confidence >= 0.90;
        action.isSafe = suggestion.confidence >= 0.75;

        if(safeOnly && !action.isSafe) {
            continue;
        }

        auto dedupeKey = action.kind + "|" + editsKey(action.edits);
        if(!seenKeys.insert(dedupeKey).second) {
            continue;
        }
        result.actions.push_back(std::move(action));
    }

    sortActionsDeterministically(result.actions);
    return result;
}

} // namespace starbytes::linguistics
