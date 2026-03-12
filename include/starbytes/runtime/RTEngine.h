#include <fstream>
#include <cstdint>
#include <memory>
#include <string>

#ifndef STARBYTES_RT_RTENGINE_H
#define STARBYTES_RT_RTENGINE_H

namespace starbytes::Runtime {

struct RuntimeProfileData {
    uint64_t quickenedSitesInstalled = 0;
    uint64_t quickenedExecutions = 0;
    uint64_t quickenedSpecializations = 0;
    uint64_t quickenedFallbacks = 0;
};


class Interp {
public:
    virtual void exec(std::istream & in) = 0;
    virtual bool addExtension(const std::string &path) = 0;
    virtual bool hasRuntimeError() const = 0;
    virtual std::string takeRuntimeError() = 0;
    virtual RuntimeProfileData getProfileData() const = 0;
    static std::shared_ptr<Interp> Create();
    virtual ~Interp() = default;
};

}

#endif
