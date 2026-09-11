#pragma once
#include "render/BoxGeometry.h"
#include "render/CylinderGeometry.h"
#include <QImage>
#include <QMatrix4x4>
#include <QRect>
#include <cmath>
#include <QDebug>
#include <span>

// Independent CPU raster reference: test the nearest face, not just non-empty pixels.
inline bool hasSceneMesh(const QImage& image, const QMatrix4x4& mvp, const QRect& overlay,
                         std::span<const BoxVertex> mesh, int minimumSamples = 12,
                         int minimumCylinderSamples = 0, int minimumOverlaps = 0)
{
    if (image.width()<100 || image.height()<100) return false;
    int cylinderSamples=0;
    int boxSamples=0, backgroundSamples=0, overlaps=0;
    // The solid reference does not contain guide lines. Reserve a thin band
    // around the projected world Y axis (also unchanged by preview Y rotation).
    // Separate guide-image checks verify its colour and visibility.
    const auto axis0=mvp*QVector4D(0,0,0,1), axis1=mvp*QVector4D(0,1,0,1);
    const bool axisVisible=axis0.w()>0 && axis1.w()>0;
    const QPointF axisStart((axis0.x()/axis0.w()+1)*image.width()/2,(axis0.y()/axis0.w()+1)*image.height()/2);
    const QPointF axisEnd((axis1.x()/axis1.w()+1)*image.width()/2,(axis1.y()/axis1.w()+1)*image.height()/2);
    const auto direction=axisEnd-axisStart;
    const auto lengthSquared=QPointF::dotProduct(direction,direction);
    // Denser coverage keeps the existing minimum counts meaningful in the
    // shorter viewport above the command panel, without lowering thresholds.
    constexpr int steps=32;
    for (int row=1; row<steps; ++row) for (int col=1; col<steps; ++col) {
        const int px=image.width()*col/steps, py=image.height()*row/steps;
        if (overlay.contains(px,py)) continue;
        if (axisVisible && lengthSquared>1) {
            const auto offset=QPointF(px+.5,py+.5)-axisStart;
            const double cross=offset.x()*direction.y()-offset.y()*direction.x();
            if (cross*cross<2.25*lengthSquared) continue;
        }
        const float x=2*(float(px)+0.5f)/float(image.width())-1;
        const float y=2*(float(py)+0.5f)/float(image.height())-1;
        float nearest=2, edge=0;
        int winner=-1, hits=0;
        for (int tri=0; tri<int(mesh.size()/3); ++tri) {
            QVector3D v[3];
            bool behind=false;
            for (int k=0; k<3; ++k) {
                const auto& p=mesh[tri*3+k].position;
                const auto clip=mvp*QVector4D(p[0],p[1],p[2],1);
                if (clip.w()<=0) { behind=true; break; }
                v[k]=clip.toVector3D()/clip.w();
            }
            if (behind) continue;
            const float denominator=(v[1].y()-v[2].y())*(v[0].x()-v[2].x())+(v[2].x()-v[1].x())*(v[0].y()-v[2].y());
            if (std::abs(denominator)<1e-7f) continue;
            const float a=((v[1].y()-v[2].y())*(x-v[2].x())+(v[2].x()-v[1].x())*(y-v[2].y()))/denominator;
            const float b=((v[2].y()-v[0].y())*(x-v[2].x())+(v[0].x()-v[2].x())*(y-v[2].y()))/denominator;
            const float c=1-a-b;
            if (a<0 || b<0 || c<0) continue;
            const float z=a*v[0].z()+b*v[1].z()+c*v[2].z();
            if (z<0 || z>1) continue;
            ++hits;
            if (z<nearest) { nearest=z; winner=tri; edge=std::min(a,std::min(b,c)); }
        }
        const auto actual=image.pixelColor(px,py);
        if (winner>=0) {
            if (edge<0.06f) continue; // Exclude rasterization boundary rules.
            ++boxSamples;
            if(winner>=12) ++cylinderSamples;
            if (hits>1) ++overlaps;
            const auto& expected=mesh[winner*3].color;
            if (std::abs(actual.red()-int(expected[0]*255))>5
                || std::abs(actual.green()-int(expected[1]*255))>5
                || std::abs(actual.blue()-int(expected[2]*255))>5) {
                qInfo("Box mismatch at %d,%d: %d %d %d",px,py,actual.red(),actual.green(),actual.blue()); return false;
            }
        } else {
            if (std::abs(actual.red()-240)<=3 && std::abs(actual.green()-245)<=3 && std::abs(actual.blue()-250)<=3) {
                ++backgroundSamples;
            } else {
                // Shader first mixes grid/axis colors, then blends over the clear color.
                // Check the resulting color triangle instead of a single palette segment.
                const QVector3D background(0.94f*255,0.96f*255,0.98f*255);
                const QVector3D grid=QVector3D(0.42f,0.49f,0.58f)*255-background;
                const QVector3D actualDelta=QVector3D(actual.red(),actual.green(),actual.blue())-background;
                const QVector3D axes[]={{0.95f,0.22f,0.22f},{0.25f,0.85f,0.35f},{0.25f,0.55f,1.0f}};
                bool guide=false;
                for(const auto& color : axes) {
                    const auto axis=color*255-background;
                    const float gg=QVector3D::dotProduct(grid,grid),aa=QVector3D::dotProduct(axis,axis),ga=QVector3D::dotProduct(grid,axis);
                    const float gd=QVector3D::dotProduct(grid,actualDelta),ad=QVector3D::dotProduct(axis,actualDelta);
                    const float determinant=gg*aa-ga*ga;
                    const float u=(gd*aa-ad*ga)/determinant,v=(ad*gg-gd*ga)/determinant;
                    guide |= u>=-0.04f && v>=-0.04f && u+v<=1.04f && (actualDelta-grid*u-axis*v).length()<5;
                }
                if (!guide) { qInfo("Background mismatch at %d,%d: %d %d %d",px,py,actual.red(),actual.green(),actual.blue()); return false; }
            }
        }
    }
    const bool passed=boxSamples>=minimumSamples && cylinderSamples>=minimumCylinderSamples
        && backgroundSamples>=40 && overlaps>=minimumOverlaps;
    if(!passed) qInfo("Box counts: samples=%d background=%d overlaps=%d",boxSamples,backgroundSamples,overlaps);
    return passed;
}

inline bool hasBox(const QImage& image, const QMatrix4x4& mvp, const QRect& overlay = {}) {
    const auto mesh = demoVertices();
    return hasSceneMesh(image, mvp, overlay, mesh, 12, 3, 8);
}
