#include "render/MoveGizmo.h"
#include "render/OrbitCamera.h"
#include <cstdio>
#include <limits>

int main() {
    int failures=0;
    auto check=[&](bool okay,const char* name) { if (!okay) { ++failures; std::fprintf(stderr,"FAIL: %s\n",name); } };
    QMatrix4x4 correction; correction(1,1)=-1; correction(2,2)=.5f; correction(2,3)=.5f;
    OrbitCamera camera;
    const QSizeF size(800,600);
    for (float angle : {0.f,37.f}) for (float distance : {4.f,9.f,25.f}) {
        camera.distance=distance;
        QMatrix4x4 model; model.rotate(angle,0,1,0);
        const auto vp=correction*camera.projection(800.f/600)*camera.view()*model;
        const n3d::Vector3 original{-123,237,-349};
        const auto handles=MoveGizmo::layout(vp,original,size);
        check(handles.has_value(),"visible handle layout at different distances and preview angles");
        if (!handles) continue;
        for (int axis=0;axis<3;++axis) {
            const auto base=MoveGizmo::metres(original);
            const auto startWorld=handles->model.map(MoveGizmo::axis(axis)*.55f);
            const auto start=MoveGizmo::project(vp,startWorld,size);
            const auto end=MoveGizmo::project(vp,startWorld+MoveGizmo::axis(axis)*.375f,size);
            if (!start || !end || !handles->visible[axis]) continue;
            check(MoveGizmo::hit(*handles,*start)==axis,"hit projected shaft");
            const auto drag=MoveGizmo::begin(vp,size,original,axis,*start);
            check(drag.has_value(),"begin axis drag");
            if (!drag) continue;
            const auto unchanged=drag->position(*start,false), free=drag->position(*end,false), snap=drag->position(*end,true);
            check(unchanged && *unchanged==original,"press keeps original position without jump");
            if (!free || !snap) { check(false,"finite drag result"); continue; }
            const float values[]={free->x,free->y,free->z}, snapped[]={snap->x,snap->y,snap->z};
            for (int i=0;i<3;++i) {
                const float expected=base[i]*1000+(i==axis ? 375:0);
                check(std::abs(values[i]-expected)<.15f,"only chosen world axis changes by known 375 mm");
                check(std::abs(snapped[i]-(i==axis ? std::round(expected/100)*100:expected))<.001f,"absolute signed 100 mm snapping on one axis");
            }
            check(drag->position(*start,false)==unchanged,"return to start has no cumulative drift");
            check(!drag->position({std::numeric_limits<double>::quiet_NaN(),0},false),"nonfinite input rejected");
            const auto scaled=MoveGizmo::begin(vp,size*1.5,original,axis,*start*1.5);
            const auto dpi=scaled ? scaled->position(*end*1.5,false):std::optional<n3d::Vector3>{};
            check(dpi && (MoveGizmo::metres(*dpi)-MoveGizmo::metres(*free)).length()<.0001f,"DPI scaling preserves millimetres");
        }
        check(MoveGizmo::hit(*handles,handles->origin)==-1,"centre reserved for object picking");
        check(MoveGizmo::hit(*handles,{-500,-500})==-1,"background misses handles");
    }
    QMatrix4x4 view; view.lookAt({0,0,5},{0,0,0},{0,1,0});
    const auto vp=correction*camera.projection(800.f/600)*view;
    const auto headOn=MoveGizmo::layout(vp,{},size);
    check(headOn && !headOn->visible[2],"end-on Z axis hidden");
    check(!MoveGizmo::begin(vp,size,{},2,{400,300}),"end-on unstable drag rejected");
    check(!MoveGizmo::layout(vp,{0,0,6000},size),"behind-camera handle hidden");
    check(!MoveGizmo::layout(vp,{100000,0,0},size),"off-screen anchor hidden");
    check(!MoveGizmo::layout(vp,{},{}),"zero size rejected");
    QMatrix4x4 singular; singular.fill(0);
    check(!MoveGizmo::begin(singular,size,{},0,{400,300}),"singular projection rejected");
    check(MoveGizmo::vertices().size()==3*MoveGizmo::verticesPerAxis,"complete draw ranges");
    if (!failures) std::puts("PASS: axis drag, snapping, preview rotation, zoom, DPI, degeneracy");
    return failures ? 1:0;
}
