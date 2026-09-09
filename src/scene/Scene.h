#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace n3d {
using ObjectId = std::uint64_t;
struct Vector3 {
    float x = 0, y = 0, z = 0;
    bool operator==(const Vector3&) const = default;
};
struct BoxParameters {
    float width = 2000, height = 1500, depth = 1000;
    bool operator==(const BoxParameters&) const = default;
};
struct CylinderParameters {
    float diameter = 800, height = 1400;
    bool operator==(const CylinderParameters&) const = default;
};
// Business data: millimetres, degrees, Y-up, origin at the bottom centre.
// No QWidget, QObject, Vulkan handles or render-unit conversions belong here.
struct ObjectData {
    std::string name;
    std::variant<BoxParameters, CylinderParameters> shape = BoxParameters{};
    Vector3 positionMm;
    Vector3 rotationDegrees;
    bool operator==(const ObjectData&) const = default;
};
struct SceneObject {
    ObjectId id;
    ObjectData data;
};

class Scene {
public:
    const std::vector<SceneObject>& objects() const { return objects_; }
    const SceneObject* find(ObjectId id) const {
        const auto it = std::find_if(objects_.begin(), objects_.end(),
            [id](const auto& object) { return object.id == id; });
        return it == objects_.end() ? nullptr : &*it;
    }
    ObjectId add(ObjectData data) {
        validate(data);
        const auto id = nextId_;
        objects_.push_back({id, std::move(data)});
        ++nextId_;
        return id;
    }
    bool update(ObjectId id, ObjectData data) {
        for (auto& object : objects_) if (object.id == id) {
            validate(data); // Invalid edits leave the old object intact.
            object.data = std::move(data);
            return true;
        }
        return false;
    }
    bool remove(ObjectId id) {
        return std::erase_if(objects_, [id](const auto& object) { return object.id == id; }) != 0;
    }
    void clear() { objects_.clear(); } // Never recycle IDs within this scene.

private:
    static void validate(const ObjectData& data) {
        const auto dimension = [](float value) {
            return std::isfinite(value) && value > 0 && value <= 1e7f;
        };
        const bool shapeOkay = std::visit([&](const auto& shape) {
            if constexpr (requires { shape.width; })
                return dimension(shape.width) && dimension(shape.height) && dimension(shape.depth);
            else return dimension(shape.diameter) && dimension(shape.height);
        }, data.shape);
        const auto finite = [](Vector3 v) {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
        const auto p = data.positionMm;
        if (!shapeOkay || !finite(p) || !finite(data.rotationDegrees)
            || std::abs(p.x) > 1e9f || std::abs(p.y) > 1e9f || std::abs(p.z) > 1e9f)
            throw std::invalid_argument("Invalid scene dimensions or transform");
    }
    std::vector<SceneObject> objects_;
    ObjectId nextId_ = 1;
};
} // namespace n3d
