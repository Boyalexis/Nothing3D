#pragma once
#include "render/GuideGeometry.h"
#include <QImage>
#include <QPainter>
#include <QFont>
#include <array>

namespace ViewCube {
using Mesh=std::vector<BoxVertex>;
inline void triangle(Mesh& mesh,QVector3D a,QVector3D b,QVector3D c,QVector3D color) {
    for(auto p : {a,b,c}) mesh.push_back({{p.x(),p.y(),p.z()},{color.x(),color.y(),color.z()}});
}
inline void quad(Mesh& mesh,QVector3D center,QVector3D u,QVector3D v,QVector3D color) {
    triangle(mesh,center-u-v,center+u-v,center+u+v,color);
    triangle(mesh,center-u-v,center+u+v,center-u+v,color);
}
// Rasterize Qt font coverage once, then upload as static colored micro-quads.
// This keeps the overlay in Vulkan (including screenshots) without a texture atlas yet.
inline void text(Mesh& mesh,const QString& word,QVector3D center,QVector3D u,QVector3D v,
                 QVector3D ink,QVector3D background) {
    QImage image(192,48,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QFont font(QStringLiteral("Segoe UI")); font.setPixelSize(40); font.setWeight(QFont::DemiBold);
    painter.setFont(font); painter.setPen(Qt::white);
    painter.drawText(image.rect(),Qt::AlignCenter,word); painter.end();
    for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
        const float alpha=float(qAlpha(image.pixel(x,y)))/255;
        if(alpha<0.05f) continue;
        const auto p=center+u*(2*(float(x)+0.5f)/image.width()-1)+v*(1-2*(float(y)+0.5f)/image.height());
        quad(mesh,p,u/float(image.width()),v/float(image.height()),ink*alpha+background*(1-alpha));
    }
}
struct Face { QVector3D normal,u,v; const char* name; QVector3D color; };
inline const std::array<Face,6> faces{{
    {{0,0,1},{1,0,0},{0,1,0},"FRONT",{0.75f,0.82f,0.89f}},
    {{1,0,0},{0,0,-1},{0,1,0},"RIGHT",{0.66f,0.75f,0.84f}},
    {{0,0,-1},{-1,0,0},{0,1,0},"BACK",{0.73f,0.80f,0.88f}},
    {{-1,0,0},{0,0,1},{0,1,0},"LEFT",{0.70f,0.79f,0.87f}},
    {{0,1,0},{1,0,0},{0,0,-1},"TOP",{0.86f,0.90f,0.95f}},
    {{0,-1,0},{1,0,0},{0,0,1},"BOTTOM",{0.62f,0.71f,0.81f}}
}};
inline Mesh cube() {
    Mesh mesh;
    for(const auto& f : faces) {
        quad(mesh,f.normal*0.65f,f.u*0.65f,f.v*0.65f,f.color);
        // Narrow face outlines and labels stand slightly above the surface.
        const auto border=QVector3D(0.37f,0.48f,0.61f);
        for(float sign : {-1.0f,1.0f}) {
            quad(mesh,f.normal*0.651f+f.u*(sign*0.64f),f.u*0.009f,f.v*0.65f,border);
            quad(mesh,f.normal*0.651f+f.v*(sign*0.64f),f.u*0.65f,f.v*0.009f,border);
        }
        text(mesh,QString::fromLatin1(f.name),f.normal*0.654f,f.u*0.59f,f.v*0.22f,
             {0.20f,0.30f,0.43f},f.color);
    }
    // Compass ring in the horizontal plane, below the cube.
    for(int i=0;i<128;++i) {
        const float a=float(i)*6.2831853f/128, b=float(i+1)*6.2831853f/128;
        const QVector3D p{std::cos(a),0,std::sin(a)},q{std::cos(b),0,std::sin(b)},o{0,-0.86f,0};
        const QVector3D color{0.38f,0.52f,0.68f};
        triangle(mesh,o+p*1.08f,o+q*1.08f,o+q*1.18f,color);
        triangle(mesh,o+p*1.08f,o+q*1.18f,o+p*1.18f,color);
    }
    return mesh;
}
struct Labels { Mesh mesh; std::array<uint32_t,4> first{},count{}; };
inline Labels labels() {
    Labels result;
    const char* names[]={"E","S","W","N"};
    for(size_t i=0;i<4;++i) {
        result.first[i]=uint32_t(result.mesh.size());
        text(result.mesh,QString::fromLatin1(names[i]),{}, {0.42f,0,0},{0,-0.11f,0},
             {0.20f,0.30f,0.43f},{0.94f,0.96f,0.98f});
        result.count[i]=uint32_t(result.mesh.size())-result.first[i];
    }
    return result;
}
inline Mesh home() {
    Mesh mesh;
    const QVector3D color{0.20f,0.30f,0.43f};
    triangle(mesh,{-0.085f,0,0},{0,-0.075f,0},{0.085f,0,0},color);
    quad(mesh,{-0.035f,0.035f,0},{0.022f,0,0},{0,0.04f,0},color);
    quad(mesh,{0.035f,0.035f,0},{0.022f,0,0},{0,0.04f,0},color);
    quad(mesh,{0,0.005f,0},{0.055f,0,0},{0,0.018f,0},color);
    return mesh;
}
inline const QVector3D cardinalPositions[]={{1.4f,-0.86f,0},{0,-0.86f,1.4f},{-1.4f,-0.86f,0},{0,-0.86f,-1.4f}};
}
