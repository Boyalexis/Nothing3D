#pragma once
#include "ui/HistoryCheck.h"
#include <QToolButton>

inline bool checkObjectActions(MainWindow& window) {
    bool okay=true;
    auto check=[&](bool result,const char* name) { okay &= result; qInfo("ObjectActions %s: %d",name,int(result)); };
    auto* viewport=window.viewport();
    auto* tree=window.findChild<QTreeWidget*>("sceneTree");
    auto* duplicate=window.findChild<QAction*>("duplicateObject");
    auto* remove=window.findChild<QAction*>("deleteObject");
    auto* tabs=window.findChild<QTabWidget*>("ribbonTabs");
    if (!tree || !duplicate || !remove || !tabs) return false;
    auto key=[&](QObject* target,int code,Qt::KeyboardModifiers modifiers=Qt::NoModifier,bool repeat=false) {
        QKeyEvent event(QEvent::KeyPress,code,modifiers,QString(),repeat); QCoreApplication::sendEvent(target,&event);
    };
    auto button=[&](QAction* action) {
        for (auto* item:tabs->findChildren<QToolButton*>()) if (item->defaultAction()==action) { item->click(); return; }
        check(false,"toolbar button exists");
    };
    auto matches=[&] {
        bool result=tree->topLevelItemCount()==int(window.scene().objects().size());
        if (viewport) result &= viewport->scene().objects().size()==window.scene().objects().size() && viewport->selectedObject()==window.selectedObject();
        for (size_t i=0;i<window.scene().objects().size();++i) {
            const auto& object=window.scene().objects()[i];
            auto* item=tree->topLevelItem(int(i));
            result &= item && item->data(0,Qt::UserRole).toULongLong()==object.id && item->text(0)==QString::fromStdString(object.data.name);
            if (viewport) result &= viewport->scene().find(object.id) && viewport->scene().find(object.id)->data==object.data;
        }
        check(result,"scene, render snapshot and ordered tree agree");
    };
    window.setScene(n3d::initialScene()); tabs->setCurrentIndex(1);
    if (viewport) viewport->resetView();
    check(!duplicate->isEnabled() && !remove->isEnabled(),"no selection disables both actions");
    window.duplicateSelected(); window.deleteSelected();
    check(window.history().size()==0 && window.scene().objects().size()==2,"no selection calls are no-op");
    for (int type=0;type<2;++type) {
        auto scene=n3d::initialScene();
        const auto id=scene.objects()[size_t(type)].id;
        auto parameters=scene.find(id)->data;
        parameters.positionMm={123,250,-375}; parameters.rotationDegrees={15,37,-12};
        scene.update(id,parameters); window.setScene(scene); window.selectObject(id);
        check(duplicate->isEnabled() && remove->isEnabled(),"selected object enables actions");
        button(duplicate);
        const auto copy=window.scene().objects().back();
        auto expected=parameters; expected.name+=" 副本";
        check(copy.id>scene.objects().back().id && copy.data==expected && window.selectedObject()==copy.id
              && window.scene().find(id)->data==parameters && window.history().size()==1,"copy preserves parameters with independent ID and selection");
        matches();
        window.findChild<QDoubleSpinBox*>("positionX")->setValue(723);
        check(window.scene().find(id)->data==parameters && window.scene().find(copy.id)->data.positionMm.x==723,"copy edits leave source unchanged");
        window.undo(); window.undo();
        check(!window.scene().find(copy.id) && window.selectedObject()==id,"undo copy removes only copy and selects source");
        window.redo(); check(window.scene().find(copy.id)->data==copy.data,"redo restores exact copy identity");
        window.selectObject(id); key(tree,Qt::Key_D,Qt::ControlModifier);
        const auto second=window.scene().objects().back();
        check(second.id>copy.id && second.data.name==expected.name+" 2" && !window.history().canRedo(),"repeat source copy uses unique name and replaces redo branch");
        key(tree,Qt::Key_D,Qt::ControlModifier,true);
        check(window.scene().objects().back().id==second.id,"key auto-repeat cannot create extra copies");
        const auto beforeDelete=window.history().cursor();
        key(tree,Qt::Key_Delete);
        check(!window.scene().find(second.id) && window.selectedObject()==0 && window.history().cursor()==beforeDelete+1,"Delete removes selected copy and clears selection");
        window.undo(); check(window.scene().find(second.id)->data==second.data && window.selectedObject()==second.id,"undo deletion restores copied object");
        window.redo(); check(!window.scene().find(second.id),"redo deletion removes restored copy");
        matches();
    }

    auto scene=n3d::sceneExample(2); window.setScene(scene);
    const auto middle=scene.objects()[2]; window.selectObject(middle.id); button(remove);
    check(!window.scene().find(middle.id) && window.selectedObject()==0 && !remove->isEnabled(),"toolbar deletion clears selection");
    window.undo();
    check(window.scene().objects()[2].id==middle.id && window.scene().find(middle.id)->data==middle.data && window.selectedObject()==middle.id,"undo restores middle row and exact data");
    matches(); window.redo(); matches();

    n3d::Scene single; n3d::ObjectData data; data.name="Only"; const auto only=single.add(data);
    window.setScene(single); window.selectObject(only);
    auto* line=window.findChild<QDoubleSpinBox*>("dimensionWidth")->findChild<QLineEdit*>();
    const auto beforeText=line->text();
    line->selectAll(); key(line,Qt::Key_Delete);
    check(window.scene().objects().size()==1 && window.scene().find(only)->data==data && window.history().size()==0
          && line->text()!=beforeText,"Delete in input edits text without deleting object");
    window.selectObject(only); window.deleteSelected();
    check(window.scene().objects().empty() && !window.selectedObject() && tree->topLevelItemCount()==0,"last object deletion produces empty scene");
    window.undo(); check(window.scene().find(only)->data==data && window.selectedObject()==only,"undo last deletion restores object");
    window.duplicateSelected(); const auto previousCopy=window.selectedObject(); window.undo();
    window.duplicateSelected(); check(window.selectedObject()>previousCopy && !window.history().canRedo(),"abandoned copy ID never reused");

    if (viewport) {
        window.setScene(n3d::initialScene()); viewport->resetView();
        const auto original=window.scene().objects()[0]; window.selectObject(original.id);
        const auto camera=viewport->camera;
        viewport->showMoveGizmo=false;
        const auto selectedImage=viewport->grab();
        key(viewport,Qt::Key_D,Qt::ControlModifier);
        const auto copy=window.scene().objects().back();
        check(viewport->grab()==selectedImage,"in-place selected copy keeps visible highlight");
        viewport->showMoveGizmo=true;
        window.findChild<QDoubleSpinBox*>("positionX")->setValue(2400);
        matches();
        viewport->showMoveGizmo=false;
        auto pixels=[&](const char* label) {
            const auto image=viewport->grab();
            check(hasSceneMesh(image,viewport->modelViewProjection(),Guides::compassRect(image.size(),viewport->devicePixelRatio()),sceneReference(window.scene(),window.selectedObject())),label);
            image.save(QStringLiteral("build/q6-objects-%1.png").arg(QString::fromLatin1(label)));
        };
        pixels("copy-moved");
        key(viewport,Qt::Key_Delete); check(!viewport->scene().find(copy.id),"viewport Delete removes rendering object");
        pixels("deleted"); window.undo(); pixels("restored");
        viewport->showMoveGizmo=true;
        viewport->beginPlacement(data);
        const auto count=window.scene().objects().size(), cursor=window.history().cursor();
        key(viewport,Qt::Key_Delete); key(viewport,Qt::Key_D,Qt::ControlModifier);
        check(!duplicate->isEnabled() && !remove->isEnabled() && window.scene().objects().size()==count && window.history().cursor()==cursor,"placement blocks copy and deletion");
        viewport->cancelPlacement();
        // Real gizmo gesture stays active and unchanged when Delete/Ctrl+D arrive.
        const auto layout=viewport->moveGizmo();
        if (layout) {
            const auto start=MoveGizmo::project(viewport->modelViewProjection(),layout->model.map(MoveGizmo::axis(1)*.55f),QSizeF(viewport->width(),viewport->height()));
            if (start) {
                QMouseEvent press(QEvent::MouseButtonPress,*start,*start,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier); QCoreApplication::sendEvent(viewport,&press);
                key(viewport,Qt::Key_Delete); key(viewport,Qt::Key_D,Qt::ControlModifier);
                check(viewport->isMoving() && !duplicate->isEnabled() && !remove->isEnabled() && window.scene().objects().size()==count,"drag blocks object actions");
                viewport->cancelMove();
                QMouseEvent release(QEvent::MouseButtonRelease,*start,*start,Qt::LeftButton,Qt::NoButton,Qt::NoModifier); QCoreApplication::sendEvent(viewport,&release);
            } else check(false,"drag point visible");
        } else check(false,"handles visible");
        check(viewport->camera.yaw==camera.yaw && viewport->camera.distance==camera.distance && viewport->camera.target==camera.target,"object operations preserve camera");
        viewport->grab().save(QStringLiteral("build/q6-objects-final.png"));
        tabs->grab().save(QStringLiteral("build/q6-objects-toolbar.png"));
        window.setScene(single); window.selectObject(only); window.deleteSelected();
        check(viewport->scene().objects().empty() && !viewport->moveGizmo(),"empty rendering scene has no stale gizmo");
    }
    window.setScene(n3d::initialScene()); tabs->setCurrentIndex(0);
    if (viewport) viewport->resetView();
    check(!duplicate->isEnabled() && !remove->isEnabled() && window.history().size()==0,"new scene resets object actions and history");
    qInfo("ObjectActions overall: %d",int(okay));
    return okay;
}
