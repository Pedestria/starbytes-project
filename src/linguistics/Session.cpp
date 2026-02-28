#include "starbytes/linguistics/Session.h"

#include <cstddef>
#include <utility>

namespace starbytes::linguistics {

namespace {

uint64_t hashBytes(const char *data, size_t size) {
    uint64_t hash = 1469598103934665603ULL;
    for(size_t i = 0; i < size; ++i) {
        hash ^= static_cast<unsigned char>(data[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

LinguisticsSession::LinguisticsSession() = default;

LinguisticsSession::LinguisticsSession(std::string sourceNameIn, std::string sourceTextIn) {
    setSource(std::move(sourceNameIn), std::move(sourceTextIn));
}

void LinguisticsSession::setSource(std::string sourceNameIn, std::string sourceTextIn) {
    sourceName = std::move(sourceNameIn);
    sourceText = std::move(sourceTextIn);
    sourceHash = hashBytes(sourceText.data(), sourceText.size());
}

const std::string &LinguisticsSession::getSourceName() const {
    return sourceName;
}

const std::string &LinguisticsSession::getSourceText() const {
    return sourceText;
}

uint64_t LinguisticsSession::getSourceHash() const {
    return sourceHash;
}

} // namespace starbytes::linguistics
