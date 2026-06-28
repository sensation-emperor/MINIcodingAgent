#pragma once
#include <string>
#include <memory>

namespace aios {

class Filesystem {
public:
    bool initialize() { return true; }
    void shutdown() {}
    void stop() {}
};

} // namespace aios
