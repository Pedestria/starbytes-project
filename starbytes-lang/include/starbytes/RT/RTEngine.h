#include <fstream>

#ifndef STARBYTES_RTENGINE_H
#define STARBYTES_RTENGINE_H

namespace starbytes {
namespace Runtime {


class Interp {
public:
    virtual void exec(std::istream & in) = 0;
    static std::shared_ptr<Interp> Create();
};

};
}

#endif
