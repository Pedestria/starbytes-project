#include "starbytes/Parser/Parser.h"
#include "starbytes/Base/Document.h"


#ifndef CORE_CORE_H
#define CORE_CORE_H

STARBYTES_STD_NAMESPACE

struct DriverOpts {
    std::string directory;
    std::string out;
};
class Driver {
    const DriverOpts & opts;
    void evalDirEntry(const std::filesystem::directory_entry & entry);
    std::vector<std::string> srcs;
    public:
    Driver() = delete;
    Driver(const DriverOpts &_opts):opts(_opts){};
    void doWork();
};

NAMESPACE_END

#endif