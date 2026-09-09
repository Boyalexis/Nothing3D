#include "render/GroundPlacement.h"
#include "render/SceneGeometry.h"
#include <cstdio>
#include <limits>

int main() {
    int failures = 0;
    auto check = [&](bool okay,const char* name) { if (!okay) { ++failures; std::fprintf(stderr,"FAIL: %s\n",name); } };
    QMatrix4x4 correction; correction(1,1)=-1; correction(2,2)=0.5f; correction(2,3)=0.5f;
    QMatrix4x4 projection; projection.perspective(45,1,0.1f,100);
    QMatrix4x4 view; view.lookAt({0,5,0},{0,0,0},{0,0,-1});
    auto vp = correction*projection*view;
    auto project = [](const QMatrix4x4& matrix,QVector3D point) {
        const auto clip=matrix*QVector4D(point,1);
        return QPointF((clip.x()/clip.w()+1)*300,(clip.y()/clip.w()+1)*300);
    };
    const auto pixel=project(vp,{-0.823f,0,0.736f});
    const auto free=GroundPlacement::point(vp,pixel,{600,600},false);
    check(free && std::abs(free->x+823)<0.05f && free->y==0 && std::abs(free->z-736)<0.05f,"free placement converts metres to mm");
    const auto snapped=GroundPlacement::point(vp,pixel,{600,600},true);
    check(snapped && snapped->x==-800 && snapped->z==700,"signed coordinates snap to nearest 100 mm");
    const auto dpi=GroundPlacement::point(vp,pixel*1.5,{900,900},false);
    check(dpi && free && std::abs(dpi->x-free->x)<0.01f && std::abs(dpi->z-free->z)<0.01f,"logical and scaled pixel coordinates agree");
    check(!GroundPlacement::point(vp,{-1,100},{600,600},false) && !GroundPlacement::point(vp,{600,100},{600,600},false),"outside viewport rejected");
    check(!GroundPlacement::point(vp,{0,0},{0,0},false),"empty viewport rejected");
    check(!GroundPlacement::point(vp,{std::numeric_limits<double>::quiet_NaN(),0},{600,600},false),"nonfinite cursor rejected");
    QMatrix4x4 singular; singular.fill(0);
    check(!GroundPlacement::point(singular,{300,300},{600,600},false),"singular matrix rejected");
    view.setToIdentity(); view.lookAt({0,1,5},{0,1,0},{0,1,0});
    check(!GroundPlacement::point(correction*projection*view,{300,300},{600,600},false),"parallel ray rejected");
    view.setToIdentity(); view.lookAt({0,5,0},{0,10,0},{0,0,-1});
    check(!GroundPlacement::point(correction*projection*view,{300,300},{600,600},false),"ground behind camera rejected");
    view.setToIdentity(); view.lookAt({0,150,0},{0,0,0},{0,0,-1});
    check(!GroundPlacement::point(correction*projection*view,{300,300},{600,600},false),"ground past far clip rejected");
    view.setToIdentity(); view.lookAt({0,0.05f,0},{0,0,0},{0,0,-1});
    check(!GroundPlacement::point(correction*projection*view,{300,300},{600,600},false),"ground before near clip rejected");
    const auto wire=SceneGeometry::wireVertices();
    check(wire.size()==(SceneGeometry::boxCount+SceneGeometry::cylinderCount)*2,"both shared primitive wire ranges");
    for (const auto& vertex:wire) check(vertex.color[0]==1 && vertex.color[1]==0.8f && vertex.color[2]==0.15f,"preview wire colour");
    if (!failures) std::puts("PASS: ground placement, signed snapping, clipping, preview geometry");
    return failures ? 1 : 0;
}
