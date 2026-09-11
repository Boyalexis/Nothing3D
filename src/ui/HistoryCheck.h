#pragma once
#include "ui/DimensionCheck.h"
#include <QAction>
#include <QLineEdit>
#include <QTabWidget>

inline bool checkHistory(MainWindow& window) {
    bool okay=true;
    auto check=[&](bool result,const char* name) { okay &= result; qInfo("History %s: %d",name,int(result)); };
    auto* viewport=window.viewport();
    auto* undo=window.findChild<QAction*>("undo");
    auto* redo=window.findChild<QAction*>("redo");
    auto* width=window.findChild<QDoubleSpinBox*>("dimensionWidth");
    auto* x=window.findChild<QDoubleSpinBox*>("positionX");
    auto* rotation=window.findChild<QDoubleSpinBox*>("rotationY");
    auto* tree=window.findChild<QTreeWidget*>("sceneTree");
    if (!undo || !redo || !width || !x || !rotation || !tree) return false;
    auto key=[&](QObject* target,int code,Qt::KeyboardModifiers modifiers) {
        QKeyEvent event(QEvent::KeyPress,code,modifiers); QCoreApplication::sendEvent(target,&event);
    };
    window.setScene(n3d::initialScene());
    const auto box=window.scene().objects()[0], cylinder=window.scene().objects()[1];
    window.selectObject(box.id);
    check(!undo->isEnabled() && !redo->isEnabled(),"empty history buttons disabled");
    width->setValue(1234); x->setValue(-237); rotation->setValue(41);
    const auto edited=window.scene().find(box.id)->data;
    check(window.history().size()==3 && undo->isEnabled() && !redo->isEnabled(),"dimension, position and rotation create separate commits");
    window.selectObject(cylinder.id); undo->trigger();
    check(window.selectedObject()==box.id && rotation->value()==0 && x->value()==-237 && width->value()==1234,"undo restores edited object selection and one field");
    redo->trigger(); check(window.scene().find(box.id)->data==edited,"redo exact edited state");
    auto* line=rotation->findChild<QLineEdit*>();
    line->setText(QStringLiteral("999"));
    key(line,Qt::Key_Z,Qt::ControlModifier);
    check(rotation->value()==0 && window.history().cursor()==2,"Ctrl+Z discards pending text then undoes committed edit");
    key(line,Qt::Key_Z,Qt::ControlModifier|Qt::ShiftModifier);
    check(window.scene().find(box.id)->data==edited,"Ctrl+Shift+Z redoes from property editor");
    undo->trigger(); key(line,Qt::Key_Y,Qt::ControlModifier);
    check(window.scene().find(box.id)->data==edited,"Ctrl+Y redoes from property editor");
    undo->trigger(); undo->trigger(); undo->trigger();
    check(window.scene().find(box.id)->data==box.data && window.history().cursor()==0,"undo all properties restores baseline");
    const auto count=window.history().size();
    width->setValue(width->value()); window.selectObject(cylinder.id); window.selectObject(box.id);
    check(redo->isEnabled() && window.history().size()==count,"selection and no-op preserve redo");
    x->setValue(600);
    check(!redo->isEnabled() && window.history().size()==1,"new edit replaces redo branch");
    check(window.scene().find(cylinder.id)->data==cylinder.data,"neighbour untouched");
    window.setScene(n3d::initialScene());
    check(!undo->isEnabled() && !redo->isEnabled() && window.history().size()==0,"scene replacement clears history");

    if (viewport) {
        viewport->resetView(); window.selectObject(box.id);
        const auto camera=viewport->camera;
        auto mouse=[&](QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons) {
            QMouseEvent event(type,p,p,button,buttons,Qt::NoModifier); QCoreApplication::sendEvent(viewport,&event);
        };
        // Confirm a placement through the normal viewport mouse path.
        n3d::ObjectData created; created.shape=n3d::CylinderParameters{650,900};
        viewport->beginPlacement(created);
        const auto target=MoveGizmo::project(viewport->modelViewProjection(),{1.7f,0,1},QSizeF(viewport->width(),viewport->height()));
        if (!target) return false;
        mouse(QEvent::MouseMove,*target,Qt::NoButton,Qt::NoButton);
        check(window.history().size()==0 && !undo->isEnabled(),"placement preview has no history");
        mouse(QEvent::MouseButtonPress,*target,Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,*target,Qt::LeftButton,Qt::NoButton);
        if (window.scene().objects().size()!=3) { check(false,"creation committed"); return false; }
        const auto object=window.scene().objects().back();
        check(window.history().size()==1,"one placement is one command");
        key(viewport,Qt::Key_Z,Qt::ControlModifier);
        check(!window.scene().find(object.id) && !viewport->scene().find(object.id) && tree->topLevelItemCount()==2
              && window.selectedObject()==box.id,"undo creation removes both copies and restores previous selection");
        key(viewport,Qt::Key_Y,Qt::ControlModifier);
        check(window.scene().objects().back().id==object.id && window.scene().find(object.id)->data==object.data
              && viewport->scene().find(object.id)->data==object.data && window.selectedObject()==object.id,"redo restores ID, order, parameters and selection");

        window.selectObject(box.id);
        const auto startMove=[&]() -> std::optional<QPointF> {
            const auto layout=viewport->moveGizmo();
            if (!layout) return {};
            const auto world=layout->model.map(MoveGizmo::axis(0)*.55f);
            const QSizeF size(viewport->width(),viewport->height());
            const auto start=MoveGizmo::project(viewport->modelViewProjection(),world,size);
            const auto end=MoveGizmo::project(viewport->modelViewProjection(),world+QVector3D(.35f,0,0),size);
            if (!start || !end) return {};
            mouse(QEvent::MouseButtonPress,*start,Qt::LeftButton,Qt::LeftButton);
            for (int i=1;i<=10;++i) mouse(QEvent::MouseMove,*start+(*end-*start)*(i/10.0),Qt::NoButton,Qt::LeftButton);
            return end;
        };
        if (const auto end=startMove()) {
            check(viewport->isMoving() && window.history().size()==1 && !undo->isEnabled(),"ten drag updates remain one uncommitted gesture");
            window.undo(); check(viewport->isMoving() && window.history().cursor()==1,"history replay ignored during drag");
            mouse(QEvent::MouseButtonRelease,*end,Qt::LeftButton,Qt::NoButton);
        } else check(false,"move starts");
        const auto moved=window.scene().find(box.id)->data;
        check(window.history().size()==2 && moved!=box.data,"release records one move");
        undo->trigger(); check(window.scene().find(box.id)->data==box.data,"one undo restores entire drag");
        if (const auto end=startMove()) {
            viewport->cancelMove(); mouse(QEvent::MouseButtonRelease,*end,Qt::LeftButton,Qt::NoButton);
        }
        check(window.history().size()==2 && window.history().cursor()==1 && redo->isEnabled(),"cancelled drag preserves redo branch");
        redo->trigger();
        check(viewport->scene().find(box.id)->data==moved && window.scene().find(box.id)->data==moved,"redo movement synchronizes scenes");
        viewport->beginPlacement(created);
        check(!undo->isEnabled() && !redo->isEnabled(),"history disabled during placement");
        viewport->cancelPlacement(); check(undo->isEnabled(),"cancelling placement restores history actions");
        check(viewport->camera.yaw==camera.yaw && viewport->camera.distance==camera.distance && viewport->camera.target==camera.target,"history replay preserves camera");
        const bool handles=viewport->showMoveGizmo; viewport->showMoveGizmo=false;
        const auto image=viewport->grab();
        check(hasSceneMesh(image,viewport->modelViewProjection(),Guides::compassRect(image.size(),viewport->devicePixelRatio()),sceneReference(window.scene(),window.selectedObject())),"replayed scene pixels match CPU reference");
        viewport->showMoveGizmo=handles;
        viewport->grab().save(QStringLiteral("build/q6-history-scene.png"));
        window.findChild<QTabWidget*>("ribbonTabs")->setCurrentIndex(0);
        window.findChild<QTabWidget*>("ribbonTabs")->grab().save(QStringLiteral("build/q6-history-toolbar.png"));
        window.setScene(n3d::initialScene()); viewport->resetView();
    }
    qInfo("History overall: %d",int(okay));
    return okay;
}
