#pragma once
#include "render/SceneGeometry.h"
#include "render/OrbitCamera.h"
#include <limits>

// Fit a conservative world-space bounding sphere. Rotated/scaled primitive
// corners are included, so this also works for arbitrary object orientations.
inline bool frameScene(OrbitCamera& camera,const n3d::Scene& scene,float aspect) {
    if(scene.objects().empty()) return false;
    QVector3D low(std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max());
    QVector3D high=-low;
    for(const auto& object:scene.objects()) {
        const auto model=SceneGeometry::prepare(object).model;
        for(float x:{-.5f,.5f}) for(float y:{0.0f,1.0f}) for(float z:{-.5f,.5f}) {
            const auto p=model.map(QVector3D(x,y,z));
            for(int axis=0;axis<3;++axis) { low[axis]=std::min(low[axis],p[axis]); high[axis]=std::max(high[axis],p[axis]); }
        }
    }
    camera.target=(low+high)*.5f;
    const float radius=std::max((high-low).length()*.5f,1e-5f);
    const float tangent=std::tan(qDegreesToRadians(22.5f))*std::min(std::max(aspect,.01f),1.0f);
    camera.distance=radius*1.12f/(camera.orthographic ? tangent : std::sin(std::atan(tangent)));
    camera.framingRadius=radius;
    camera.minimumDistance=std::min(2.5f,camera.distance*.05f);
    camera.maximumDistance=std::max(30.0f,camera.distance*10);
    return true;
}
