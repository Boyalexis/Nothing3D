#include "ui/MainWindow.h"
#include "render/VulkanViewport.h"
#include "scene/SceneExamples.h"
#include "ui/DimensionSpinBox.h"
#include "ui/TransformSpinBox.h"
#include "ui/CreationDialog.h"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QStatusBar>
#include <QSignalBlocker>
#include <QFormLayout>
#include <QScrollArea>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QString objectDescription(const n3d::ObjectData& data)
{
    QString dimensions;
    if (const auto* box = std::get_if<n3d::BoxParameters>(&data.shape)) {
        dimensions = QStringLiteral("宽 %1 / 高 %2 / 深 %3").arg(box->width).arg(box->height).arg(box->depth);
    } else {
        const auto& cylinder = std::get<n3d::CylinderParameters>(data.shape);
        dimensions = QStringLiteral("直径 %1 / 高 %2").arg(cylinder.diameter).arg(cylinder.height);
    }
    const auto p = data.positionMm, r = data.rotationDegrees;
    return QString::fromStdString(data.name) + QStringLiteral("\n")
        + dimensions + QStringLiteral(" mm\n")
        + QStringLiteral("位置 (%1, %2, %3) mm\n旋转 (%4, %5, %6)°")
            .arg(p.x).arg(p.y).arg(p.z).arg(r.x).arg(r.y).arg(r.z);
}

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
    setScene(n3d::initialScene());
    buildToolbar();
    applyTheme();
    auto* clearSelection = new QAction(this);
    clearSelection->setShortcut(QKeySequence(Qt::Key_Escape));
    addAction(clearSelection);
    connect(clearSelection, &QAction::triggered, this, [this] {
        if (viewport_ && viewport_->isPlacing()) viewport_->cancelPlacement();
        else selectObject(0);
    });

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
    sceneTree_ = sceneTree;
    sceneTree->setObjectName(QStringLiteral("sceneTree"));
    sceneTree->setHeaderHidden(true);
    sceneTree->setRootIsDecorated(false);
    sceneTree->setAlternatingRowColors(true);
    sceneTree->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(sceneTree, &QTreeWidget::itemSelectionChanged, this, [this] {
        const auto items = sceneTree_->selectedItems();
        selectObject(items.isEmpty() ? 0 : items.front()->data(0, Qt::UserRole).toULongLong());
    });

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
                [this](const QString& text) { rendererMessage_ = text; if (!viewport_->isPlacing()) statusBar()->showMessage(text); });
        connect(renderWindow,&VulkanViewport::placementStatus,this,[this](const QString& text) { statusBar()->showMessage(text); });
        connect(renderWindow,&VulkanViewport::placementChanged,this,[this](bool active) {
            if (!active) statusBar()->showMessage(rendererMessage_);
        });
        connect(renderWindow,&VulkanViewport::objectPlaced,this,[this](const n3d::ObjectData& object) { addCreatedObject(object); });
        connect(renderWindow, &VulkanViewport::selectionChanged, this,
                [this](quint64 id) { selectObject(id); });
        viewport = QWidget::createWindowContainer(renderWindow, workspace);
        viewport->setMinimumSize(320, 240);
    } else {
        viewport = createPanel(QStringLiteral("三维视图"), QStringLiteral("无显卡界面测试"), workspace);
    }

    auto* propertyScroll = new QScrollArea(workspace);
    propertyScroll->setObjectName(QStringLiteral("propertyScroll"));
    propertyScroll->setWidgetResizable(true);
    propertyScroll->setMinimumWidth(240);
    auto* properties = new QFrame;
    propertyScroll->setWidget(properties);
    properties->setFrameShape(QFrame::StyledPanel);
    auto* propertyLayout = new QVBoxLayout(properties);
    propertyLayout->setContentsMargins(12, 12, 12, 12);
    auto* title = new QLabel(QStringLiteral("场景示例"), properties);
    title->setFont(titleFont);
    propertyLayout->addWidget(title);
    auto* examples = new QComboBox(properties);
    examples->setObjectName(QStringLiteral("sceneExamples"));
    examples->addItems({QStringLiteral("初始场景 · 2 个对象"), QStringLiteral("参数变化 · 仅修改长方体"),
                       QStringLiteral("多个对象 · 6 个基本体"), QStringLiteral("空场景")});
    propertyLayout->addWidget(examples);
    auto* selectedTitle = new QLabel(QStringLiteral("对象参数"), properties);
    selectedTitle->setFont(titleFont);
    propertyLayout->addWidget(selectedTitle);
    sceneDescription_ = new QLabel(properties);
    sceneDescription_->setObjectName(QStringLiteral("sceneDescription"));
    sceneDescription_->setWordWrap(true);
    sceneDescription_->setTextFormat(Qt::PlainText);
    propertyLayout->addWidget(sceneDescription_);
    dimensionPanel_ = new QWidget(properties);
    dimensionPanel_->setObjectName(QStringLiteral("dimensionPanel"));
    auto* dimensions = new QFormLayout(dimensionPanel_);
    dimensions->setContentsMargins(0, 4, 0, 4);
    dimensions->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    dimensions->addRow(new QLabel(QStringLiteral("尺寸"),dimensionPanel_));
    const char* editorIds[] = {"dimensionWidth", "dimensionHeight", "dimensionDepth"};
    for (int field=0; field<3; ++field) {
        auto* editor = new DimensionSpinBox(dimensionPanel_);
        editor->setObjectName(QString::fromLatin1(editorIds[field]));
        dimensionEditors_[field] = editor;
        dimensionLabels_[field] = new QLabel(dimensionPanel_);
        dimensionLabels_[field]->setBuddy(editor);
        dimensions->addRow(dimensionLabels_[field], editor);
        // Arrow buttons and focused-wheel changes commit immediately; typing
        // is deferred by keyboardTracking=false until Return or focus loss.
        connect(editor, &QDoubleSpinBox::valueChanged, this, [this,field] { commitDimension(field); });
    }
    propertyLayout->addWidget(dimensionPanel_);
    transformPanel_ = new QWidget(properties);
    transformPanel_->setObjectName(QStringLiteral("transformPanel"));
    auto* transforms = new QFormLayout(transformPanel_);
    transforms->setContentsMargins(0,0,0,4);
    transforms->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    const char* transformIds[] = {"positionX","positionY","positionZ","rotationX","rotationY","rotationZ"};
    const QString axes[] = {QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")};
    for (int field=0; field<6; ++field) {
        if (field == 0 || field == 3)
            transforms->addRow(new QLabel(field == 0 ? QStringLiteral("位置 · 底面中心") : QStringLiteral("旋转"),transformPanel_));
        auto* editor = new TransformSpinBox(field >= 3,transformPanel_);
        editor->setObjectName(QString::fromLatin1(transformIds[field]));
        editor->setAccessibleName((field < 3 ? QStringLiteral("位置 ") : QStringLiteral("旋转 ")) + axes[field%3]);
        transformEditors_[field] = editor;
        auto* axis = new QLabel(axes[field%3],transformPanel_);
        axis->setBuddy(editor);
        transforms->addRow(axis,editor);
        connect(editor,&QDoubleSpinBox::valueChanged,this,[this,field] { commitTransform(field); });
    }
    propertyLayout->addWidget(transformPanel_);
    auto* instructions = new QLabel(QStringLiteral(
        "回车或移开焦点应用；无效输入恢复\nCtrl 步进：位置 100 mm / 旋转 15°\n\n"
        "左键单击选择 / 拖动导航\n中键旋转 / Shift＋中键平移\n滚轮缩放 / 空白单击或 Esc 取消选择\n"
        "F 重置视图 / 空格切换预览旋转"), properties);
    instructions->setWordWrap(true);
    instructions->setObjectName(QStringLiteral("mutedLabel"));
    propertyLayout->addWidget(instructions);
    propertyLayout->addStretch();
    connect(examples, &QComboBox::currentIndexChanged, this, [this](int index) {
        setScene(n3d::sceneExample(index));
        if (viewport_) {
            viewport_->resetView();
            if (index == 2) {
                viewport_->camera.distance = 9;
                viewport_->camera.target = QVector3D(0, 0.75f, -0.7f);
                viewport_->requestUpdate();
            }
        }
    });

    workspace->addWidget(scenePanel);
    workspace->addWidget(viewport);
    workspace->addWidget(propertyScroll);
    workspace->setStretchFactor(0, 0);
    workspace->setStretchFactor(1, 1);
    workspace->setStretchFactor(2, 0);
    workspace->setSizes({220, 800, 260});

    setCentralWidget(workspace);
}

