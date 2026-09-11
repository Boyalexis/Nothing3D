#pragma once

#include <QMainWindow>
#include <QPointer>
#include "scene/Scene.h"
#include "scene/SceneHistory.h"
class QAction;
class QVulkanInstance;
class VulkanViewport;
class QTreeWidget;
class QLabel;
class QDoubleSpinBox;
class CreationDialog;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QVulkanInstance* instance = nullptr, QWidget* parent = nullptr);
    VulkanViewport* viewport() const { return viewport_; }
    const n3d::Scene& scene() const { return scene_; }
    void setScene(n3d::Scene scene);
    void selectObject(n3d::ObjectId id);
    n3d::ObjectId selectedObject() const { return selectedObject_; }
    const n3d::SceneHistory& history() const { return history_; }
    void undo();
    void redo();
    void duplicateSelected();
    void deleteSelected();
    bool saveProjectTo(const QString& path,QString* error=nullptr);
    bool openProjectFrom(const QString& path,QString* error=nullptr);
    bool hasUnsavedChanges() const { return !history_.isClean(); }
    QString projectPath() const { return projectPath_; }

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched,QEvent* event) override;

private:
    void openProjectDialog();
    bool saveProject(bool saveAs=false);
    bool confirmReplacement();
    void commitPendingProperties();
    void cancelTransientEdit();
    void updateProjectActions();
    QString projectPath_;
    int exampleIndex_=0;
    QAction* openAction_=nullptr;
    QAction* saveAction_=nullptr;
    QAction* saveAsAction_=nullptr;
    void syncScene(n3d::ObjectId selection);
    void updateHistoryActions();
    bool historyBusy() const;
    void replayHistory(bool redo);
    n3d::SceneHistory history_;
    std::optional<n3d::SceneObject> moveBefore_;
    QAction* undoAction_=nullptr;
    QAction* redoAction_=nullptr;
    QAction* duplicateAction_=nullptr;
    QAction* deleteAction_=nullptr;
    void showCreationDialog(bool cylinder);
    void addCreatedObject(n3d::ObjectData object);
    QPointer<CreationDialog> creationDialog_;
    QString rendererMessage_;
    QVulkanInstance* instance_;
    VulkanViewport* viewport_ = nullptr;
    n3d::Scene scene_;
    n3d::ObjectId selectedObject_ = 0;
    QTreeWidget* sceneTree_ = nullptr;
    QLabel* sceneDescription_ = nullptr;
    QWidget* dimensionPanel_ = nullptr;
    QLabel* dimensionLabels_[3]{};
    QDoubleSpinBox* dimensionEditors_[3]{};
    QWidget* transformPanel_ = nullptr;
    QDoubleSpinBox* transformEditors_[6]{};
    void refreshProperties();
    void commitDimension(int field);
    void commitTransform(int field);
    void applyObjectEdit(n3d::ObjectId id, const n3d::ObjectData& edited);
    void buildToolbar();
    void buildWorkspace();
    void applyTheme();
};
