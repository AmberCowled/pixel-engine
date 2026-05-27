#include <gtest/gtest.h>
#include <engine/core/EditorHistory.hpp>

class MockCommand : public PixelEngine::EditorCommand {
public:
    MockCommand(int* value, int target) : m_Value(value), m_Target(target), m_Old(0) {}

    void Execute() override {
        m_Old = *m_Value;
        *m_Value = m_Target;
    }

    void Undo() override {
        *m_Value = m_Old;
    }

    std::string GetName() const override { return "MockCommand"; }

private:
    int* m_Value;
    int m_Target;
    int m_Old;
};

class TestHistorySuite : public ::testing::Test {
protected:
    void SetUp() override {
        PixelEngine::EditorHistory::Clear();
        m_Value = 0;
    }

    void TearDown() override {
        PixelEngine::EditorHistory::Clear();
    }

    int m_Value;
};

TEST_F(TestHistorySuite, ExecuteAndUndoRedo) {
    EXPECT_FALSE(PixelEngine::EditorHistory::CanUndo());
    EXPECT_FALSE(PixelEngine::EditorHistory::CanRedo());
    EXPECT_FALSE(PixelEngine::EditorHistory::IsDirty());

    // Push command
    PixelEngine::EditorHistory::PushCommand(std::make_unique<MockCommand>(&m_Value, 42));
    EXPECT_EQ(m_Value, 42);
    EXPECT_TRUE(PixelEngine::EditorHistory::CanUndo());
    EXPECT_FALSE(PixelEngine::EditorHistory::CanRedo());
    EXPECT_TRUE(PixelEngine::EditorHistory::IsDirty());

    // Undo command
    PixelEngine::EditorHistory::Undo();
    EXPECT_EQ(m_Value, 0);
    EXPECT_FALSE(PixelEngine::EditorHistory::CanUndo());
    EXPECT_TRUE(PixelEngine::EditorHistory::CanRedo());

    // Redo command
    PixelEngine::EditorHistory::Redo();
    EXPECT_EQ(m_Value, 42);
    EXPECT_TRUE(PixelEngine::EditorHistory::CanUndo());
    EXPECT_FALSE(PixelEngine::EditorHistory::CanRedo());
}

TEST_F(TestHistorySuite, ClearStack) {
    PixelEngine::EditorHistory::PushCommand(std::make_unique<MockCommand>(&m_Value, 100));
    EXPECT_TRUE(PixelEngine::EditorHistory::CanUndo());
    EXPECT_TRUE(PixelEngine::EditorHistory::IsDirty());

    PixelEngine::EditorHistory::Clear();
    EXPECT_FALSE(PixelEngine::EditorHistory::CanUndo());
    EXPECT_FALSE(PixelEngine::EditorHistory::IsDirty());
}
