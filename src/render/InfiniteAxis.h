#pragma once
#include <QMatrix4x4>
#include <optional>
#include <limits>
#include <algorithm>
#include <cmath>

// Clip the world Y line (0,t,0) against Vulkan's homogeneous view volume.
// This avoids both arbitrary world lengths and endpoints behind the camera.
inline std::optional<QMatrix4x4> infiniteYAxis(const QMatrix4x4& vp) {
    const auto origin=vp*QVector4D(0,0,0,1);
    const auto direction=vp*QVector4D(0,1,0,0);
    const double a[]={origin.w()+origin.x(),origin.w()-origin.x(),origin.w()+origin.y(),
        origin.w()-origin.y(),origin.z(),origin.w()-origin.z()};
    const double b[]={direction.w()+direction.x(),direction.w()-direction.x(),direction.w()+direction.y(),
        direction.w()-direction.y(),direction.z(),direction.w()-direction.z()};
    double low=-std::numeric_limits<double>::infinity(),high=-low;
    for(int i=0;i<6;++i) {
        if(std::abs(b[i])<1e-10) { if(a[i]<0) return {}; }
        else if(b[i]>0) low=std::max(low,-a[i]/b[i]);
        else high=std::min(high,-a[i]/b[i]);
    }
    if(low>=high || !std::isfinite(low) || !std::isfinite(high)) return {};
    const auto first=origin+direction*float(low),last=origin+direction*float(high);
    if(first.w()<=0 || last.w()<=0) return {};
    const auto p=first.toVector3D()/first.w(),q=last.toVector3D()/last.w();
    if((p-q).lengthSquared()<1e-12f) return {};
    QMatrix4x4 transform;
    transform.setColumn(1,QVector4D(q-p,0));
    transform.setColumn(3,QVector4D(p,1));
    return transform;
}
