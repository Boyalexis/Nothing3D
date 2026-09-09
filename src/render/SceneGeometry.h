#pragma once
#include "scene/Scene.h"
#include "render/CylinderGeometry.h"
#include <QMatrix4x4>

namespace SceneGeometry {
enum class Primitive { Box, Cylinder };
struct RenderObject {
    n3d::ObjectId id;
    Primitive primitive;
    QMatrix4x4 model;
};
inline constexpr uint32_t boxCount = 36;
inline constexpr uint32_t cylinderCount = 64 * 12;

// Immutable unit primitives shared by every object for the device lifetime.
inline std::vector<BoxVertex> vertices() {
    const auto box = boxVertices(1, 1, 1);
    std::vector<BoxVertex> result(box.begin(), box.end());
    const auto cylinder = cylinderVertices(1, 1);
    result.insert(result.end(), cylinder.begin(), cylinder.end());
    return result;
}

inline std::vector<BoxVertex> wireVertices() {
    const auto triangles = vertices();
    std::vector<BoxVertex> result;
    result.reserve(triangles.size()*2);
    for (size_t i=0; i<triangles.size(); i+=3) for (int edge=0; edge<3; ++edge) {
        for (const auto index : {i+edge,i+(edge+1)%3}) {
            auto vertex = triangles[index];
            vertex.color[0] = 1; vertex.color[1] = 0.8f; vertex.color[2] = 0.15f;
            result.push_back(vertex);
        }
    }
    return result;
}

inline RenderObject prepare(const n3d::SceneObject& object) {
    const auto& data = object.data;
    RenderObject result{object.id, Primitive::Box, {}};
    auto& model = result.model;
    constexpr float mmPerMetre = 1000.0f;
    model.translate(data.positionMm.x / mmPerMetre, data.positionMm.y / mmPerMetre,
                    data.positionMm.z / mmPerMetre);
    // Column vectors: local scale, then X/Y/Z rotations, then world translation.
    model.rotate(std::fmod(data.rotationDegrees.z, 360.0f), 0, 0, 1);
    model.rotate(std::fmod(data.rotationDegrees.y, 360.0f), 0, 1, 0);
    model.rotate(std::fmod(data.rotationDegrees.x, 360.0f), 1, 0, 0);
    if (const auto* box = std::get_if<n3d::BoxParameters>(&data.shape)) {
        model.scale(box->width / mmPerMetre, box->height / mmPerMetre, box->depth / mmPerMetre);
    } else {
        const auto& cylinder = std::get<n3d::CylinderParameters>(data.shape);
        result.primitive = Primitive::Cylinder;
        model.scale(cylinder.diameter / mmPerMetre, cylinder.height / mmPerMetre,
                    cylinder.diameter / mmPerMetre);
    }
    return result;
}
} // namespace SceneGeometry
