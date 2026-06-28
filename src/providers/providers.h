#pragma once
#include <string>
#include <memory>

namespace aios {

class Providers {
public:
    bool initialize() { return true; }
    void shutdown() {}
};

} // namespace aios
