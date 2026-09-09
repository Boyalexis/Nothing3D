#include "render/ScenePicking.h"
#include <cstdio>

int main() {
    int failures = 0;
    auto check = [&](bool okay, const char* name) {
        if (!okay) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    QMatrix4x4 correction;
    correction(1,1) = -1; correction(2,2) = 0.5f; correction(2,3) = 0.5f;
    QMatrix4x4 projection, view;
    projection.perspective(45, 1, 0.1f, 100);
    view.lookAt({0,0.5f,5}, {0,0.5f,0}, {0,1,0});
    const auto vp = correction*projection*view;
    auto project = [](const QMatrix4x4& matrix, QVector3D p, QSizeF size = {600,600}) {
        const auto clip = matrix*QVector4D(p,1);
        return QPointF((clip.x()/clip.w()+1)*size.width()/2, (clip.y()/clip.w()+1)*size.height()/2);
    };
    n3d::Scene scene;
    const auto back = scene.add({"back", n3d::BoxParameters{2000,2000,1000}, {0,0,-2000}});
    const auto front = scene.add({"front", n3d::BoxParameters{1000,1000,1000}});
    check(ScenePicking::pick(scene,vp,{300,300},{600,600}) == front, "nearest surface wins, not insertion order or local distance");
    scene.remove(front);
    check(ScenePicking::pick(scene,vp,{300,300},{600,600}) == back, "deleted foreground reveals background");
    scene.clear();
    check(ScenePicking::pick(scene,vp,{300,300},{600,600}) == 0, "empty scene");
    const auto cylinder = scene.add({"cylinder", n3d::CylinderParameters{1000,1000}});
    check(ScenePicking::pick(scene,vp,{300,300},{600,600}) == cylinder, "cylinder side");
    QMatrix4x4 top;
    top.lookAt({0,5,0}, {0,0,0}, {0,0,-1});
    const auto topVp = correction*projection*top;
    check(ScenePicking::pick(scene,topVp,{300,300},{600,600}) == cylinder, "closed cylinder cap");
    check(ScenePicking::pick(scene,topVp,project(topVp,{0.47f,1,0.47f}),{600,600}) == 0,
          "cylinder bounding-box corner is not solid");
    auto data = scene.find(cylinder)->data;
    data.shape = n3d::BoxParameters{700,1200,500};
    data.positionMm = {1200,300,-400}; data.rotationDegrees = {20,40,30};
    scene.update(cylinder,data);
    QMatrix4x4 preview; preview.rotate(35,0,1,0);
    const auto transform = SceneGeometry::prepare(*scene.find(cylinder)).model;
    const auto target = transform.map(QVector3D(0,0.5f,0));
    const auto position = project(vp*preview,target);
    check(ScenePicking::pick(scene,vp*preview,position,{600,600}) == cylinder, "translated rotated scaled object with preview rotation");
    check(ScenePicking::pick(scene,vp*preview,position*1.5,{900,900}) == cylinder, "logical and 150-percent pixel coordinates agree");
    QMatrix4x4 wideProjection; wideProjection.perspective(45,2,0.1f,100);
    const auto wideVp = correction*wideProjection*view*preview;
    check(ScenePicking::pick(scene,wideVp,project(wideVp,target,{800,400}),{800,400}) == cylinder, "resized aspect ratio");
    for (const auto point : {QPointF(-1,300), QPointF(600,300), QPointF(10,10)})
        check(ScenePicking::pick(scene,vp,point,{600,600}) == 0, "outside viewport or empty background");
    check(ScenePicking::pick(scene,vp,{0,0},{0,0}) == 0, "zero-sized viewport");
    QMatrix4x4 singular; singular.fill(0);
    check(ScenePicking::pick(scene,singular,{300,300},{600,600}) == 0, "singular camera");
    scene.clear();
    scene.add({"behind camera", n3d::BoxParameters{1000,1000,1000}, {0,0,8000}});
    scene.add({"beyond far", n3d::BoxParameters{1000,1000,1000}, {0,0,-110000}});
    scene.add({"before near", n3d::BoxParameters{20,1000,20}, {0,0,4950}});
    check(ScenePicking::pick(scene,vp,{300,300},{600,600}) == 0, "only visible near-far segment participates");
    scene.clear();
    const auto enclosing = scene.add({"inside", n3d::BoxParameters{20000,20000,20000}, {0,-10000,0}});
    check(ScenePicking::pick(scene,vp,{300,300},{600,600}) == enclosing, "camera inside object hits exit face");
    if (!failures) std::puts("PASS: nearest triangle picking, cylinder caps, transforms, DPI, clipping");
    return failures ? 1 : 0;
}
