#include <gtest/gtest.h>
#include "logging/Logger.h"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    aios::Logger::initialize("aios_tests", aios::LogLevel::Info);
    return RUN_ALL_TESTS();
}
