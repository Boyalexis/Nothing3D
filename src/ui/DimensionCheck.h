#pragma once
#include "ui/ExampleCheck.h"
#include "ui/SceneCheck.h"
#include "render/ScenePicking.h"
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QScrollArea>
#include <QLabel>
#include <QCoreApplication>

inline bool checkDimensions(MainWindow& window) {
    auto* width = window.findChild<QDoubleSpinBox*>("dimensionWidth");
    auto* height = window.findChild<QDoubleSpinBox*>("dimensionHeight");
    auto* depth = window.findChild<QDoubleSpinBox*>("dimensionDepth");
    auto* panel = window.findChild<QWidget*>("dimensionPanel");
    auto* tree = window.findChild<QTreeWidget*>("sceneTree");
    auto* examples = window.findChild<QComboBox*>("sceneExamples");
    auto* viewport = window.viewport();
    if (!width || !height || !depth || !panel || !tree || !examples) return false;
    bool okay = true;
    auto check = [&](bool result, const char* name) {
        okay &= result;
        qInfo("Dimensions %s: %d",name,int(result));
    };
    auto key = [](QObject* target, int code, const QString& text = {}) {
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier, text);
        QCoreApplication::sendEvent(target, &press);
        QKeyEvent release(QEvent::KeyRelease, code, Qt::NoModifier, text);
        QCoreApplication::sendEvent(target, &release);
    };
    auto type = [&](QDoubleSpinBox* editor, const QString& text) {
        editor->setFocus();
        editor->selectAll();
        key(editor->findChild<QLineEdit*>(), 0, text);
    };
    auto enter = [&](QDoubleSpinBox* editor, const QString& text) {
        type(editor,text);
        key(editor,Qt::Key_Return);
    };
    auto pixels = [&](const char* name) {
        if (!viewport) return;
        const auto image = viewport->grab();
        const auto reference = sceneReference(window.scene(),window.selectedObject());
        const auto corner = Guides::compassRect(image.size(),viewport->devicePixelRatio());
        check(hasSceneMesh(image,viewport->modelViewProjection(),corner,reference),name);
        image.save(QStringLiteral("build/q5-dimensions-%1.png").arg(QString::fromLatin1(name)));
    };
    window.setScene(n3d::initialScene());
    if (viewport) viewport->resetView();
    const auto boxId = window.scene().objects()[0].id, cylinderId = window.scene().objects()[1].id;
    const auto originalCylinder = window.scene().find(cylinderId)->data;
    check(!panel->isEnabled() && panel->isHidden(),"no selection hides editor");
    width->setValue(900);
    check(std::get<n3d::BoxParameters>(window.scene().find(boxId)->data.shape).width == 2000,
          "disabled editor cannot mutate scene");
    window.selectObject(boxId);
    auto* originalItem = tree->topLevelItem(0);
    check(width->value() == 2000 && height->value() == 1500 && depth->value() == 1000 && !depth->isHidden(),
          "box fields populated");
    type(width,QStringLiteral("1200.25"));
    check(std::get<n3d::BoxParameters>(window.scene().find(boxId)->data.shape).width == 2000,
          "typing is not committed prematurely");
    key(width,Qt::Key_Return);
    enter(height,QStringLiteral("2100"));
    type(depth,QStringLiteral("800"));
    QFocusEvent focusOut(QEvent::FocusOut,Qt::TabFocusReason);
    QCoreApplication::sendEvent(depth,&focusOut);
    const auto changedBox = window.scene().find(boxId)->data;
    check(std::get<n3d::BoxParameters>(changedBox.shape) == n3d::BoxParameters{1200.25f,2100,800},
          "Return and focus loss commit dimensions");
    check(changedBox.positionMm == n3d::Vector3{} && changedBox.rotationDegrees == n3d::Vector3{}
          && window.scene().find(cylinderId)->data == originalCylinder,"edit preserves placement and neighbour");
    check(window.selectedObject() == boxId && tree->topLevelItem(0) == originalItem
          && originalItem->toolTip(0).contains(QStringLiteral("1200.25")),"selection and tree identity retained, tooltip refreshed");
    if (viewport) {
        check(viewport->scene().find(boxId)->data == changedBox && viewport->selectedObject() == boxId
              && viewport->camera.yaw == 35 && viewport->camera.distance == 5.5f,"render snapshot and camera retained");
    }
    pixels("box-edited");
    if (viewport) {
        const auto before = n3d::initialScene();
        int removedHits = 0, addedHits = 0;
        for (int y=40; y<viewport->height()-40; y+=20) for (int x=40; x<viewport->width()-40; x+=20) {
            const QPointF point(x,y); const QSizeF size(viewport->width(),viewport->height());
            const auto oldHit = ScenePicking::pick(before,viewport->modelViewProjection(),point,size);
            const auto newHit = ScenePicking::pick(viewport->scene(),viewport->modelViewProjection(),point,size);
            removedHits += oldHit == boxId && newHit != boxId;
            addedHits += oldHit != boxId && newHit == boxId;
        }
        check(removedHits > 0 && addedHits > 0,"picking follows both shrink and growth");
    }
    for (const auto& invalid : {QStringLiteral("0"),QStringLiteral("-20"),QStringLiteral("nan"),
                                QStringLiteral("inf"),QStringLiteral("10000001"),QStringLiteral("")}) {
        // Simulate an invalid intermediate/pasted buffer, then Qt correction.
        width->findChild<QLineEdit*>()->setText(invalid);
        key(width,Qt::Key_Return);
        check(window.scene().find(boxId)->data == changedBox && width->value() == 1200.25,
              "invalid input restores previous value");
    }
    enter(width,QStringLiteral("0.01"));
    check(std::get<n3d::BoxParameters>(window.scene().find(boxId)->data.shape).width == 0.01f,"minimum dimension accepted");
    enter(width,QStringLiteral("10000000"));
    check(std::get<n3d::BoxParameters>(window.scene().find(boxId)->data.shape).width == 1e7f,"maximum dimension accepted");
    enter(width,QStringLiteral("1200.25"));
    // A pending field from the old selection must not leak into the new one.
    type(depth,QStringLiteral("777"));
    window.selectObject(cylinderId);
    key(depth,Qt::Key_Return);
    check(depth->isHidden() && width->accessibleName() == QStringLiteral("直径")
          && width->value() == 800 && height->value() == 1400,"cylinder fields replace box fields");
    check(window.scene().find(cylinderId)->data == originalCylinder,"pending box edit cannot affect cylinder");
    enter(width,QStringLiteral("1000"));
    enter(height,QStringLiteral("1700.5"));
    const auto cylinder = window.scene().find(cylinderId)->data;
    check(std::get<n3d::CylinderParameters>(cylinder.shape) == n3d::CylinderParameters{1000,1700.5f}
          && cylinder.positionMm == originalCylinder.positionMm,"diameter and height edited at fixed bottom centre");
    pixels("cylinder-edited");
    height->stepUp();
    check(std::get<n3d::CylinderParameters>(window.scene().find(cylinderId)->data.shape).height == 1800.5f,
          "step button commits immediately");
    if (viewport) {
        viewport->camera.orbit(12,4); viewport->modelAngle = 20;
        const auto cameraBefore = viewport->camera.view();
        enter(height,QStringLiteral("1600"));
        check(viewport->camera.view() == cameraBefore && viewport->modelAngle == 20
              && viewport->selectedObject() == cylinderId,"editing preserves custom view and preview angle");
        pixels("rotated-edited");
        viewport->resetView();
        window.resize(960,600);
        QCoreApplication::processEvents();
        window.findChild<QScrollArea*>("propertyScroll")->grab().save(QStringLiteral("build/q5-dimensions-panel.png"));
        pixels("minimum-window");
    }
    type(height,QStringLiteral("2222"));
    window.selectObject(0);
    key(height,Qt::Key_Return);
    check(window.selectedObject() == 0 && !panel->isEnabled(),"deselection disables pending edits");
    discardAndSelectExample(window,examples,3);
    width->setValue(300);
    check(window.scene().objects().empty(),"empty scene cannot be edited");
    examples->setCurrentIndex(0);
    window.selectObject(boxId);
    check(width->value() == 2000 && height->value() == 1500 && depth->value() == 1000,"scene replacement repopulates fresh values");
    auto preciseScene = window.scene();
    auto precise = preciseScene.find(boxId)->data;
    precise.shape = n3d::BoxParameters{1200.1234f,1500,800.1234f};
    preciseScene.update(boxId,precise);
    window.setScene(preciseScene);
    window.selectObject(boxId);
    key(width,Qt::Key_Return);
    check(window.scene().find(boxId)->data == precise,"display rounding alone does not edit data");
    enter(height,QStringLiteral("1600"));
    const auto preciseAfter = std::get<n3d::BoxParameters>(window.scene().find(boxId)->data.shape);
    check(preciseAfter.width == 1200.1234f && preciseAfter.depth == 800.1234f,
          "editing one field preserves unedited precision");
    window.setScene(n3d::initialScene());
    window.selectObject(0);
    if (viewport) { window.resize(1200,740); QCoreApplication::processEvents(); viewport->resetView(); }
    qInfo("Dimensions overall: %d",int(okay));
    return okay;
}
