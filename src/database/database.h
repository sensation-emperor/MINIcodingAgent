#pragma once
#include <string>
#include <memory>

namespace aios {

class Database {
public:
    bool initialize() { return true; }
    void shutdown() {}
};

} // namespace aios
