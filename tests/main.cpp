#include <gtest/gtest.h>
#include <engine/base/Log.hpp>

int main(int argc, char** argv) {
    PixelEngine::Log::Init();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
