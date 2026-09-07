#pragma once

#include <QMainWindow>
class QVulkanInstance;
class VulkanViewport;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QVulkanInstance* instance = nullptr, QWidget* parent = nullptr);
    VulkanViewport* viewport() const { return viewport_; }

private:
    QVulkanInstance* instance_;
    VulkanViewport* viewport_ = nullptr;
    void buildToolbar();
    void buildWorkspace();
    void applyTheme();
};
