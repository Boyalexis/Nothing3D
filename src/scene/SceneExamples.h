#pragma once
#include "scene/Scene.h"

namespace n3d {
inline Scene initialScene() {
    Scene scene;
    scene.add({"长方体 1", BoxParameters{}});
    scene.add({"圆柱体 1", CylinderParameters{}, {-1600, 0, 0}});
    return scene;
}

// Small, repeatable Q4 examples; interactive creation and editing come in Q5.
inline Scene sceneExample(int example) {
    auto scene = initialScene();
    if (example == 1) {
        auto box = scene.objects().front().data;
        box.shape = BoxParameters{1200, 2100, 800};
        box.positionMm = {400, 0, 0};
        box.rotationDegrees = {0, 25, 0};
        scene.update(scene.objects().front().id, box);
    } else if (example == 2) {
        scene.add({"长方体 2", BoxParameters{800, 900, 800}, {1900, 0, 0}, {0, 30, 0}});
        scene.add({"圆柱体 2", CylinderParameters{650, 1900}, {1500, 0, -2200}});
        scene.add({"长方体 3", BoxParameters{1200, 600, 700}, {0, 0, -2200}, {0, -20, 0}});
        scene.add({"圆柱体 3", CylinderParameters{1000, 900}, {-1700, 0, -2200}});
    } else if (example == 3) {
        scene.clear();
    }
    return scene;
}
} // namespace n3d
