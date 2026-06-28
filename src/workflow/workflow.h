#pragma once
#include <string>
#include <memory>

namespace aios {

class Workflow {
public:
    bool initialize() { return true; }
    void shutdown() {}
};

} // namespace aios
