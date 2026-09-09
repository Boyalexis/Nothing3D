#pragma once
#include "ui/SceneCheck.h"
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCoreApplication>

inline bool checkSelection(MainWindow& window) {
    auto* viewport = window.viewport();
    auto* tree = window.findChild<QTreeWidget*>("sceneTree");
    auto* description = window.findChild<QLabel*>("sceneDescription");
    auto* examples = window.findChild<QComboBox*>("sceneExamples");
    if (!viewport || !tree || !description || !examples) return false;
    window.setScene(n3d::initialScene());
    viewport->resetView();
    const auto box = window.scene().objects()[0].id, cylinder = window.scene().objects()[1].id;
    bool okay = true;
    auto assertCheck = [&](bool result, const char* name) {
        okay &= result;
        qInfo("Q5 %s: %d", name, int(result));
    };
    auto mouse = [](QObject* target, QEvent::Type type, QPointF position, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, position, position, button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(target,&event);
    };
    auto click = [&](QPointF position) {
        mouse(viewport,QEvent::MouseButtonPress,position,Qt::LeftButton,Qt::LeftButton);
        mouse(viewport,QEvent::MouseButtonRelease,position,Qt::LeftButton,Qt::NoButton);
    };
    auto projected = [&](QVector3D point) {
        const auto clip = viewport->modelViewProjection()*QVector4D(point,1);
        return QPointF((clip.x()/clip.w()+1)*viewport->width()/2,
                       (clip.y()/clip.w()+1)*viewport->height()/2);
    };
    auto state = [&](n3d::ObjectId id, const char* name) {
        bool matches = window.selectedObject() == id && viewport->selectedObject() == id;
        const auto items = tree->selectedItems();
        if (id) {
            matches &= items.size() == 1 && items.front()->data(0,Qt::UserRole).toULongLong() == id
                && description->text().contains(QString::fromStdString(window.scene().find(id)->data.name));
        } else matches &= items.empty();
        assertCheck(matches,name);
    };
    auto pixels = [&](const char* name) {
        const auto image = viewport->grab();
        const auto reference = sceneReference(window.scene(),window.selectedObject());
        const auto corner = Guides::compassRect(image.size(),viewport->devicePixelRatio());
        assertCheck(hasSceneMesh(image,viewport->modelViewProjection(),corner,reference),name);
        image.save(QStringLiteral("build/q5-%1.png").arg(QString::fromLatin1(name)));
    };
    click(projected({0,0.75f,0}));
    state(box,"viewport selects box"); pixels("box-highlight");
    click(projected({-1.6f,0.7f,0}));
    state(cylinder,"viewport selects cylinder"); pixels("cylinder-highlight");
    auto* item = tree->topLevelItem(0);
    const auto itemCentre = tree->visualItemRect(item).center();
    mouse(tree->viewport(),QEvent::MouseButtonPress,itemCentre,Qt::LeftButton,Qt::LeftButton);
    mouse(tree->viewport(),QEvent::MouseButtonRelease,itemCentre,Qt::LeftButton,Qt::NoButton);
    state(box,"tree click updates viewport and parameters");
    // Small hand motion is still a click and must not rotate the camera.
    auto point = projected({-1.6f,0.7f,0});
    const auto yaw = viewport->camera.yaw;
    mouse(viewport,QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);
    mouse(viewport,QEvent::MouseMove,point+QPointF(1,1),Qt::NoButton,Qt::LeftButton);
    mouse(viewport,QEvent::MouseButtonRelease,point+QPointF(1,1),Qt::LeftButton,Qt::NoButton);
    state(cylinder,"click tolerates hand motion");
    assertCheck(viewport->camera.yaw == yaw,"short click leaves camera fixed");
    point = projected({0,0.75f,0});
    mouse(viewport,QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);
    mouse(viewport,QEvent::MouseMove,point+QPointF(30,15),Qt::NoButton,Qt::LeftButton);
    mouse(viewport,QEvent::MouseMove,point,Qt::NoButton,Qt::LeftButton);
    mouse(viewport,QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton);
    state(cylinder,"drag returning to origin does not pick");
    viewport->resetView();
    mouse(viewport,QEvent::MouseButtonPress,point,Qt::MiddleButton,Qt::MiddleButton);
    mouse(viewport,QEvent::MouseButtonRelease,point,Qt::MiddleButton,Qt::NoButton);
    state(cylinder,"middle click preserves selection");
    const auto corner = Guides::compassRect(viewport->swapChainImageSize(),viewport->devicePixelRatio());
    click(QPointF(corner.center())/viewport->devicePixelRatio());
    state(cylinder,"view cube does not pick through overlay");
    click(QPointF(corner.x()+corner.width()*0.89,corner.y()+corner.height()*0.10)/viewport->devicePixelRatio());
    state(cylinder,"Home preserves selection");
    click({4,4}); state(0,"blank click clears selection"); pixels("cleared");
    window.selectObject(box);
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    QCoreApplication::sendEvent(viewport,&escape);
    state(0,"Escape clears selection");
    viewport->modelAngle = 35;
    click(projected({0,0.75f,0}));
    state(box,"picking follows preview rotation"); pixels("rotated-highlight");
    viewport->resetView();
    window.resize(1200,740);
    QCoreApplication::processEvents();
    state(box,"selection survives resize"); pixels("resized-highlight");
    examples->setCurrentIndex(2);
    state(0,"scene replacement clears old IDs");
    window.selectObject(window.scene().objects().back().id);
    state(window.scene().objects().back().id,"sixth object parameters"); pixels("sixth-object");
    auto reduced = window.scene();
    reduced.remove(window.selectedObject());
    window.setScene(reduced); state(0,"removal clears stale selection");
    window.selectObject(999999); state(0,"unknown ID is rejected");
    examples->setCurrentIndex(3);
    click({100,100}); state(0,"empty scene click");
    examples->setCurrentIndex(0);
    viewport->resetView();
    state(0,"restored baseline");
    qInfo("Q5 selection / highlight / tree / parameters / input: %d",int(okay));
    return okay;
}
