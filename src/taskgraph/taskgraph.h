#pragma once
#include <string>
#include <memory>

namespace aios {

class Taskgraph {
public:
    bool initialize() { return true; }
    void shutdown() {}
};

} // namespace aios
