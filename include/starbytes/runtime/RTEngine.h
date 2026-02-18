#include <fstream>
#include <memory>
#include <string>

#ifndef STARBYTES_RT_RTENGINE_H
#define STARBYTES_RT_RTENGINE_H

namespace starbytes::Runtime {



class Interp {
public:
    virtual void exec(std::istream & in) = 0;
    virtual bool addExtension(const std::string &path) = 0;
    static std::shared_ptr<Interp> Create();
    virtual ~Interp() = default;
};

}

#endif
