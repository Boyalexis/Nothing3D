#pragma once
#include "render/ViewCubeGeometry.h"
#include <QImage>
#include <cmath>

inline bool hasInfiniteGrid(const QImage& image,const QRect& corner) {
    int samples=0;
    for(int y=image.height()/2;y<image.height();y+=2) for(int x=0;x<image.width();x+=2) {
        if(corner.contains(x,y)) continue;
        const auto c=image.pixelColor(x,y);
        // Neutral blue-grey grid samples, excluding box and colored world axes.
        if(c.red()>160 && c.red()<228 && c.green()>c.red()+3
            && c.blue()>c.green()+3 && c.blue()<244) ++samples;
    }
    return samples>100;
}
inline bool hasGuides(const QImage& image, const QRect& corner, const QMatrix4x4& orientation) {
    if(image.isNull() || corner.width()<40 || !hasInfiniteGrid(image,corner)) return false;
    auto matches=[](QColor c,QVector3D expected) {
        return std::abs(c.red()-int(expected.x()*255))<=3
            && std::abs(c.green()-int(expected.y()*255))<=3
            && std::abs(c.blue()-int(expected.z()*255))<=3;
    };
    // Visible cube faces must be present with their distinct shaded colors.
    for(const auto& face : ViewCube::faces) {
        if(orientation.mapVector(face.normal).z()>-0.015f) continue;
        int pixels=0;
        for(int y=corner.top();y<=corner.bottom();++y) for(int x=corner.left();x<=corner.right();++x)
            pixels+=matches(image.pixelColor(x,y),face.color);
        if(pixels<30) return false;
    }
    int ring=0, home=0, ink=0;
    for(int y=corner.top();y<=corner.bottom();++y) for(int x=corner.left();x<=corner.right();++x) {
        const auto c=image.pixelColor(x,y);
        ring+=matches(c,{0.38f,0.52f,0.68f});
        ink+=matches(c,{0.20f,0.30f,0.43f});
        if(x>corner.x()+corner.width()*0.8 && y<corner.y()+corner.height()*0.2)
            home+=matches(c,{0.20f,0.30f,0.43f});
    }
    return ring>25 && home>8 && ink>10;
}