void MainWindow::setScene(n3d::Scene scene)
{
    if (creationDialog_) creationDialog_->reject();
    scene_ = std::move(scene);
    const QSignalBlocker blocked(sceneTree_);
    if (viewport_) viewport_->setScene(scene_);
    sceneTree_->clear();
    for (const auto& object : scene_.objects()) {
        const auto name = QString::fromStdString(object.data.name);
        auto* item = new QTreeWidgetItem({name});
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(object.id));
        sceneTree_->addTopLevelItem(item);
        item->setToolTip(0, objectDescription(object.data));
    }
    selectObject(0);
}

void MainWindow::selectObject(n3d::ObjectId id)
{
    if (viewport_) viewport_->cancelPlacement();
    if (!scene_.find(id)) id = 0;
    selectedObject_ = id;
    const QSignalBlocker blocked(sceneTree_);
    QTreeWidgetItem* selected = nullptr;
    for (int i=0; i<sceneTree_->topLevelItemCount(); ++i) {
        auto* item = sceneTree_->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toULongLong() == id) selected = item;
    }
    sceneTree_->clearSelection();
    sceneTree_->setCurrentItem(selected);
    if (selected) {
        selected->setSelected(true);
        sceneTree_->scrollToItem(selected);
        sceneDescription_->setText(QString::fromStdString(scene_.find(id)->data.name));
    } else {
        sceneDescription_->setText(scene_.objects().empty()
            ? QStringLiteral("当前场景没有对象。\n切换示例可重新显示模型。")
            : QStringLiteral("未选中对象\n\n点击模型或左侧名称查看参数。"));
    }
    if (viewport_ && viewport_->selectedObject() != id) viewport_->selectObject(id);
    refreshProperties();
}

