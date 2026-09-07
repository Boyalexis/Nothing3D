#pragma once
#include "render/BoxGeometry.h"
#include <QMatrix4x4>
#include <QRect>
#include <vector>
#include <algorithm>

namespace Guides {
inline const QVector3D red{0.95f,0.22f,0.22f};
inline const QVector3D green{0.25f,0.85f,0.35f};
inline const QVector3D blue{0.25f,0.55f,1.0f};
inline void line(std::vector<BoxVertex>& mesh, QVector3D a, QVector3D b, QVector3D color) {
    for (const auto p : {a,b}) mesh.push_back({{p.x(),p.y(),p.z()}, {color.x(),color.y(),color.z()}});
}
inline std::vector<BoxVertex> ground() {
    std::vector<BoxVertex> mesh;
    // Metres internally: 100 mm cells, 1 m major lines, 10 m total extent.
    for (int i=-50; i<=50; ++i) {
        if (i==0) continue; // World axes replace the two origin grid lines.
        const float p=float(i)*0.1f;
        const QVector3D color = i%10==0 ? QVector3D(0.25f,0.34f,0.44f) : QVector3D(0.13f,0.21f,0.30f);
        // Slightly below the floor prevents coplanar fighting with box bottoms.
        line(mesh,{p,-0.002f,-5},{p,-0.002f,5},color);
        line(mesh,{-5,-0.002f,p},{5,-0.002f,p},color);
    }
    line(mesh,{-5,0,0},{5,0,0},red);
    line(mesh,{0,0,0},{0,3,0},green);
    line(mesh,{0,0,-5},{0,0,5},blue);
    return mesh;
}
inline std::vector<BoxVertex> compass() {
    std::vector<BoxVertex> mesh;
    const QVector3D directions[]={{1,0,0},{0,1,0},{0,0,1}};
    const QVector3D colors[]={red,green,blue};
    for(int i=0;i<3;++i) {
        const auto d=directions[i];
        const auto side=directions[(i+1)%3]*0.08f;
        line(mesh,{},d,colors[i]);
        line(mesh,d,d*0.80f+side,colors[i]);
        line(mesh,d,d*0.80f-side,colors[i]);
    }
    return mesh;
}
inline std::vector<BoxVertex> letters() {
    std::vector<BoxVertex> mesh;
    // Six vertices per glyph, drawn individually in screen space.
    line(mesh,{-0.5f,-0.6f,0},{0.5f,0.6f,0},red);
    line(mesh,{-0.5f,0.6f,0},{0.5f,-0.6f,0},red);
    line(mesh,{0,0,0},{0,0,0},red);
    line(mesh,{-0.5f,-0.6f,0},{0,0,0},green);
    line(mesh,{0.5f,-0.6f,0},{0,0,0},green);
    line(mesh,{0,0,0},{0,0.6f,0},green);
    line(mesh,{-0.5f,-0.6f,0},{0.5f,-0.6f,0},blue);
    line(mesh,{0.5f,-0.6f,0},{-0.5f,0.6f,0},blue);
    line(mesh,{-0.5f,0.6f,0},{0.5f,0.6f,0},blue);
    return mesh;
}
inline QRect compassRect(QSize size, qreal pixelRatio) {
    const int margin=std::min(int(16*pixelRatio),std::min(size.width(),size.height())/8);
    const int side=std::max(1,std::min({int(180*pixelRatio),size.width()-2*margin,size.height()-2*margin}));
    return {size.width()-margin-side,margin,side,side};
}
inline QMatrix4x4 compassMatrix(QMatrix4x4 view, const QMatrix4x4& correction) {
    view.setColumn(3,QVector4D(0,0,-3,1)); // Ignore camera pan and distance.
    QMatrix4x4 projection;
    projection.ortho(-1.8f,1.8f,-1.8f,1.8f,0.1f,10.0f);
    return correction*projection*view;
}
}
