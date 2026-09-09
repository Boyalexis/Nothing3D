#pragma once
#include "scene/Scene.h"
#include <QMatrix4x4>
#include <QPointF>
#include <QSizeF>
#include <optional>

namespace GroundPlacement {
// Intersect the visible near-to-far segment with the world Y=0 plane.
inline std::optional<n3d::Vector3> point(const QMatrix4x4& viewProjection,
                                        QPointF mouse, QSizeF size, bool snap) {
    if (size.width() <= 0 || size.height() <= 0 || !std::isfinite(mouse.x()) || !std::isfinite(mouse.y())
        || mouse.x() < 0 || mouse.y() < 0 || mouse.x() >= size.width() || mouse.y() >= size.height()) return {};
    bool invertible = false;
    const auto inverse = viewProjection.inverted(&invertible);
    if (!invertible) return {};
    const float x = float(mouse.x()/size.width()*2-1), y = float(mouse.y()/size.height()*2-1);
    const auto a = inverse*QVector4D(x,y,0,1), b = inverse*QVector4D(x,y,1,1);
    if (a.w() == 0 || b.w() == 0) return {};
    const auto nearPoint = a.toVector3DAffine(), farPoint = b.toVector3DAffine();
    const auto direction = farPoint-nearPoint;
    if (std::abs(direction.y()) < 1e-7f) return {};
    const double t = -double(nearPoint.y())/direction.y();
    if (!std::isfinite(t) || t < 0 || t > 1) return {};
    double px = (nearPoint.x()+t*direction.x())*1000;
    double pz = (nearPoint.z()+t*direction.z())*1000;
    if (snap) { px = std::round(px/100)*100; pz = std::round(pz/100)*100; }
    if (!std::isfinite(px) || !std::isfinite(pz) || std::abs(px) > 1e9 || std::abs(pz) > 1e9) return {};
    return n3d::Vector3{float(px),0,float(pz)};
}
} // namespace GroundPlacement
