#pragma once

#include <vector>
#include <memory>
#include <string>

namespace PixelEngine {

    class EditorCommand {
    public:
        virtual ~EditorCommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual std::string GetName() const = 0;
    };

    class EditorHistory {
    public:
        static void PushCommand(std::unique_ptr<EditorCommand> command) {
            command->Execute();
            
            // Clear redo stack on new action
            s_RedoStack.clear();
            
            s_UndoStack.push_back(std::move(command));
            if (s_UndoStack.size() > s_MaxHistorySize) {
                s_UndoStack.erase(s_UndoStack.begin());
            }
            
            s_Dirty = true;
        }

        static void Undo() {
            if (s_UndoStack.empty()) return;
            
            auto command = std::move(s_UndoStack.back());
            s_UndoStack.pop_back();
            command->Undo();
            s_RedoStack.push_back(std::move(command));
            
            s_Dirty = true;
        }

        static void Redo() {
            if (s_RedoStack.empty()) return;
            
            auto command = std::move(s_RedoStack.back());
            s_RedoStack.pop_back();
            command->Execute();
            s_UndoStack.push_back(std::move(command));
            
            s_Dirty = true;
        }

        static bool CanUndo() { return !s_UndoStack.empty(); }
        static bool CanRedo() { return !s_RedoStack.empty(); }
        
        static bool IsDirty() { return s_Dirty; }
        static void SetDirty(bool dirty) { s_Dirty = dirty; }
        
        static void Clear() {
            s_UndoStack.clear();
            s_RedoStack.clear();
            s_Dirty = false;
        }

    private:
        static inline std::vector<std::unique_ptr<EditorCommand>> s_UndoStack;
        static inline std::vector<std::unique_ptr<EditorCommand>> s_RedoStack;
        static inline size_t s_MaxHistorySize = 100;
        static inline bool s_Dirty = false;
    };

}
