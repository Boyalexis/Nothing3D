#pragma once
#include "ui/MainWindow.h"
#include "scene/SceneExamples.h"
#include "render/VulkanViewport.h"
#include "render/BoxImageCheck.h"
#include <QComboBox>
#include <QTreeWidget>

// Independent CPU reference: generate actual-sized vertices, rather than using
// the renderer's unit mesh and SceneGeometry::prepare scale matrix.
inline std::vector<BoxVertex> sceneReference(const n3d::Scene& scene, n3d::ObjectId selected = 0) {
    std::vector<BoxVertex> result;
    for (const auto& object : scene.objects()) {
        const auto& data = object.data;
        std::vector<BoxVertex> mesh;
        if (const auto* box = std::get_if<n3d::BoxParameters>(&data.shape)) {
            const auto vertices = boxVertices(box->width/1000, box->height/1000, box->depth/1000);
            mesh.assign(vertices.begin(), vertices.end());
        } else {
            const auto& cylinder = std::get<n3d::CylinderParameters>(data.shape);
            mesh = cylinderVertices(cylinder.diameter/1000, cylinder.height/1000);
        }
        QMatrix4x4 placement;
        placement.translate(data.positionMm.x/1000, data.positionMm.y/1000, data.positionMm.z/1000);
        placement.rotate(data.rotationDegrees.z, 0, 0, 1);
        placement.rotate(data.rotationDegrees.y, 0, 1, 0);
        placement.rotate(data.rotationDegrees.x, 1, 0, 0);
        for (auto vertex : mesh) {
            if (object.id == selected) {
                constexpr float gold[] = {1.0f, 0.8f, 0.15f};
                for (int channel=0; channel<3; ++channel)
                    vertex.color[channel] = vertex.color[channel]*0.4f + gold[channel]*0.6f;
            }
            const auto point = placement.map(QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
            vertex.position[0] = point.x(); vertex.position[1] = point.y(); vertex.position[2] = point.z();
            result.push_back(vertex);
        }
    }
    return result;
}

inline bool checkSceneExamples(MainWindow& window) {
    auto* viewport = window.viewport();
    auto* examples = window.findChild<QComboBox*>("sceneExamples");
    auto* tree = window.findChild<QTreeWidget*>("sceneTree");
    if (!viewport || !examples || !tree || !viewport->supportsGrab()) return false;
    bool okay = true;
    auto check = [&](const QString& name) {
        const auto reference = sceneReference(window.scene());
        const auto image = viewport->grab();
        const auto corner = Guides::compassRect(image.size(), viewport->devicePixelRatio());
        const bool pixels = hasSceneMesh(image, viewport->modelViewProjection(), corner, reference,
                                        reference.empty() ? 0 : 12);
        okay &= pixels && tree->topLevelItemCount() == int(window.scene().objects().size())
            && viewport->scene().objects().size() == window.scene().objects().size();
        for (int i=0; i<tree->topLevelItemCount(); ++i) {
            const auto& object = window.scene().objects()[size_t(i)];
            okay &= tree->topLevelItem(i)->data(0, Qt::UserRole).toULongLong() == object.id
                && tree->topLevelItem(i)->text(0) == QString::fromStdString(object.data.name);
        }
        image.save(QStringLiteral("build/q4-%1.png").arg(name));
        qInfo("Q4 %s: pixels=%d objects=%d", qPrintable(name), int(pixels), int(window.scene().objects().size()));
    };
    for (int round=0; round<2; ++round) for (int index : {1,2,3,0}) {
        examples->setCurrentIndex(index);
        check(QStringLiteral("example-%1-%2").arg(round).arg(index));
    }
    auto scene = window.scene();
    const auto first = scene.objects().front().id;
    auto data = scene.find(first)->data;
    data.shape = n3d::CylinderParameters{1600, 1900};
    scene.update(first, data);
    window.setScene(scene);
    check(QStringLiteral("replace-type"));
    scene.remove(first);
    window.setScene(scene);
    check(QStringLiteral("remove-object"));
    scene.clear();
    window.setScene(scene);
    check(QStringLiteral("clear"));
    // Restore the baseline for the existing Q3 camera/guide regression checks.
    window.setScene(n3d::initialScene());
    viewport->resetView();
    check(QStringLiteral("restored"));
    window.grab().save(QStringLiteral("build/q4-window.png"));
    qInfo("Q4 scene mutation / examples / tree / readback: %d", int(okay));
    return okay;
}
