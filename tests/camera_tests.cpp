#include "render/OrbitCamera.h"
#include "render/BoxGeometry.h"
#include "render/GuideGeometry.h"
#include "render/InfiniteAxis.h"
#include "render/CylinderGeometry.h"
#include <cstdio>

int main() {
    int failures=0;
    auto check=[&](bool ok, const char* name) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",name); ++failures; } };
    OrbitCamera camera;
    check(std::abs(camera.direction().length()-1)<0.0001f,"camera unit direction");
    auto target=camera.view()*QVector4D(camera.target,1);
    check(std::abs(target.x())<0.0001f && std::abs(target.y())<0.0001f && std::abs(target.z()+camera.distance)<0.0001f,"view looks at target");
    camera.orbit(100000,100000);
    check(camera.pitch==85 && std::abs(camera.yaw)<=180,"orbit bounded");
    camera.zoom(10000); check(camera.distance==2.5f,"near zoom limit");
    camera.zoom(-10000); check(camera.distance==30,"far zoom limit");
    camera.reset();
    auto near=camera.projection(2)*QVector4D(0,0,-0.1f,1);
    auto far=camera.projection(2)*QVector4D(0,0,-100,1);
    check(std::abs(near.z()/near.w()+1)<0.0001f && std::abs(far.z()/far.w()-1)<0.0001f,"perspective clipping before Vulkan correction");
    const auto eyeBefore=camera.target;
    camera.pan(20,10,720); check((camera.target-eyeBefore).length()>0,"pan changes target");
    camera.reset(); check(camera.yaw==35 && camera.pitch==25 && camera.distance==5.5f,"reset restores camera");
    const auto mesh=boxVertices();
    check(mesh.size()==36,"12 box triangles");
    for (int i=0; i<36; i+=3) {
        QVector3D points[3];
        for(int j=0;j<3;++j) {
            auto& p=mesh[i+j].position;
            check(p[1]>=0 && p[1]<=1.5f,"bottom centre convention");
            points[j]={p[0],p[1],p[2]};
        }
        check(QVector3D::crossProduct(points[1]-points[0],points[2]-points[0]).length()>0,"nondegenerate face");
    }
    check(Guides::compass().size()==18 && Guides::letters().size()==18,"compass and labels");
    QMatrix4x4 correction;
    const auto initialCompass=Guides::compassMatrix(camera.view(),correction);
    camera.pan(60,30,720); camera.zoom(3);
    const auto movedCompass=Guides::compassMatrix(camera.view(),correction);
    bool sameOrientation=true;
    for(int i=0;i<16;++i) sameOrientation &= std::abs(movedCompass.constData()[i]-initialCompass.constData()[i])<0.00001f;
    check(sameOrientation,"compass ignores pan and zoom");
    camera.orbit(30,20);
    check(Guides::compassMatrix(camera.view(),correction)!=initialCompass,"compass follows camera orbit");
    for(const auto size : {QSize(800,600),QSize(90,70),QSize(1,1)}) {
        const auto rect=Guides::compassRect(size,1.5);
        check(QRect(QPoint(0,0),size).contains(rect),"compass stays inside resized viewport");
    }
    camera.reset();
    QMatrix4x4 vulkanCorrection;
    vulkanCorrection(1,1)=-1; vulkanCorrection(2,2)=0.5f; vulkanCorrection(2,3)=0.5f;
    for(float distance : {2.5f,5.5f,30.0f}) {
        camera.distance=distance;
        const auto axis=infiniteYAxis(vulkanCorrection*camera.projection(1)*camera.view());
        check(axis.has_value(),"infinite Y axis visible across zoom range");
        if(axis) {
            const auto a=*axis*QVector4D(0,0,0,1),b=*axis*QVector4D(0,1,0,1);
            check(std::abs(std::abs(a.y())-1)<0.0001f && std::abs(std::abs(b.y())-1)<0.0001f,"Y axis reaches both viewport edges");
            check(a.z()>=0 && a.z()<=1 && b.z()>=0 && b.z()<=1,"Y axis retains valid depth");
        }
    }
    camera.target.setX(1000);
    check(!infiniteYAxis(vulkanCorrection*camera.projection(1)*camera.view()),"offscreen world axis is not a screen overlay");
    const auto cylinder=cylinderVertices();
    check(cylinder.size()==64*12,"closed cylinder triangle count");
    for(size_t i=0;i<cylinder.size();i+=3) {
        QVector3D p[3];
        for(int j=0;j<3;++j) {
            const auto& v=cylinder[i+j].position;
            p[j]={v[0],v[1],v[2]};
            check(v[1]>=0 && v[1]<=1.4f && v[0]*v[0]+v[2]*v[2]<=0.16001f,"cylinder dimensions");
        }
        check(QVector3D::crossProduct(p[1]-p[0],p[2]-p[0]).length()>0.00001f,"nondegenerate cylinder triangles");
    }
    if (!failures) std::puts("PASS: camera, projection, demo solids and guides");
    return failures?1:0;
}
