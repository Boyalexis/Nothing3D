#pragma once
#include "scene/Scene.h"
#include <optional>

namespace n3d {
// One committed operation stores only its changed object, not the entire scene.
// Camera, hover, uncommitted input and placement previews are not document edits.
struct SceneChange {
    std::string label;
    ObjectId id=0;
    std::optional<ObjectData> before, after;
    size_t index=0;
    ObjectId selectionBefore=0, selectionAfter=0;
};
class SceneHistory {
public:
    bool canUndo() const { return cursor_>0; }
    bool canRedo() const { return cursor_<changes_.size(); }
    size_t size() const { return changes_.size(); }
    size_t cursor() const { return cursor_; }
    std::string undoLabel() const { return canUndo()?changes_[cursor_-1].label:std::string{}; }
    std::string redoLabel() const { return canRedo()?changes_[cursor_].label:std::string{}; }
    bool isClean() const { return cleanCursor_ && *cleanCursor_==cursor_; }
    void markClean() { cleanCursor_=cursor_; }
    void clear() { changes_.clear(); cursor_=0; cleanCursor_=0; }
    // Call after a successful edit. No-op/cancelled gestures preserve redo.
    void recordApplied(SceneChange change) {
        if (change.before==change.after) return;
        if (!change.id) throw std::invalid_argument("History requires an object ID");
        if (cleanCursor_ && *cleanCursor_>cursor_) cleanCursor_.reset();
        changes_.erase(changes_.begin()+std::ptrdiff_t(cursor_),changes_.end());
        changes_.push_back(std::move(change));
        cursor_=changes_.size();
    }
    std::optional<ObjectId> undo(Scene& scene) {
        if (!canUndo()) return {};
        const auto& change=changes_[cursor_-1];
        apply(scene,change,change.after,change.before);
        --cursor_;
        return change.selectionBefore;
    }
    std::optional<ObjectId> redo(Scene& scene) {
        if (!canRedo()) return {};
        const auto& change=changes_[cursor_];
        apply(scene,change,change.before,change.after);
        ++cursor_;
        return change.selectionAfter;
    }
private:
    static void apply(Scene& scene,const SceneChange& change,const std::optional<ObjectData>& expected,
                      const std::optional<ObjectData>& target) {
        const auto* current=scene.find(change.id);
        if (bool(current)!=expected.has_value() || (current && current->data!=*expected))
            throw std::logic_error("History and scene diverged");
        if (!target) scene.remove(change.id);
        else if (current) scene.update(change.id,*target);
        else scene.restore({change.id,*target},change.index);
    }
    std::vector<SceneChange> changes_;
    size_t cursor_=0;
    std::optional<size_t> cleanCursor_=0;
};
}