void MainWindow::showCreationDialog(bool cylinder)
{
    if (!viewport_) return;
    viewport_->cancelPlacement();
    if (creationDialog_) creationDialog_->reject();
    auto* dialog = new CreationDialog(cylinder,this);
    creationDialog_ = dialog;
    connect(dialog,&QDialog::accepted,this,[this,dialog] { viewport_->beginPlacement(dialog->objectData()); });
    dialog->open();
}

void MainWindow::addCreatedObject(n3d::ObjectData object)
{
    const QString base = std::holds_alternative<n3d::BoxParameters>(object.shape) ? QStringLiteral("长方体 ") : QStringLiteral("圆柱体 ");
    int suffix = 1;
    do {
        object.name = (base+QString::number(suffix++)).toStdString();
    } while (std::any_of(scene_.objects().begin(),scene_.objects().end(),[&](const auto& existing) { return existing.data.name == object.name; }));
    const auto id = scene_.add(object);
    // Keep the existing tree rows and camera, then select the newly assigned ID.
    auto* item = new QTreeWidgetItem({QString::fromStdString(object.name)});
    item->setData(0,Qt::UserRole,QVariant::fromValue<qulonglong>(id));
    item->setToolTip(0,objectDescription(object));
    sceneTree_->addTopLevelItem(item);
    viewport_->setScene(scene_);
    selectObject(id);
}

