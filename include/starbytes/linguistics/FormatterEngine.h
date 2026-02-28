#ifndef STARBYTES_LINGUISTICS_FORMATTERENGINE_H
#define STARBYTES_LINGUISTICS_FORMATTERENGINE_H

#include <string>
#include <vector>

#include "Config.h"
#include "Session.h"

namespace starbytes::linguistics {

struct FormatRequest {
    bool previewMode = false;
    bool requireParseValidation = true;
    bool requireIdempotence = true;
    bool allowFallbackToPreview = true;
};

struct FormatResult {
    bool ok = true;
    std::string formattedText;
    std::vector<std::string> notes;
};

class FormatterEngine {
public:
    FormatResult format(const LinguisticsSession &session,
                        const LinguisticsConfig &config,
                        const FormatRequest &request = FormatRequest()) const;

    static std::string normalizePreview(const std::string &source,
                                        const FormattingConfig &config);
};

} // namespace starbytes::linguistics

#endif // STARBYTES_LINGUISTICS_FORMATTERENGINE_H
