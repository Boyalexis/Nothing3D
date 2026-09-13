#pragma once
#include "ui/HistoryCheck.h"
#include "io/SceneFile.h"
#include <QTemporaryDir>
#include <QMessageBox>
#include <QCloseEvent>
#include <QTimer>
#include <QFileDialog>

inline bool checkProject(MainWindow& window) {
    bool okay=true;
    auto check=[&](bool result,const char* label) { okay &= result; qInfo("Project %s: %d",label,int(result)); };
    QTemporaryDir directory;
    if (!directory.isValid()) return false;
    const auto path=directory.filePath(QStringLiteral("工程测试.n3d")), other=directory.filePath("other.n3d");
    QString error;
    auto* viewport=window.viewport();
    auto* width=window.findChild<QDoubleSpinBox*>("dimensionWidth");
    auto* examples=window.findChild<QComboBox*>("sceneExamples");
    auto* tabs=window.findChild<QTabWidget*>("ribbonTabs");
    auto* save=window.findChild<QAction*>("saveProject");
    auto* open=window.findChild<QAction*>("openProject");
    auto* saveAs=window.findChild<QAction*>("saveProjectAs");
    check(save && open && saveAs && tabs->count()==4,"project actions and page exist");
    if (!save || !open || !saveAs) return false;
    auto choice=[&](QMessageBox::StandardButton button,auto action) {
        bool prompted=false; QTimer response;
        QObject::connect(&response,&QTimer::timeout,&window,[&] {
            if (auto* prompt=window.findChild<QMessageBox*>("unsavedChanges"); prompt && prompt->isVisible()) {
                response.stop(); prompted=true; prompt->done(button);
            }
        });
        response.start(1); const bool result=action(); response.stop();
        check(prompted,"unsaved replacement asks for a decision"); return result;
    };
    window.setScene(n3d::initialScene()); const auto box=window.scene().objects()[0].id;
    check(!window.hasUnsavedChanges() && window.projectPath().isEmpty(),"initial baseline clean and unnamed");
    window.selectObject(box); width->setValue(1234.25);
    check(window.hasUnsavedChanges() && window.windowTitle().contains('*'),"editing marks title dirty");
    width->findChild<QLineEdit*>()->setText(QStringLiteral("1450.50"));
    check(window.saveProjectTo(path,&error) && error.isEmpty(),"save commits pending valid input");
    check(std::get<n3d::BoxParameters>(SceneFile::read(path).find(box)->data.shape).width==1450.5f
          && !window.hasUnsavedChanges() && !window.windowTitle().contains('*') && window.projectPath()==path,"saved scene, clean marker and path agree");
    window.undo(); check(window.hasUnsavedChanges(),"undo away from save marks dirty");
    window.redo(); check(!window.hasUnsavedChanges(),"redo returns to saved state");
    window.undo(); width->setValue(1800);
    check(window.hasUnsavedChanges() && !window.history().canRedo(),"branch at old save cursor remains dirty");
    check(!window.saveProjectTo(directory.path(),&error) && !error.isEmpty() && window.hasUnsavedChanges() && window.projectPath()==path,"failed save preserves dirty state and current path");
    check(std::get<n3d::BoxParameters>(SceneFile::read(path).find(box)->data.shape).width==1450.5f,"failed save preserves previous file");
    SceneFile::write(other,n3d::sceneExample(2));
    const auto cursor=window.history().cursor();
    check(!choice(QMessageBox::Cancel,[&] { return window.openProjectFrom(other,&error); })
          && window.scene().objects().size()==2 && window.history().cursor()==cursor && window.projectPath()==path,"cancel opening retains active document");
    const auto broken=directory.filePath("broken.n3d");
    { QFile file(broken); check(file.open(QIODevice::WriteOnly),"invalid fixture opens"); file.write("{\"version\":999}"); }
    check(!window.openProjectFrom(broken,&error) && !error.isEmpty() && window.hasUnsavedChanges()
          && window.history().cursor()==cursor && window.selectedObject()==box,"invalid file preserves data, history and selection");
    check(choice(QMessageBox::Save,[&] { return window.openProjectFrom(path,&error); }),"save then open same file succeeds");
    check(std::get<n3d::BoxParameters>(window.scene().find(box)->data.shape).width==1800
          && !window.hasUnsavedChanges() && window.history().size()==0 && window.selectedObject()==0,"same-file open reads newly saved data and clears history");
    window.selectObject(box); width->setValue(1900);
    check(choice(QMessageBox::Discard,[&] { return window.openProjectFrom(other,&error); }),"discard then open another project succeeds");
    check(window.scene().objects().size()==6 && window.projectPath()==other && !window.hasUnsavedChanges()
          && examples->currentIndex()==-1,"loaded project is clean and independent of examples");
    window.selectObject(window.scene().objects().front().id); width->setValue(990);
    check(!choice(QMessageBox::Cancel,[&] { QCloseEvent event; QCoreApplication::sendEvent(&window,&event); return event.isAccepted(); })
          && window.hasUnsavedChanges(),"cancel closing leaves dirty project open");
    check(choice(QMessageBox::Save,[&] { QCloseEvent event; QCoreApplication::sendEvent(&window,&event); return event.isAccepted(); })
          && !window.hasUnsavedChanges(),"save before closing persists current scene");
    width->setValue(1100);
    check(!choice(QMessageBox::Cancel,[&] { examples->setCurrentIndex(3); return examples->currentIndex()==3; })
          && examples->currentIndex()==-1 && window.scene().objects().size()==6,"cancel example switch restores combo and scene");
    check(choice(QMessageBox::Discard,[&] { examples->setCurrentIndex(3); return examples->currentIndex()==3; })
          && window.scene().objects().empty() && window.projectPath().isEmpty(),"discard allows example replacement");
    check(window.saveProjectTo(path,&error) && SceneFile::read(path).objects().empty(),"empty scene saves and loads");
    window.setScene(n3d::initialScene()); window.selectObject(box); window.duplicateSelected();
    const auto allocated=window.selectedObject(); window.deleteSelected();
    check(window.saveProjectTo(path,&error) && window.openProjectFrom(path,&error),"save after deletion and reload");
    window.selectObject(box); window.duplicateSelected();
    check(window.selectedObject()>allocated,"reloaded allocator never reuses deleted ID");
    check(window.saveProjectTo(other,&error) && window.projectPath()==other && SceneFile::read(path).objects().size()==2
          && SceneFile::read(other).objects().size()==3,"save to another path preserves previous project file");
    window.setScene(n3d::initialScene()); window.selectObject(box); width->setValue(1700);
    {
        bool cancelledDialog=false;
        QTimer cancelSave;
        QObject::connect(&cancelSave,&QTimer::timeout,&window,[&] {
            if (auto* dialog=window.findChild<QFileDialog*>("saveProjectDialog"); dialog && dialog->isVisible()) {
                cancelSave.stop();
                // Let the native dialog finish entering its event loop before
                // simulating a user's single Cancel action.
                QTimer::singleShot(100,dialog,[dialog,&cancelledDialog] { cancelledDialog=true; dialog->reject(); });
            }
        });
        cancelSave.start(1);
        const bool replacement=choice(QMessageBox::Save,[&] { return window.openProjectFrom(path,&error); });
        qInfo("Project cancel-save state: replacement=%d dialog=%d dirty=%d unnamed=%d width=%g",int(replacement),int(cancelledDialog),int(window.hasUnsavedChanges()),int(window.projectPath().isEmpty()),double(std::get<n3d::BoxParameters>(window.scene().find(box)->data.shape).width));
        check(!replacement
              && cancelledDialog && window.hasUnsavedChanges() && window.projectPath().isEmpty()
              && std::get<n3d::BoxParameters>(window.scene().find(box)->data.shape).width==1700,"cancelling first save prevents document replacement");
    }
    if (viewport) {
        window.setScene(n3d::sceneExample(2)); viewport->resetView(); viewport->camera.distance=9;
        const auto snapshot=window.scene();
        check(window.saveProjectTo(path,&error),"GPU scene saved");
        const auto before=viewport->grab();
        const auto savedCamera=viewport->camera;
        window.setScene(n3d::sceneExample(3));
        check(window.openProjectFrom(path,&error),"GPU scene reopened");
        check(viewport->camera.framingRadius>0,"opening scene automatically frames objects");
        viewport->camera=savedCamera;
        const auto after=viewport->grab();
        check(after==before && viewport->scene().objects().size()==snapshot.objects().size(),"saved and reopened scene pixels identical at same camera");
        after.save(QStringLiteral("build/q6-file-reopened.png"));
        window.selectObject(window.scene().objects().front().id);
        viewport->beginPlacement(window.scene().objects().front().data);
        const auto history=window.history().cursor();
        check(!save->isEnabled() && !saveAs->isEnabled() && !open->isEnabled()
              && !window.saveProjectTo(other,&error) && !window.openProjectFrom(path,&error)
              && window.history().cursor()==history,"active placement blocks project operations");
        viewport->cancelPlacement(); check(save->isEnabled() && open->isEnabled(),"file actions recover after cancellation");
        tabs->setCurrentIndex(3); tabs->grab().save(QStringLiteral("build/q6-file-toolbar.png"));
    }
    window.setScene(n3d::initialScene()); tabs->setCurrentIndex(0);
    { const QSignalBlocker blocked(examples); examples->setCurrentIndex(0); }
    if (viewport) viewport->resetView();
    qInfo("Project overall: %d",int(okay)); return okay;
}
