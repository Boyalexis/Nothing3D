#include "ui/MainWindow.h"
#include "ui/CreationDialog.h"
#include "render/VulkanViewport.h"
#include "io/SceneFile.h"
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QPushButton>

void MainWindow::commitPendingProperties()
{
    for (auto* editor:dimensionEditors_) if (editor->isEnabled() && !editor->isHidden()) editor->interpretText();
    for (auto* editor:transformEditors_) if (editor->isEnabled()) editor->interpretText();
}

void MainWindow::cancelTransientEdit()
{
    if (viewport_) { viewport_->cancelMove(); viewport_->cancelPlacement(); }
    if (creationDialog_) creationDialog_->reject();
}

void MainWindow::updateProjectActions()
{
    const bool enabled=!historyBusy();
    if (openAction_) openAction_->setEnabled(enabled);
    if (saveAction_) saveAction_->setEnabled(enabled);
    if (saveAsAction_) saveAsAction_->setEnabled(enabled);
    QString title=projectPath_.isEmpty()?QStringLiteral("未命名"):QFileInfo(projectPath_).fileName();
    if (hasUnsavedChanges()) title+=QStringLiteral(" *");
    setWindowTitle(projectPath_.isEmpty() && !hasUnsavedChanges()?QStringLiteral("Nothing3D"):title+QStringLiteral(" — Nothing3D"));
}

bool MainWindow::saveProjectTo(const QString& path,QString* error)
{
    if (error) error->clear();
    if (historyBusy()) { if (error) *error=QStringLiteral("请先完成或取消当前操作"); return false; }
    commitPendingProperties();
    try { SceneFile::write(path,scene_); }
    catch (const std::exception& problem) { if (error) *error=QString::fromUtf8(problem.what()); return false; }
    projectPath_=QFileInfo(path).absoluteFilePath();
    history_.markClean(); updateHistoryActions();
    statusBar()->showMessage(QStringLiteral("已保存：%1").arg(projectPath_),5000);
    return true;
}

bool MainWindow::saveProject(bool saveAs)
{
    if (historyBusy()) return false;
    QString path=projectPath_;
    if (path.isEmpty() || saveAs) {
        QFileDialog dialog(this,QStringLiteral("保存 Nothing3D 工程"));
        dialog.setObjectName(QStringLiteral("saveProjectDialog"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter(QStringLiteral("Nothing3D 工程 (*.n3d)"));
        // Resolve the extension inside the dialog, before its overwrite check.
        dialog.setDefaultSuffix(QStringLiteral("n3d"));
        dialog.selectFile(path.isEmpty()?QStringLiteral("未命名.n3d"):path);
        if (dialog.exec()!=QDialog::Accepted || dialog.selectedFiles().isEmpty()) return false;
        path=dialog.selectedFiles().front();
    }
    QString error;
    if (saveProjectTo(path,&error)) return true;
    QMessageBox::warning(this,QStringLiteral("保存失败"),error);
    return false;
}

bool MainWindow::confirmReplacement()
{
    commitPendingProperties();
    if (!hasUnsavedChanges()) return true;
    QMessageBox prompt(QMessageBox::Question,QStringLiteral("未保存的修改"),
        QStringLiteral("当前场景有未保存的修改。是否先保存？"),QMessageBox::NoButton,this);
    prompt.setObjectName(QStringLiteral("unsavedChanges"));
    prompt.addButton(QMessageBox::Save)->setText(QStringLiteral("保存"));
    prompt.addButton(QMessageBox::Discard)->setText(QStringLiteral("不保存"));
    prompt.addButton(QMessageBox::Cancel)->setText(QStringLiteral("取消"));
    prompt.setDefaultButton(QMessageBox::Save); prompt.setEscapeButton(QMessageBox::Cancel);
    const auto choice=prompt.exec();
    if (choice==QMessageBox::Save) return saveProject();
    return choice==QMessageBox::Discard;
}

bool MainWindow::openProjectFrom(const QString& path,QString* error)
{
    if (error) error->clear();
    if (historyBusy()) { if (error) *error=QStringLiteral("请先完成或取消当前操作"); return false; }
    n3d::Scene loaded;
    try { loaded=SceneFile::read(path); }
    catch (const std::exception& problem) { if (error) *error=QString::fromUtf8(problem.what()); return false; }
    // Only a completely validated candidate can replace the current project.
    if (!confirmReplacement()) return false;
    // Saving during the prompt may have changed the chosen file (including an
    // Open of this same project). Reload the validated candidate after that save.
    try { loaded=SceneFile::read(path); }
    catch (const std::exception& problem) { if (error) *error=QString::fromUtf8(problem.what()); return false; }
    setScene(std::move(loaded));
    projectPath_=QFileInfo(path).absoluteFilePath();
    auto* examples=findChild<QComboBox*>("sceneExamples");
    const QSignalBlocker blocked(examples);
    examples->setPlaceholderText(QStringLiteral("已打开工程")); examples->setCurrentIndex(-1); exampleIndex_=-1;
    if (viewport_) viewport_->resetView();
    updateHistoryActions();
    statusBar()->showMessage(QStringLiteral("已打开：%1").arg(projectPath_),5000);
    return true;
}

void MainWindow::openProjectDialog()
{
    if (historyBusy()) return;
    const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("打开 Nothing3D 工程"),projectPath_,QStringLiteral("Nothing3D 工程 (*.n3d)"));
    if (path.isEmpty()) return;
    QString error;
    if (!openProjectFrom(path,&error) && !error.isEmpty()) QMessageBox::warning(this,QStringLiteral("打开失败"),error);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    cancelTransientEdit();
    if (!confirmReplacement()) { event->ignore(); return; }
    QMainWindow::closeEvent(event);
}
