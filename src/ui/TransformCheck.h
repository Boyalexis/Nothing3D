#pragma once
#include "ui/DimensionCheck.h"
#include <QStyleOptionSpinBox>
#include <QScrollBar>

inline bool checkTransforms(MainWindow& window) {
    const char* ids[] = {"positionX","positionY","positionZ","rotationX","rotationY","rotationZ"};
    QDoubleSpinBox* editors[6]{};
    for (int i=0; i<6; ++i) {
        editors[i] = window.findChild<QDoubleSpinBox*>(ids[i]);
        if (!editors[i]) return false;
    }
    auto* panel = window.findChild<QWidget*>("transformPanel");
    auto* dimensions = window.findChild<QDoubleSpinBox*>("dimensionWidth");
    auto* tree = window.findChild<QTreeWidget*>("sceneTree");
    auto* examples = window.findChild<QComboBox*>("sceneExamples");
    auto* scroll = window.findChild<QScrollArea*>("propertyScroll");
    auto* viewport = window.viewport();
    if (!panel || !dimensions || !tree || !examples || !scroll) return false;
    bool okay = true;
    auto check = [&](bool result, const char* name) { okay &= result; qInfo("Transform %s: %d",name,int(result)); };
    auto key = [](QObject* target, int code, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = {}) {
        QKeyEvent press(QEvent::KeyPress,code,modifiers,text);
        QCoreApplication::sendEvent(target,&press);
        QKeyEvent release(QEvent::KeyRelease,code,modifiers,text);
        QCoreApplication::sendEvent(target,&release);
    };
    auto type = [&](QDoubleSpinBox* editor, const QString& text) {
        editor->setFocus(); editor->selectAll();
        key(editor->findChild<QLineEdit*>(),0,Qt::NoModifier,text);
    };
    auto enter = [&](QDoubleSpinBox* editor, const QString& text) { type(editor,text); key(editor,Qt::Key_Return); };
    auto pixels = [&](const char* name) {
        if (!viewport) return;
        const auto image = viewport->grab();
        const auto reference = sceneReference(window.scene(),window.selectedObject());
        const auto corner = Guides::compassRect(image.size(),viewport->devicePixelRatio());
        check(hasSceneMesh(image,viewport->modelViewProjection(),corner,reference),name);
        image.save(QStringLiteral("build/q5-transform-%1.png").arg(QString::fromLatin1(name)));
    };
    window.setScene(n3d::initialScene());
    if (viewport) viewport->resetView();
    const auto box = window.scene().objects()[0].id, cylinder = window.scene().objects()[1].id;
    const auto originalCylinder = window.scene().find(cylinder)->data;
    check(panel->isHidden() && !panel->isEnabled(),"no selection hides and disables transforms");
    editors[0]->setValue(999);
    check(window.scene().find(box)->data.positionMm == n3d::Vector3{},"disabled edit has no effect");
    window.selectObject(box);
    check(editors[0]->value() == 0 && editors[5]->value() == 0,"selection populates transform fields");
    auto* item = tree->topLevelItem(0);
    type(editors[0],QStringLiteral("400.25"));
    check(window.scene().find(box)->data.positionMm.x == 0,"typing waits for commit");
    key(editors[0],Qt::Key_Return);
    enter(editors[1],QStringLiteral("600"));
    type(editors[2],QStringLiteral("-300"));
    QFocusEvent focusOut(QEvent::FocusOut,Qt::TabFocusReason);
    QCoreApplication::sendEvent(editors[2],&focusOut);
    enter(editors[3],QStringLiteral("15")); enter(editors[4],QStringLiteral("-30")); enter(editors[5],QStringLiteral("20"));
    auto changed = window.scene().find(box)->data;
    check(changed.positionMm == n3d::Vector3{400.25f,600,-300} && changed.rotationDegrees == n3d::Vector3{15,-30,20},
          "all six components accept signed values, Return and focus loss");
    check(std::get<n3d::BoxParameters>(changed.shape) == n3d::BoxParameters{}
          && window.scene().find(cylinder)->data == originalCylinder,"dimensions and neighbour preserved");
    const auto origin = SceneGeometry::prepare(*window.scene().find(box)).model.map(QVector3D());
    check((origin-QVector3D(0.40025f,0.6f,-0.3f)).length() < 0.00001f,"rotation leaves translated bottom centre fixed");
    check(window.selectedObject() == box && tree->topLevelItem(0) == item
          && item->toolTip(0).contains(QStringLiteral("400.25")),"selection identity and tooltip synchronized");
    if (viewport) {
        check(viewport->scene().find(box)->data == changed && viewport->selectedObject() == box
              && viewport->camera.yaw == 35 && viewport->camera.distance == 5.5f,"render snapshot and camera preserved");
        int removed = 0, added = 0;
        const auto before = n3d::initialScene();
        for (int y=40; y<viewport->height()-40; y+=20) for (int x=40; x<viewport->width()-40; x+=20) {
            const QSizeF size(viewport->width(),viewport->height()); const QPointF point(x,y);
            const auto oldId = ScenePicking::pick(before,viewport->modelViewProjection(),point,size);
            const auto newId = ScenePicking::pick(viewport->scene(),viewport->modelViewProjection(),point,size);
            removed += oldId == box && newId != box;
            added += oldId != box && newId == box;
        }
        check(removed > 0 && added > 0,"picking follows transformed surface");
    }
    pixels("box-edited");
    for (int field : {0,3}) {
        const auto baseline = window.scene().find(box)->data;
        const auto previous = editors[field]->value();
        for (const auto& invalid : {QStringLiteral("nan"),QStringLiteral("inf"),QStringLiteral(""),QStringLiteral("999999999999")}) {
            editors[field]->findChild<QLineEdit*>()->setText(invalid); key(editors[field],Qt::Key_Return);
            check(window.scene().find(box)->data == baseline && editors[field]->value() == previous,"invalid transform restores previous value");
        }
    }
    enter(editors[0],QStringLiteral("-1000000000")); enter(editors[2],QStringLiteral("1000000000"));
    check(window.scene().find(box)->data.positionMm.x == -1e9f && window.scene().find(box)->data.positionMm.z == 1e9f,"position range endpoints");
    enter(editors[3],QStringLiteral("-360000")); enter(editors[5],QStringLiteral("360000"));
    check(window.scene().find(box)->data.rotationDegrees.x == -360000 && window.scene().find(box)->data.rotationDegrees.z == 360000,"angle range endpoints");
    const float restored[] = {400.25f,600,-300,15,-30,20};
    for (int field=0; field<6; ++field) enter(editors[field],QString::number(restored[field]));
    enter(editors[4],QStringLiteral("450"));
    check(window.scene().find(box)->data.rotationDegrees.y == 450,"multiple turns retained as entered");
    enter(editors[4],QStringLiteral("0"));
    key(editors[4],Qt::Key_Up);
    check(editors[4]->value() == 1,"normal angle step is one degree");
    key(editors[4],Qt::Key_Up,Qt::ControlModifier);
    check(editors[4]->value() == 16,"Ctrl angle step is fifteen degrees without extra Qt multiplier");
    key(editors[4],Qt::Key_Down,Qt::ControlModifier);
    check(editors[4]->value() == 1,"negative Ctrl angle step");
    enter(editors[0],QStringLiteral("-25.25"));
    key(editors[0],Qt::Key_Up,Qt::ControlModifier);
    check(editors[0]->value() == 74.75,"Ctrl position step is one hundred millimetres");
    key(editors[0],Qt::Key_Down);
    check(editors[0]->value() == 73.75,"normal position step is one millimetre");
    // Use actual spin-button events as well as keyboard arrows.
    QStyleOptionSpinBox option; option.initFrom(editors[4]); option.frame = true;
    option.buttonSymbols = QAbstractSpinBox::UpDownArrows;
    option.stepEnabled = QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
    const auto up = editors[4]->style()->subControlRect(QStyle::CC_SpinBox,&option,QStyle::SC_SpinBoxUp,editors[4]).center();
    QMouseEvent down(QEvent::MouseButtonPress,up,up,Qt::LeftButton,Qt::LeftButton,Qt::ControlModifier);
    QMouseEvent release(QEvent::MouseButtonRelease,up,up,Qt::LeftButton,Qt::NoButton,Qt::ControlModifier);
    QCoreApplication::sendEvent(editors[4],&down); QCoreApplication::sendEvent(editors[4],&release);
    qInfo("Transform Ctrl button value=%g, point=(%d,%d), size=%dx%d",editors[4]->value(),up.x(),up.y(),editors[4]->width(),editors[4]->height());
    check(editors[4]->value() == 16,"Ctrl spin button uses same angle step");
    editors[4]->clearFocus();
    QWheelEvent wheel(QPointF(10,10),QPointF(10,10),QPoint(),QPoint(0,120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);
    QCoreApplication::sendEvent(editors[4],&wheel);
    check(editors[4]->value() == 16,"unfocused wheel cannot edit transforms");
    type(editors[0],QStringLiteral("777")); window.selectObject(cylinder); key(editors[0],Qt::Key_Return);
    check(window.scene().find(cylinder)->data == originalCylinder && editors[0]->value() == -1600,
          "pending old input cannot change new selection");
    enter(editors[0],QStringLiteral("-1300")); enter(editors[1],QStringLiteral("300")); enter(editors[2],QStringLiteral("400"));
    enter(editors[5],QStringLiteral("-20"));
    check(window.scene().find(cylinder)->data.positionMm == n3d::Vector3{-1300,300,400}
          && window.scene().find(cylinder)->data.rotationDegrees == n3d::Vector3{0,0,-20},"cylinder has independent transform");
    pixels("cylinder-edited");
    if (viewport) {
        viewport->camera.orbit(10,3); viewport->modelAngle = 18;
        const auto camera = viewport->camera.view();
        enter(editors[4],QStringLiteral("25"));
        check(viewport->camera.view() == camera && viewport->modelAngle == 18 && viewport->selectedObject() == cylinder,
              "transform edit preserves custom camera and preview angle");
        pixels("preview-edited");
        viewport->resetView(); window.resize(960,600); QCoreApplication::processEvents();
        scroll->verticalScrollBar()->setValue(0);
        scroll->grab().save(QStringLiteral("build/q5-transform-panel-top.png"));
        scroll->ensureWidgetVisible(editors[5]);
        scroll->grab().save(QStringLiteral("build/q5-transform-panel-rotation.png"));
        check(scroll->verticalScrollBar()->maximum() > 0,"small window properties remain scrollable");
        pixels("minimum-window");
    }
    type(editors[0],QStringLiteral("888")); window.selectObject(0); key(editors[0],Qt::Key_Return);
    check(!panel->isEnabled() && panel->isHidden(),"deselection disables pending transform");
    examples->setCurrentIndex(3); editors[1]->setValue(700);
    check(window.scene().objects().empty(),"empty scene cannot be edited");
    examples->setCurrentIndex(0); window.selectObject(box);
    check(editors[0]->value() == 0 && editors[5]->value() == 0,"scene replacement resets fields");
    auto preciseScene = window.scene(); auto precise = preciseScene.find(box)->data;
    precise.positionMm = {100.1234f,200.5678f,-300.1234f}; precise.rotationDegrees = {1.1234f,2.5678f,-3.1234f};
    preciseScene.update(box,precise); window.setScene(preciseScene); window.selectObject(box);
    key(editors[0],Qt::Key_Return); key(editors[3],Qt::Key_Return);
    check(window.scene().find(box)->data == precise,"display rounding does not rewrite transforms");
    enter(editors[4],QStringLiteral("20.25"));
    const auto after = window.scene().find(box)->data;
    check(after.positionMm == precise.positionMm && after.rotationDegrees.x == precise.rotationDegrees.x
          && after.rotationDegrees.z == precise.rotationDegrees.z,"one component edit preserves unedited precision");
    enter(dimensions,QStringLiteral("1500"));
    check(window.scene().find(box)->data.positionMm == after.positionMm
          && window.scene().find(box)->data.rotationDegrees == after.rotationDegrees,"dimension edit preserves transform precision");
    window.setScene(n3d::initialScene()); window.selectObject(0);
    if (viewport) { window.resize(1200,740); QCoreApplication::processEvents(); viewport->resetView(); }
    qInfo("Transform overall: %d",int(okay));
    return okay;
}
