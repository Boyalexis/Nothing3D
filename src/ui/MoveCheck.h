#pragma once
#include "ui/DimensionCheck.h"
#include <QAction>
#include <QWheelEvent>

inline bool checkMove(MainWindow& window) {
    auto* viewport=window.viewport();
    if (!viewport || !viewport->supportsGrab()) return false;
    bool okay=true;
    auto check=[&](bool result,const char* name) { okay &= result; qInfo("Move %s: %d",name,int(result)); };
    auto mouse=[&](QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
        QMouseEvent event(type,p,p,button,buttons,modifiers); QCoreApplication::sendEvent(viewport,&event);
    };
    auto key=[&](QEvent::Type type,int code,Qt::KeyboardModifiers modifiers=Qt::NoModifier) {
        QKeyEvent event(type,code,modifiers); QCoreApplication::sendEvent(viewport,&event);
    };
    auto* toggle=window.findChild<QAction*>("showMoveGizmo");
    const char* fields[]={"positionX","positionY","positionZ"};
    window.setScene(n3d::initialScene()); viewport->resetView();
    check(!viewport->moveGizmo(),"no handles without selection");
    const auto box=window.scene().objects()[0].id, cylinder=window.scene().objects()[1].id;
    window.selectObject(box);
    // Verify actual overlay colours along all three projected shafts, including
    // the green shaft where the solid would otherwise occlude it.
    const auto image=viewport->grab();
    const auto handles=viewport->moveGizmo();
    check(handles.has_value(),"selection shows handles");
    if (!handles) return false;
    const QColor colors[]={QColor(255,56,56),QColor(64,255,97),QColor(71,148,255)};
    for (int axis=0;axis<3;++axis) {
        int hits=0;
        for (float t : {.35f,.5f,.65f,.85f}) {
            const auto world=handles->model.map(MoveGizmo::axis(axis)*t);
            const auto p=MoveGizmo::project(viewport->modelViewProjection(),world,QSizeF(viewport->width(),viewport->height()));
            if (!p) continue;
            const QPoint pixel=(*p*viewport->devicePixelRatio()).toPoint();
            bool found=false;
            for (int y=-3;y<=3;++y) for (int x=-3;x<=3;++x) {
                const auto at=pixel+QPoint(x,y);
                if (!image.rect().contains(at)) continue;
                const auto color=image.pixelColor(at), expected=colors[axis];
                found |= std::abs(color.red()-expected.red())<12 && std::abs(color.green()-expected.green())<12 && std::abs(color.blue()-expected.blue())<12;
            }
            hits+=int(found);
        }
        check(hits>=3,"RGB shaft and arrow pixels present over scene");
    }
    image.save(QStringLiteral("build/q5-move-handles.png"));
    toggle->setChecked(false); check(!viewport->moveGizmo() && viewport->grab()!=image,"display toggle hides overlay");
    toggle->setChecked(true);

    QPointF start,end;
    auto begin=[&](int axis,float deltaMm) {
        const auto layout=viewport->moveGizmo();
        if (!layout || !layout->visible[axis]) { check(false,"drag axis visible"); return false; }
        const auto vp=viewport->modelViewProjection();
        const QSizeF size(viewport->width(),viewport->height());
        const auto world=layout->model.map(MoveGizmo::axis(axis)*.55f);
        const auto a=MoveGizmo::project(vp,world,size), b=MoveGizmo::project(vp,world+MoveGizmo::axis(axis)*(deltaMm/1000),size);
        if (!a || !b) { check(false,"drag points project"); return false; }
        start=*a; end=*b;
        mouse(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        check(viewport->isMoving() && viewport->activeMoveAxis()==axis,"arrow press captures axis");
        return viewport->isMoving();
    };
    auto move=[&] { mouse(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton); };
    auto release=[&] { mouse(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton); };
    for (const auto id : {box,cylinder}) for (int axis=0;axis<3;++axis) {
        window.selectObject(id);
        const auto original=window.scene().find(id)->data;
        const auto neighbour=window.scene().find(id==box ? cylinder:box)->data;
        const auto camera=viewport->camera;
        if (!begin(axis,237)) continue;
        check(window.scene().find(id)->data==original,"press has no position jump");
        move();
        const auto moved=window.scene().find(id)->data;
        const float before[]={original.positionMm.x,original.positionMm.y,original.positionMm.z};
        const float after[]={moved.positionMm.x,moved.positionMm.y,moved.positionMm.z};
        bool position=true;
        for (int i=0;i<3;++i) position &= std::abs(after[i]-before[i]-(i==axis?237:0))<.2f;
        check(position && viewport->scene().find(id)->data==moved,"known world-axis displacement synchronizes both scenes");
        check(std::abs(window.findChild<QDoubleSpinBox*>(fields[axis])->value()-after[axis])<.02,"property follows drag live");
        check(window.selectedObject()==id && moved.shape==original.shape && moved.rotationDegrees==original.rotationDegrees
              && window.scene().find(id==box?cylinder:box)->data==neighbour,"selection, shape, rotation and neighbour unchanged");
        QWheelEvent wheel(end,end,QPoint(),QPoint(0,120),Qt::LeftButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(viewport,&wheel);
        check(viewport->camera.yaw==camera.yaw && viewport->camera.pitch==camera.pitch && viewport->camera.distance==camera.distance,"drag and wheel do not navigate camera");
        key(QEvent::KeyPress,Qt::Key_Control,Qt::ControlModifier);
        const auto snapped=window.scene().find(id)->data.positionMm;
        const float value=axis==0?snapped.x:axis==1?snapped.y:snapped.z;
        check(value==std::round((before[axis]+237)/100)*100,"Ctrl snaps absolute signed coordinate without mouse motion");
        key(QEvent::KeyRelease,Qt::Key_Control);
        check(window.scene().find(id)->data==moved,"Ctrl release returns to free displacement");
        release();
        check(!viewport->isMoving() && window.scene().find(id)->data==moved && window.findChild<QDoubleSpinBox*>(fields[axis])->isEnabled(),"release retains position and unlocks editor");
    }
    // Compare moved solids against the independent CPU geometry reference.
    toggle->setChecked(false);
    const auto movedImage=viewport->grab();
    check(hasSceneMesh(movedImage,viewport->modelViewProjection(),Guides::compassRect(movedImage.size(),viewport->devicePixelRatio()),sceneReference(window.scene(),window.selectedObject())),"moved solid pixels match scene data");
    toggle->setChecked(true);
    viewport->grab().save(QStringLiteral("build/q5-move-result.png"));
    window.findChild<QWidget*>("propertyScroll")->grab().save(QStringLiteral("build/q5-move-properties.png"));

    window.selectObject(box);
    const auto original=window.scene().find(box)->data;
    if (begin(0,315)) {
        move(); key(QEvent::KeyPress,Qt::Key_Escape);
        const auto camera=viewport->camera;
        mouse(QEvent::MouseMove,end+QPointF(35,20),Qt::NoButton,Qt::LeftButton); release();
        check(!viewport->isMoving() && window.scene().find(box)->data==original && window.selectedObject()==box,"Escape restores snapshot and keeps selection after release");
        check(viewport->camera.yaw==camera.yaw && viewport->camera.pitch==camera.pitch,"cancelled press cannot become camera drag");
    }
    if (begin(1,315)) {
        move(); QFocusEvent lost(QEvent::FocusOut); QCoreApplication::sendEvent(viewport,&lost);
        check(!viewport->isMoving() && window.scene().find(box)->data==original,"focus loss cancels move"); release();
    }
    if (begin(2,315)) {
        move(); window.selectObject(cylinder);
        check(!viewport->isMoving() && window.scene().find(box)->data==original && window.selectedObject()==cylinder,"selection switch restores old object"); release();
    }
    window.selectObject(box);
    if (begin(0,315)) {
        move(); mouse(QEvent::MouseButtonPress,end,Qt::RightButton,Qt::LeftButton|Qt::RightButton); release();
        check(!viewport->isMoving() && window.scene().find(box)->data==original,"right click restores snapshot");
    }
    if (begin(0,315)) {
        move(); toggle->setChecked(false); release();
        check(!viewport->isMoving() && window.scene().find(box)->data==original,"hiding handles cancels move"); toggle->setChecked(true);
    }
    if (begin(0,315)) {
        move(); window.resize(1100,700); QCoreApplication::processEvents(); release();
        check(!viewport->isMoving() && window.scene().find(box)->data==original,"resize cancels move");
    }
    viewport->resetView(); viewport->modelAngle=30;
    window.findChild<QDoubleSpinBox*>("rotationY")->setValue(41);
    const auto rotated=window.scene().find(box)->data;
    if (begin(0,250)) {
        move(); release();
        const auto after=window.scene().find(box)->data;
        check(std::abs(after.positionMm.x-rotated.positionMm.x-250)<.2f && after.positionMm.y==rotated.positionMm.y
              && after.positionMm.z==rotated.positionMm.z && after.rotationDegrees==rotated.rotationDegrees && viewport->modelAngle==30,"object and preview rotations preserve world-axis movement");
    }
    if (begin(0,315)) {
        move(); viewport->beginPlacement(original);
        check(!viewport->isMoving() && !viewport->moveGizmo(),"creation cancels drag and hides handles");
        viewport->cancelPlacement(); release();
    }
    if (begin(0,315)) {
        move(); window.setScene(n3d::sceneExample(2)); release();
        check(!viewport->isMoving() && window.scene().objects().size()==6 && window.selectedObject()==0,"scene replacement cannot receive stale move");
    }
    window.setScene(n3d::initialScene()); viewport->resetView(); window.resize(1200,740); QCoreApplication::processEvents();
    qInfo("Move overall: %d",int(okay));
    return okay;
}
