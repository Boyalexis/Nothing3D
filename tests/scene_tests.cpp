#include "scene/SceneExamples.h"
#include "render/SceneGeometry.h"
#include <cstdio>
#include <limits>

int main() {
    int failures = 0;
    auto check = [&](bool okay, const char* name) {
        if (!okay) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
    };
    auto near = [](QVector3D a, QVector3D b) { return (a-b).length() < 0.00001f; };
    auto scene = n3d::initialScene();
    const auto boxId = scene.objects()[0].id, cylinderId = scene.objects()[1].id;
    const auto cylinderBefore = scene.find(cylinderId)->data;
    auto changed = scene.find(boxId)->data;
    changed.shape = n3d::BoxParameters{1000, 2000, 3000};
    changed.positionMm = {4000, 5000, 6000};
    changed.rotationDegrees = {0, 90, 0};
    check(scene.update(boxId, changed), "update existing ID");
    check(scene.find(cylinderId)->data == cylinderBefore, "editing one object preserves its neighbour");
    const auto model = SceneGeometry::prepare(*scene.find(boxId)).model;
    check(near(model.map(QVector3D(0,0,0)), {4,5,6}), "bottom centre is world position in metres");
    check(near(model.map(QVector3D(0.5f,1,0.5f)), {5.5f,7,5.5f}), "dimensions, rotation, translation order");
    auto cylinder = scene.find(cylinderId)->data;
    cylinder.shape = n3d::CylinderParameters{2000, 3000};
    cylinder.rotationDegrees = {90, 0, 0};
    scene.update(cylinderId, cylinder);
    const auto cylinderModel = SceneGeometry::prepare(*scene.find(cylinderId));
    check(cylinderModel.primitive == SceneGeometry::Primitive::Cylinder, "cylinder render kind");
    check(near(cylinderModel.model.map(QVector3D(0,1,0)), {-1.6f,0,3}), "cylinder height and local X rotation");
    check(near(cylinderModel.model.map(QVector3D(0.5f,0,0)), {-0.6f,0,0}), "diameter is not radius");
    auto invalid = [&](n3d::ObjectData bad) {
        bool rejected = false;
        try { scene.update(boxId, bad); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected && scene.find(boxId)->data == changed, "invalid update is atomic");
    };
    for (float dimension : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::quiet_NaN(), 1e8f}) {
        auto bad = changed;
        bad.shape = n3d::BoxParameters{dimension, 2000, 3000};
        invalid(bad);
        bad.shape = n3d::CylinderParameters{dimension, 1400};
        invalid(bad);
    }
    auto bad = changed;
    bad.positionMm.z = std::numeric_limits<float>::quiet_NaN(); invalid(bad);
    bad = changed; bad.rotationDegrees.y = std::numeric_limits<float>::infinity(); invalid(bad);
    const auto sizeBefore = scene.objects().size();
    try { scene.add(bad); check(false, "invalid add rejected"); } catch (const std::invalid_argument&) {}
    check(scene.objects().size() == sizeBefore, "invalid add preserves scene");
    check(!scene.update(999, changed) && !scene.remove(999), "missing IDs are harmless");
    check(scene.remove(boxId) && !scene.find(boxId) && scene.find(cylinderId), "delete by stable ID");
    const auto next = scene.add(changed);
    check(next > cylinderId, "deleted IDs are not reused");
    scene.clear();
    check(scene.objects().empty() && scene.add(changed) > next, "clear preserves ID sequence");
    changed.shape = n3d::CylinderParameters{};
    scene.update(scene.objects()[0].id, changed);
    check(SceneGeometry::prepare(scene.objects()[0]).primitive == SceneGeometry::Primitive::Cylinder,
          "type replacement changes render primitive");
    const auto unit = SceneGeometry::vertices();
    check(unit.size() == SceneGeometry::boxCount + SceneGeometry::cylinderCount, "shared primitive ranges");
    for (const auto& vertex : unit) {
        const auto& p = vertex.position;
        check(p[0] >= -0.50001f && p[0] <= 0.50001f && p[1] >= 0 && p[1] <= 1
              && p[2] >= -0.50001f && p[2] <= 0.50001f, "unit meshes are centred on their bottom");
    }
    auto many = n3d::sceneExample(2);
    check(many.objects().size() == 6 && n3d::sceneExample(3).objects().empty(), "multiple and empty presets");
    scene.clear();
    for (int i=0; i<1000; ++i) scene.add({"object", n3d::BoxParameters{}, {float(i)*1000, 0, 0}});
    check(scene.objects().size() == 1000, "1000 independent CPU objects");
    check(near(SceneGeometry::prepare(scene.objects().back()).model.map(QVector3D()), {999,0,0}),
          "last object keeps its own transform");
    if (!failures) std::puts("PASS: scene identity, validation, independence, transforms and geometry");
    return failures ? 1 : 0;
}