void MainWindow::refreshProperties()
{
    const auto* object = scene_.find(selectedObject_);
    // Block all fields before hiding or changing focus: a focus-out signal
    // from the previous object must not write into the next selection.
    std::vector<QSignalBlocker> blockers;
    blockers.reserve(9);
    for (auto* editor : dimensionEditors_) blockers.emplace_back(editor);
    for (auto* editor : transformEditors_) blockers.emplace_back(editor);
    dimensionPanel_->setEnabled(object != nullptr);
    dimensionPanel_->setVisible(object != nullptr);
    transformPanel_->setEnabled(object != nullptr);
    transformPanel_->setVisible(object != nullptr);
    if (!object) return;
    const auto* box = std::get_if<n3d::BoxParameters>(&object->data.shape);
    const auto* cylinder = std::get_if<n3d::CylinderParameters>(&object->data.shape);
    dimensionLabels_[0]->setText(box ? QStringLiteral("宽") : QStringLiteral("直径"));
    dimensionLabels_[1]->setText(QStringLiteral("高"));
    dimensionLabels_[2]->setText(QStringLiteral("深"));
    dimensionEditors_[0]->setAccessibleName(box ? QStringLiteral("宽") : QStringLiteral("直径"));
    dimensionEditors_[1]->setAccessibleName(QStringLiteral("高"));
    dimensionEditors_[2]->setAccessibleName(QStringLiteral("深"));
    dimensionEditors_[0]->setValue(box ? box->width : cylinder->diameter);
    dimensionEditors_[1]->setValue(box ? box->height : cylinder->height);
    dimensionEditors_[2]->setValue(box ? box->depth : 1);
    dimensionEditors_[2]->setVisible(box != nullptr);
    dimensionLabels_[2]->setVisible(box != nullptr);
    const auto p = object->data.positionMm, r = object->data.rotationDegrees;
    const float values[] = {p.x,p.y,p.z,r.x,r.y,r.z};
    for (int field=0; field<6; ++field) transformEditors_[field]->setValue(values[field]);
}

void MainWindow::commitDimension(int field)
{
    const auto* object = scene_.find(selectedObject_);
    if (!object || !dimensionPanel_->isEnabled()) return;
    const auto id = object->id;
    auto edited = object->data;
    const float value = float(dimensionEditors_[field]->value());
    if (auto* box = std::get_if<n3d::BoxParameters>(&edited.shape)) {
        if (field == 0) box->width = value;
        else if (field == 1) box->height = value;
        else box->depth = value;
    } else {
        auto& cylinder = std::get<n3d::CylinderParameters>(edited.shape);
        if (field == 0) cylinder.diameter = value;
        else if (field == 1) cylinder.height = value;
        else return;
    }
    applyObjectEdit(id,edited);
}

void MainWindow::commitTransform(int field)
{
    const auto* object = scene_.find(selectedObject_);
    if (!object || !transformPanel_->isEnabled()) return;
    auto edited = object->data;
    auto& vector = field < 3 ? edited.positionMm : edited.rotationDegrees;
    const float value = float(transformEditors_[field]->value());
    if (field%3 == 0) vector.x = value;
    else if (field%3 == 1) vector.y = value;
    else vector.z = value;
    applyObjectEdit(object->id,edited);
}

void MainWindow::applyObjectEdit(n3d::ObjectId id, const n3d::ObjectData& edited)
{
    const auto* object = scene_.find(id);
    if (!object || edited == object->data) return;
    try {
        scene_.update(id, edited);
    } catch (const std::invalid_argument&) {
        refreshProperties();
        return;
    }
    if (viewport_) viewport_->updateObject(id, edited);
    // Keep tree identity, selection, camera and preview rotation intact.
    for (int i=0; i<sceneTree_->topLevelItemCount(); ++i) {
        auto* item = sceneTree_->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toULongLong() == id)
            item->setToolTip(0, objectDescription(edited));
    }
    sceneDescription_->setText(QString::fromStdString(edited.name));
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
        QTreeWidget::item:selected {
            background-color: #665326;
            color: #fff1c4;
        }
        QDoubleSpinBox {
            background-color: #11161d;
            color: #dbe2ea;
            border: 1px solid #354259;
            border-radius: 3px;
            padding: 3px;
        }
        QDoubleSpinBox:focus {
            border-color: #5c9cff;
        }
        QDoubleSpinBox:disabled {
            color: #687385;
        }
        QScrollBar:vertical {
            background: #151a22;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #435269;
            min-height: 24px;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: #151a22;
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
