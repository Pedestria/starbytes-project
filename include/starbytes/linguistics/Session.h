#ifndef STARBYTES_LINGUISTICS_SESSION_H
#define STARBYTES_LINGUISTICS_SESSION_H

#include <cstdint>
#include <string>

namespace starbytes::linguistics {

class LinguisticsSession {
public:
    LinguisticsSession();
    LinguisticsSession(std::string sourceName, std::string sourceText);

    void setSource(std::string sourceName, std::string sourceText);

    const std::string &getSourceName() const;
    const std::string &getSourceText() const;
    uint64_t getSourceHash() const;

private:
    std::string sourceName;
    std::string sourceText;
    uint64_t sourceHash = 0;
};

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_SESSION_H
