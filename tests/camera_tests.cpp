#include "render/OrbitCamera.h"
#include "render/BoxGeometry.h"
#include "render/GuideGeometry.h"
#include "render/InfiniteAxis.h"
#include "render/CylinderGeometry.h"
#include "render/ScenePicking.h"
#include "render/GroundPlacement.h"
#include "render/MoveGizmo.h"
#include "render/SceneFraming.h"
#include "scene/PerformanceScene.h"
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
    camera.reset();
    const auto perspectiveTarget=camera.projection(1)*QVector4D(1,0,-camera.distance,1);
    camera.orthographic=true;
    const auto orthoTarget=camera.projection(1)*QVector4D(1,0,-camera.distance,1);
    check(std::abs(perspectiveTarget.x()/perspectiveTarget.w()-orthoTarget.x())<1e-5f,"switch preserves target-plane scale");
    const auto orthoFar=camera.projection(1)*QVector4D(1,0,-30,1);
    check(std::abs(orthoFar.x()-orthoTarget.x())<1e-5f && orthoFar.w()==1,"orthographic size independent of depth");
    n3d::Scene sample;
    const auto sampleId=sample.add({"box",n3d::BoxParameters{1000,1000,1000}});
    for(auto mode:{OrbitCamera::StandardView::Top,OrbitCamera::StandardView::Front,OrbitCamera::StandardView::Right}) {
        camera.reset(); camera.target={0,.5f,0}; camera.standardView(mode);
        for(const QSizeF size:{QSizeF(800,600),QSizeF(1200,900)}) {
            const auto vp=vulkanCorrection*camera.projection(float(size.width()/size.height()))*camera.view();
            bool invertible=false; vp.inverted(&invertible);
            check(invertible,"standard view invertible including exact top");
            check(ScenePicking::pick(sample,vp,{size.width()/2,size.height()/2},size)==sampleId,"orthographic standard view picking");
            const auto ground=GroundPlacement::point(vp,{size.width()/2+17,size.height()/2+23},size,true);
            check(ground.has_value()==(mode==OrbitCamera::StandardView::Top),"ground placement only when ray meets ground");
            if(ground) check(std::fmod(std::abs(ground->x),100)==0 && std::fmod(std::abs(ground->z),100)==0,"orthographic ground snap");
            const auto layout=MoveGizmo::layout(vp,{},size);
            check(layout.has_value(),"orthographic gizmo visible");
            if(layout) for(int axis=0;axis<3;++axis) {
                const int hidden=mode==OrbitCamera::StandardView::Top ? 1 : mode==OrbitCamera::StandardView::Front ? 2 : 0;
                check(layout->visible[axis]==(axis!=hidden),"end-on gizmo axis hidden");
                if(axis==hidden) continue;
                auto drag=MoveGizmo::begin(vp,size,{},axis,layout->origin);
                check(drag.has_value(),"orthographic drag starts");
                if(drag) {
                    const auto point=MoveGizmo::project(vp,MoveGizmo::axis(axis)*.237f,size);
                    const auto moved=point ? drag->position(*point,true) : std::nullopt;
                    check(moved.has_value(),"orthographic drag resolves");
                    if(moved) check(std::abs((axis==0 ? moved->x : axis==1 ? moved->y : moved->z)-200)<.01f,"orthographic axis snap 237 to 200 mm");
                }
            }
        }
        const auto before=camera.target; camera.pan(20,10,600);
        check((camera.target-before).length()>.01f,"standard view pan including top");
        camera.orbit(10,10); check(std::isfinite(camera.view()(0,0)),"orbit away from standard view");
    }
    camera.reset(); check(!camera.orthographic,"reset restores perspective");
    for(bool ortho:{false,true}) for(float aspect:{.35f,1.7f}) {
        auto scene=n3d::performanceScene();
        scene.add({"rotated",n3d::BoxParameters{500,2000,900},{13000,400,-5000},{30,60,20}});
        for(float scale:{.001f,1.0f,1000.0f}) {
            auto scaled=scene;
            for(const auto object:scene.objects()) {
                auto data=object.data;
                data.positionMm={data.positionMm.x*scale,data.positionMm.y*scale,data.positionMm.z*scale};
                if(auto* box=std::get_if<n3d::BoxParameters>(&data.shape)) { box->width*=scale; box->height*=scale; box->depth*=scale; }
                else { auto& cylinder=std::get<n3d::CylinderParameters>(data.shape); cylinder.diameter*=scale; cylinder.height*=scale; }
                scaled.update(object.id,data);
            }
            camera.reset(); camera.orthographic=ortho;
            if(ortho) camera.standardView(OrbitCamera::StandardView::Top);
            const auto yaw=camera.yaw,pitch=camera.pitch;
            check(frameScene(camera,scaled,aspect),"frame nonempty scene");
            check(camera.yaw==yaw && camera.pitch==pitch && camera.orthographic==ortho,"framing preserves direction and projection");
            auto matrix=camera.projection(aspect)*camera.view();
            bool contained=true;
            for(const auto& object:scaled.objects()) {
                const auto model=SceneGeometry::prepare(object).model;
                for(float x:{-.5f,.5f}) for(float y:{0.0f,1.0f}) for(float z:{-.5f,.5f}) {
                    const auto clip=matrix*model*QVector4D(x,y,z,1);
                    const auto p=clip.toVector3DAffine();
                    contained &= clip.w()>0 && std::abs(p.x())<.96f && std::abs(p.y())<.96f && p.z()>-1 && p.z()<1;
                }
            }
            check(contained,"all transformed corners fit width height and clip depth at small and large scales");
            const auto distance=camera.distance; camera.zoom(1);
            check(camera.distance<distance,"zoom after fit does not jump to old limits");
        }
    }
    const auto beforeEmpty=camera.target;
    check(!frameScene(camera,n3d::Scene{},1) && camera.target==beforeEmpty,"empty fit keeps camera");
    camera.reset(); check(camera.framingRadius==0 && camera.maximumDistance==30,"reset clears adaptive framing");
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
