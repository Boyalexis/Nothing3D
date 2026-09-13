#pragma once
#include "render/SceneGeometry.h"
#include <QPointF>
#include <QSizeF>
#include <optional>
#include <algorithm>

namespace ScenePicking {
// Both unit primitives fit this local box. This is only a conservative rejection;
// cylinder corners and the final nearest surface still use exact mesh triangles.
inline bool intersectsBounds(QVector3D origin,QVector3D direction) {
    double enter=0,leave=1;
    for(int axis=0;axis<3;++axis) {
        const double low=(axis==1 ? 0.0 : -.5)-1e-5;
        const double high=(axis==1 ? 1.0 : .5)+1e-5;
        if(direction[axis]==0) {
            if(origin[axis]<low || origin[axis]>high) return false;
            continue;
        }
        double a=(low-origin[axis])/direction[axis], b=(high-origin[axis])/direction[axis];
        if(a>b) std::swap(a,b);
        enter=std::max(enter,a); leave=std::min(leave,b);
        if(enter>leave) return false;
    }
    return true;
}
// Segment parameter is preserved through each object's inverse transform.
// This makes hits comparable even when objects have different sizes.
inline std::optional<float> triangleHit(QVector3D origin, QVector3D direction,
                                       QVector3D a, QVector3D b, QVector3D c) {
    const auto e1 = b-a, e2 = c-a;
    const auto p = QVector3D::crossProduct(direction, e2);
    const float determinant = QVector3D::dotProduct(e1, p);
    if (std::abs(determinant) < 1e-8f) return {};
    const auto s = origin-a;
    const float u = QVector3D::dotProduct(s, p)/determinant;
    if (u < 0 || u > 1) return {};
    const auto q = QVector3D::crossProduct(s, e1);
    const float v = QVector3D::dotProduct(direction, q)/determinant;
    if (v < 0 || u+v > 1) return {};
    const float t = QVector3D::dotProduct(e2, q)/determinant;
    if (t < 0 || t > 1 || !std::isfinite(t)) return {};
    return t;
}

inline n3d::ObjectId pick(const n3d::Scene& scene, const QMatrix4x4& viewProjection,
                          QPointF position, QSizeF viewportSize) {
    if (viewportSize.width() <= 0 || viewportSize.height() <= 0
        || !std::isfinite(position.x()) || !std::isfinite(position.y())
        || position.x() < 0 || position.y() < 0
        || position.x() >= viewportSize.width() || position.y() >= viewportSize.height()) return 0;
    bool invertible = false;
    const auto inverse = viewProjection.inverted(&invertible);
    if (!invertible) return 0;
    // Vulkan viewport: top-left origin, depth in [0,1]. Logical coordinates
    // divided by logical size also work with fractional display scaling.
    const float x = float(2*position.x()/viewportSize.width()-1);
    const float y = float(2*position.y()/viewportSize.height()-1);
    const auto nearClip = inverse * QVector4D(x,y,0,1);
    const auto farClip = inverse * QVector4D(x,y,1,1);
    if (nearClip.w() == 0 || farClip.w() == 0) return 0;
    const auto nearPoint = nearClip.toVector3DAffine(), farPoint = farClip.toVector3DAffine();
    static const auto vertices = SceneGeometry::vertices();
    float nearest = 2;
    n3d::ObjectId selected = 0;
    for (const auto& object : scene.objects()) {
        const auto render = SceneGeometry::prepare(object);
        const auto local = render.model.inverted(&invertible);
        if (!invertible) continue;
        const auto origin = local.map(nearPoint);
        const auto direction = local.map(farPoint)-origin; // Do not normalize.
        if(!intersectsBounds(origin,direction)) continue;
        const bool box = render.primitive == SceneGeometry::Primitive::Box;
        const auto first = box ? 0u : SceneGeometry::boxCount;
        const auto count = box ? SceneGeometry::boxCount : SceneGeometry::cylinderCount;
        for (uint32_t i=first; i<first+count; i+=3) {
            auto point = [&](uint32_t index) {
                const auto& p = vertices[index].position;
                return QVector3D(p[0],p[1],p[2]);
            };
            if (const auto t = triangleHit(origin, direction, point(i), point(i+1), point(i+2)); t && *t < nearest) {
                nearest = *t;
                selected = object.id;
            }
        }
    }
    return selected;
}
} // namespace ScenePicking
