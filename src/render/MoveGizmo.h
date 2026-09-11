#pragma once
#include "scene/Scene.h"
#include "render/BoxGeometry.h"
#include <QMatrix4x4>
#include <QPointF>
#include <QSizeF>
#include <optional>
#include <cmath>
#include <vector>
#include <algorithm>

// World-axis translation. All hit distances use logical pixels (including HiDPI).
namespace MoveGizmo {
inline QVector3D axis(int i) { QVector3D a; a[i] = 1; return a; }
inline QVector3D metres(n3d::Vector3 p) { return {p.x/1000,p.y/1000,p.z/1000}; }
inline std::optional<QPointF> project(const QMatrix4x4& vp, QVector3D p, QSizeF size) {
    const auto clip = vp*QVector4D(p,1);
    if (size.isEmpty() || !std::isfinite(clip.w()) || clip.w() <= 1e-6f
        || clip.z() < 0 || clip.z() > clip.w()) return {};
    const QPointF result((clip.x()/clip.w()+1)*size.width()/2,(clip.y()/clip.w()+1)*size.height()/2);
    if (!std::isfinite(result.x()) || !std::isfinite(result.y())) return {};
    return result;
}
struct Layout {
    QMatrix4x4 model;
    QPointF origin, tips[3];
    bool visible[3]{};
};
inline std::optional<Layout> layout(const QMatrix4x4& vp, n3d::Vector3 position, QSizeF size) {
    const auto centre = metres(position);
    const auto origin = project(vp,centre,size);
    if (!origin || origin->x()<0 || origin->y()<0 || origin->x()>size.width() || origin->y()>size.height()) return {};
    const float focal = QVector3D(vp(1,0),vp(1,1),vp(1,2)).length();
    if (focal < 1e-6f) return {};
    const float scale = float(192.0/size.height())*(vp*QVector4D(centre,1)).w()/focal;
    Layout result; result.origin = *origin;
    result.model.translate(centre); result.model.scale(scale);
    for (int i=0;i<3;++i) if (auto tip=project(vp,centre+axis(i)*scale,size)) {
        result.tips[i] = *tip;
        const auto d = *tip-*origin;
        // A nearly end-on axis cannot offer a useful drag direction.
        result.visible[i] = QPointF::dotProduct(d,d) >= 14*14;
    }
    return result;
}
inline int hit(const Layout& layout, QPointF mouse) {
    int picked = -1; double closest = 9*9;
    for (int i=0;i<3;++i) if (layout.visible[i]) {
        const auto d = layout.tips[i]-layout.origin;
        const double t = QPointF::dotProduct(mouse-layout.origin,d)/QPointF::dotProduct(d,d);
        // Reserve the shared centre for ordinary object selection.
        if (t < .18 || t > 1.08) continue;
        const auto delta = mouse-(layout.origin+d*std::clamp(t,0.0,1.0));
        const double distance = QPointF::dotProduct(delta,delta);
        if (distance < closest) { closest = distance; picked = i; }
    }
    return picked;
}

struct Ray { QVector3D origin, direction; };
inline std::optional<Ray> ray(const QMatrix4x4& inverse, QPointF mouse, QSizeF size) {
    if (size.isEmpty() || !std::isfinite(mouse.x()) || !std::isfinite(mouse.y())) return {};
    const float x = float(2*mouse.x()/size.width()-1), y = float(2*mouse.y()/size.height()-1);
    const auto near = inverse*QVector4D(x,y,0,1), far = inverse*QVector4D(x,y,1,1);
    if (std::abs(near.w())<1e-7f || std::abs(far.w())<1e-7f) return {};
    const auto origin=near.toVector3DAffine(), direction=far.toVector3DAffine()-origin;
    return Ray{origin,direction};
}
struct Drag {
    QMatrix4x4 inverse;
    QSizeF size;
    n3d::Vector3 original;
    QVector3D normal;
    int axisIndex = 0;
    float start = 0;
    std::optional<float> coordinate(QPointF mouse) const {
        const auto r = ray(inverse,mouse,size);
        if (!r) return {};
        const float denominator = QVector3D::dotProduct(r->direction,normal);
        if (std::abs(denominator)<1e-6f) return {};
        const float t = QVector3D::dotProduct(metres(original)-r->origin,normal)/denominator;
        if (!std::isfinite(t) || t<0 || t>1) return {};
        return (r->origin+r->direction*t-metres(original))[axisIndex];
    }
    std::optional<n3d::Vector3> position(QPointF mouse, bool snap) const {
        const auto value = coordinate(mouse);
        if (!value) return {};
        auto result = original;
        float* component = axisIndex==0 ? &result.x : axisIndex==1 ? &result.y : &result.z;
        double mm = double(*component)+double(*value-start)*1000;
        if (snap) mm = std::round(mm/100)*100;
        if (!std::isfinite(mm) || std::abs(mm)>1e9) return {};
        *component = float(mm);
        return result;
    }
};
inline std::optional<Drag> begin(const QMatrix4x4& vp, QSizeF size, n3d::Vector3 position, int index, QPointF mouse) {
    if (index<0 || index>2) return {};
    bool invertible=false;
    const auto inverse=vp.inverted(&invertible);
    const auto origin=project(vp,metres(position),size);
    if (!invertible || !origin) return {};
    const auto r=ray(inverse,*origin,size);
    if (!r) return {};
    const auto direction=r->direction.normalized();
    const auto normal=direction-axis(index)*direction[index];
    if (normal.lengthSquared()<.0025f) return {};
    Drag result{inverse,size,position,normal.normalized(),index,0};
    const auto start=result.coordinate(mouse);
    if (!start) return {};
    result.start=*start;
    return result;
}

// Eight-sided shaft and cone. Immutable vertices, three contiguous draw ranges.
inline constexpr uint32_t verticesPerAxis = 8*12;
inline std::vector<BoxVertex> vertices() {
    std::vector<BoxVertex> result;
    constexpr float colors[3][3]={{1,.22f,.22f},{.25f,1,.38f},{.28f,.58f,1}};
    for (int a=0;a<3;++a) {
        const auto point=[a](float length,float radius,float angle) {
            QVector3D p; p[a]=length; p[(a+1)%3]=radius*std::cos(angle); p[(a+2)%3]=radius*std::sin(angle); return p;
        };
        const auto triangle=[&](QVector3D p,QVector3D q,QVector3D r) {
            for (const auto v : {p,q,r}) result.push_back({{v.x(),v.y(),v.z()},{colors[a][0],colors[a][1],colors[a][2]}});
        };
        for (int i=0;i<8;++i) {
            const float t=float(i)*6.2831853f/8, u=float(i+1)*6.2831853f/8;
            const auto p=point(.16f,.018f,t), q=point(.16f,.018f,u), r=point(.78f,.018f,t), s=point(.78f,.018f,u);
            triangle(p,q,r); triangle(q,s,r);
            triangle(point(.76f,.065f,t),point(.76f,.065f,u),axis(a));
            triangle(point(.76f,.065f,u),point(.76f,.065f,t),axis(a)*.76f);
        }
    }
    return result;
}
}
