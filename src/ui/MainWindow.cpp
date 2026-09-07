#include "ui/MainWindow.h"
#include "render/VulkanViewport.h"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QWidget* createPanel(const QString& title, const QString& message, QWidget* parent = nullptr)
{
    auto* panel = new QFrame(parent);
    panel->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* heading = new QLabel(title, panel);
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    headingFont.setPointSize(11);
    heading->setFont(headingFont);

    auto* hint = new QLabel(message, panel);
    hint->setAlignment(Qt::AlignCenter);
    hint->setObjectName(QStringLiteral("mutedLabel"));

    layout->addWidget(heading);
    layout->addWidget(hint, 1);
    return panel;
}

} // namespace

MainWindow::MainWindow(QVulkanInstance* instance, QWidget* parent)
    : QMainWindow(parent), instance_(instance)
{
    setWindowTitle(QStringLiteral("Nothing3D"));
    resize(1280, 720);
    setMinimumSize(960, 600);

    buildWorkspace();
    buildToolbar();
    applyTheme();

    statusBar()->showMessage(QStringLiteral("正在初始化 Vulkan…"));
}



void MainWindow::buildWorkspace()
{
    auto* workspace = new QSplitter(Qt::Horizontal, this);
    workspace->setChildrenCollapsible(false);

    auto* scenePanel = new QFrame(workspace);
    scenePanel->setFrameShape(QFrame::StyledPanel);
    auto* sceneLayout = new QVBoxLayout(scenePanel);
    sceneLayout->setContentsMargins(12, 12, 12, 12);

    auto* sceneTitle = new QLabel(QStringLiteral("场景对象"), scenePanel);
    QFont titleFont = sceneTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    sceneTitle->setFont(titleFont);

    auto* sceneTree = new QTreeWidget(scenePanel);
    sceneTree->setHeaderHidden(true);
    sceneTree->setRootIsDecorated(false);
    sceneTree->setAlternatingRowColors(true);
    sceneTree->addTopLevelItem(new QTreeWidgetItem({QStringLiteral("Q3 演示长方体（只读）")}));
    sceneTree->topLevelItem(0)->setDisabled(true);
    sceneTree->addTopLevelItem(new QTreeWidgetItem({QStringLiteral("演示圆柱体（只读）")}));
    sceneTree->topLevelItem(1)->setDisabled(true);

    sceneLayout->addWidget(sceneTitle);
    sceneLayout->addWidget(sceneTree, 1);

    QWidget* viewport = nullptr;
    if (instance_) {
        auto* renderWindow = new VulkanViewport;
        viewport_ = renderWindow;
        renderWindow->setObjectName(QStringLiteral("vulkanViewport"));
        renderWindow->setVulkanInstance(instance_);
        const auto devices = renderWindow->availablePhysicalDevices();
        const bool preferIntegrated = qApp->arguments().contains(QStringLiteral("--integrated"));
        for (int i = 0; i < devices.size(); ++i) {
            const auto desired = preferIntegrated ? VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
                                                  : VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            if (devices[i].deviceType == desired) {
                renderWindow->setPhysicalDeviceIndex(i);
                break;
            }
        }
        connect(renderWindow, &VulkanViewport::rendererStatus, this,
                [this](const QString& text) { statusBar()->showMessage(text); });
        viewport = QWidget::createWindowContainer(renderWindow, workspace);
        viewport->setMinimumSize(320, 240);
    } else {
        viewport = createPanel(QStringLiteral("三维视图"), QStringLiteral("无显卡界面测试"), workspace);
    }

    auto* properties = createPanel(
        QStringLiteral("演示说明"), QStringLiteral("长方体：2000 × 1500 × 1000 mm\n圆柱体：直径 800 / 高 1400 mm\n圆柱底心：(-1600, 0, 0) mm\n\n中键：旋转视角\nShift＋中键：平移\n滚轮：缩放\nF：重置视图\n空格：演示组旋转 / 暂停\n\n本阶段暂不支持选择和尺寸编辑"), workspace);

    workspace->addWidget(scenePanel);
    workspace->addWidget(viewport);
    workspace->addWidget(properties);
    workspace->setStretchFactor(0, 0);
    workspace->setStretchFactor(1, 1);
    workspace->setStretchFactor(2, 0);
    workspace->setSizes({220, 800, 260});

    setCentralWidget(workspace);
}

void MainWindow::applyTheme()
{
    qApp->setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget {
            background-color: #151a22;
            color: #dbe2ea;
        }
        QMenuBar, QMenu, QToolBar, QStatusBar {
            background-color: #1d2430;
            color: #dbe2ea;
        }
        QMenuBar::item:selected, QMenu::item:selected, QToolButton:hover {
            background-color: #2d3948;
        }
        QFrame[frameShape="6"] {
            border: 1px solid #303a47;
        }
        QTreeWidget {
            background-color: #11161d;
            border: 1px solid #303a47;
            alternate-background-color: #171e27;
        }
        QSplitter::handle {
            background-color: #303a47;
            width: 1px;
        }
        QLabel#mutedLabel {
            color: #7f8b99;
        }
        QFrame#viewportPlaceholder {
            background-color: #0c1117;
        }
    )"));
}
