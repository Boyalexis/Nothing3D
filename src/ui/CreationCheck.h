#pragma once
#include "ui/CreationDialog.h"
#include "ui/DimensionCheck.h"
#include <QAction>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>

inline bool checkCreation(MainWindow& window) {
    auto* viewport=window.viewport();
    auto* tree=window.findChild<QTreeWidget*>("sceneTree");
    auto* tabs=window.findChild<QTabWidget*>("ribbonTabs");
    auto* boxAction=window.findChild<QAction*>("createBox");
    auto* cylinderAction=window.findChild<QAction*>("createCylinder");
    auto* cancelAction=window.findChild<QAction*>("cancelPlacement");
    if (!viewport || !tree || !tabs || !boxAction || !cylinderAction || !cancelAction) return false;
    bool okay=true;
    auto check=[&](bool result,const char* name) { okay &= result; qInfo("Creation %s: %d",name,int(result)); };
    auto mouse=[&](QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
        QMouseEvent event(type,point,point,button,buttons,modifiers); QCoreApplication::sendEvent(viewport,&event);
    };
    auto hover=[&](QPointF point,Qt::KeyboardModifiers modifiers=Qt::NoModifier) { mouse(QEvent::MouseMove,point,Qt::NoButton,Qt::NoButton,modifiers); };
    auto click=[&](QPointF point,Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
        mouse(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton,modifiers);
        mouse(QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton,modifiers);
    };
    auto project=[&](QVector3D point) {
        const auto clip=viewport->modelViewProjection()*QVector4D(point,1);
        return QPointF((clip.x()/clip.w()+1)*viewport->width()/2,(clip.y()/clip.w()+1)*viewport->height()/2);
    };
    auto key=[&](QEvent::Type type,int code,Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
        QKeyEvent event(type,code,modifiers); QCoreApplication::sendEvent(viewport,&event);
    };
    auto dialog=[&](QAction* action) -> QDialog* {
        action->trigger(); QCoreApplication::processEvents();
        for (auto* d:window.findChildren<QDialog*>("creationDialog")) if (d->isVisible()) return d;
        check(false,"creation action opens dialog"); return nullptr;
    };
    auto confirm=[&](QDialog* d) {
        if (!d) return;
        d->findChild<QPushButton*>("createConfirm")->click(); QCoreApplication::processEvents();
        check(viewport->isPlacing() && cancelAction->isEnabled(),"confirmation enters placement");
    };
    auto pixels=[&](const char* name) {
        const auto image=viewport->grab();
        const auto reference=sceneReference(window.scene(),window.selectedObject());
        check(hasSceneMesh(image,viewport->modelViewProjection(),Guides::compassRect(image.size(),viewport->devicePixelRatio()),reference),name);
        image.save(QStringLiteral("build/q5-create-%1.png").arg(QString::fromLatin1(name)));
    };
    window.setScene(n3d::initialScene()); viewport->resetView(); tabs->setCurrentIndex(1);
    const auto originalBox=window.scene().objects()[0], originalCylinder=window.scene().objects()[1];
    window.selectObject(originalBox.id);
    check(boxAction->isEnabled() && cylinderAction->isEnabled(),"both creation tools enabled");
    if (auto* d=dialog(boxAction)) {
        d->findChild<QPushButton*>("createCancel")->click(); QCoreApplication::processEvents();
        check(!viewport->isPlacing() && window.scene().objects().size()==2,"dialog cancellation creates nothing");
    }
    viewport->modelAngle=30; viewport->toggleRotation();
    if (auto* d=dialog(boxAction)) {
        auto* width=d->findChild<QDoubleSpinBox*>("createWidth");
        width->findChild<QLineEdit*>()->setText(QStringLiteral("650.25"));
        d->findChild<QDoubleSpinBox*>("createHeight")->setValue(1000);
        d->findChild<QDoubleSpinBox*>("createDepth")->setValue(700);
        d->grab().save(QStringLiteral("build/q5-create-dialog.png"));
        confirm(d);
    }
    if (!viewport->isPlacing()) return false;
    check(viewport->modelAngle==0 && !window.findChild<QAction*>("autoRotate")->isEnabled()
          && viewport->camera.yaw==35,"placement stops preview rotation and keeps camera");
    const auto blankPreview=viewport->grab();
    auto target=project({1.8f,0,1.0f}); hover(target);
    check(viewport->placementPreview().has_value() && window.scene().objects().size()==2
          && viewport->scene().objects().size()==2,"hover previews without changing scene");
    auto previewImage=viewport->grab();
    check(previewImage!=blankPreview,"wire preview is visible");
    previewImage.save(QStringLiteral("build/q5-create-preview.png"));
    key(QEvent::KeyPress,Qt::Key_Control,Qt::ControlModifier);
    check(viewport->placementPreview() && std::fmod(viewport->placementPreview()->positionMm.x,100)==0,"Ctrl updates preview without moving mouse");
    key(QEvent::KeyRelease,Qt::Key_Control);
    // A drag may move the camera, but never commits placement.
    mouse(QEvent::MouseButtonPress,target,Qt::LeftButton,Qt::LeftButton);
    mouse(QEvent::MouseMove,target+QPointF(35,15),Qt::NoButton,Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease,target+QPointF(35,15),Qt::LeftButton,Qt::NoButton);
    check(window.scene().objects().size()==2 && viewport->isPlacing(),"drag does not create");
    viewport->resetView();
    viewport->camera.pitch=5; hover({4,4}); click({4,4});
    check(!viewport->placementPreview() && viewport->isPlacing() && window.scene().objects().size()==2,"sky click is rejected");
    viewport->resetView();
    const auto corner=Guides::compassRect(viewport->swapChainImageSize(),viewport->devicePixelRatio());
    hover(QPointF(corner.center())/viewport->devicePixelRatio()); click(QPointF(corner.center())/viewport->devicePixelRatio());
    check(!viewport->placementPreview() && window.scene().objects().size()==2,"view cube cannot place through overlay");
    target=project({1.8f,0,1.0f}); hover(target); click(target);
    check(window.scene().objects().size()==3 && !viewport->isPlacing() && !cancelAction->isEnabled(),"one click commits exactly one object");
    if (window.scene().objects().size()!=3) return false;
    const auto createdBox=window.scene().objects().back();
    check(createdBox.id>originalCylinder.id && createdBox.data.name=="长方体 2"
          && std::get<n3d::BoxParameters>(createdBox.data.shape)==n3d::BoxParameters{650.25f,1000,700},"unique ID, name and dialog dimensions retained");
    check(std::abs(createdBox.data.positionMm.x-1800)<0.1f && createdBox.data.positionMm.y==0
          && std::abs(createdBox.data.positionMm.z-1000)<0.1f && createdBox.data.rotationDegrees==n3d::Vector3{},"new bottom centre matches ground click");
    check(window.selectedObject()==createdBox.id && viewport->selectedObject()==createdBox.id
          && tree->topLevelItemCount()==3 && window.scene().find(originalBox.id)->data==originalBox.data
          && window.scene().find(originalCylinder.id)->data==originalCylinder.data,"new selection and tree synchronized, neighbours unchanged");
    pixels("box-placed");
    window.findChild<QDoubleSpinBox*>("dimensionHeight")->setValue(1100);
    check(std::get<n3d::BoxParameters>(viewport->scene().find(createdBox.id)->data.shape).height==1100,"created object remains editable");
    if (auto* d=dialog(cylinderAction)) {
        d->findChild<QDoubleSpinBox*>("createWidth")->setValue(600);
        d->findChild<QDoubleSpinBox*>("createHeight")->setValue(900); confirm(d);
    }
    target=project({-1.24f,0,1.26f}); hover(target,Qt::ControlModifier); click(target,Qt::ControlModifier);
    check(window.scene().objects().size()==4 && !viewport->isPlacing(),"second creation succeeds");
    if (window.scene().objects().size()!=4) return false;
    const auto createdCylinder=window.scene().objects().back();
    check(createdCylinder.data.name=="圆柱体 2" && createdCylinder.data.positionMm==n3d::Vector3{-1200,0,1300}
          && std::get<n3d::CylinderParameters>(createdCylinder.data.shape)==n3d::CylinderParameters{600,900},"Ctrl snaps signed XZ coordinates to 100 mm grid");
    pixels("cylinder-placed");
    click(project({-1.2f,0.45f,1.3f}));
    check(window.selectedObject()==createdCylinder.id,"new object participates in picking");
    if (auto* d=dialog(boxAction)) confirm(d);
    hover(project({0,0,2})); key(QEvent::KeyPress,Qt::Key_Escape);
    check(window.scene().objects().size()==4 && !viewport->isPlacing() && !viewport->placementPreview()
          && window.selectedObject()==createdCylinder.id,"Escape cancels preview and preserves previous selection");
    if (auto* d=dialog(boxAction)) confirm(d);
    hover(project({0,0,2}));
    QEvent leave(QEvent::Leave); QCoreApplication::sendEvent(viewport,&leave);
    check(!viewport->placementPreview() && viewport->isPlacing(),"leaving viewport hides ghost without creating");
    window.resize(1100,700); QCoreApplication::processEvents();
    hover(project({0,0,2}));
    check(viewport->placementPreview().has_value(),"preview recovers after resize");
    cancelAction->trigger();
    check(!viewport->isPlacing() && window.scene().objects().size()==4,"cancel toolbar action leaves scene intact");
    if (auto* d=dialog(boxAction)) confirm(d);
    window.selectObject(originalBox.id);
    check(!viewport->isPlacing(),"selecting another object cancels placement");
    if (auto* d=dialog(boxAction)) confirm(d);
    auto* examples=window.findChild<QComboBox*>("sceneExamples");
    examples->setCurrentIndex(3);
    check(!viewport->isPlacing() && window.scene().objects().empty(),"scene replacement cancels placement");
    viewport->showGrid=false;
    if (auto* d=dialog(boxAction)) confirm(d);
    hover(project({0,0,0})); click(project({0,0,0}));
    check(window.scene().objects().size()==1 && window.selectedObject()!=0,"empty scene and hidden grid allow ground placement");
    viewport->showGrid=true;
    pixels("empty-scene-created");
    examples->setCurrentIndex(0); tabs->setCurrentIndex(0);
    window.resize(1200,740); QCoreApplication::processEvents(); viewport->resetView();
    qInfo("Creation overall: %d",int(okay));
    return okay;
}
