# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

add_executable(engine-tests
    main.cpp
    TestECS.cpp
    TestHistory.cpp
    TestEditor.cpp
)

target_link_libraries(engine-tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    editor
    engine-core
    engine-ecs
    engine-assets
    engine-audio
    engine-renderer
    engine-scripting
)

target_include_directories(engine-tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(engine-tests)
