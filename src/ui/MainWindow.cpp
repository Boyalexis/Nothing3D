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
#include <QLineEdit>
#include <QKeyEvent>
#include <QToolButton>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QShortcut>
#include "ui/RibbonIcons.h"
#include <QMessageBox>

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
    resize(1448, 1000);
    setMinimumSize(960, 600);

    buildWorkspace();
    setScene(n3d::initialScene());
    buildToolbar();
    applyTheme();
    auto* clearSelection = new QAction(this);
    clearSelection->setShortcut(QKeySequence(Qt::Key_Escape));
    addAction(clearSelection);
    connect(clearSelection, &QAction::triggered, this, [this] {
        if (viewport_ && viewport_->isMoving()) viewport_->cancelMove();
        else if (viewport_ && viewport_->isPlacing()) viewport_->cancelPlacement();
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
    sceneLayout->setContentsMargins(6, 0, 6, 6);
    sceneLayout->setSpacing(5);

    auto* sceneTitle = new QLabel(QStringLiteral("项目浏览器"), scenePanel);
    QFont titleFont = sceneTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    sceneTitle->setFont(titleFont);
    sceneTitle->setObjectName("panelHeading");

    auto* sceneTree = new QTreeWidget(scenePanel);
    sceneTree_ = sceneTree;
    sceneTree->installEventFilter(this);
    sceneTree->viewport()->installEventFilter(this);
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
    auto* search = new QLineEdit(scenePanel);
    search->setObjectName("projectSearch");
    search->setPlaceholderText(QStringLiteral("在项目中搜索…"));
    search->setClearButtonEnabled(true);
    sceneLayout->addWidget(search);
    auto* project = new QLabel(QStringLiteral("▾  Demo_Plant / 模型"));
    project->setObjectName("sectionHeading");
    sceneLayout->addWidget(project);
    sceneLayout->addWidget(sceneTree, 1);
    auto filter = [this,search] {
        for (int i=0;i<sceneTree_->topLevelItemCount();++i) {
            auto* item=sceneTree_->topLevelItem(i);
            item->setHidden(!item->text(0).contains(search->text(),Qt::CaseInsensitive));
        }
    };
    connect(search,&QLineEdit::textChanged,this,filter);
    connect(sceneTree->model(),&QAbstractItemModel::rowsInserted,this,[filter] { filter(); });
    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+Q"),this);
    connect(searchShortcut,&QShortcut::activated,search,qOverload<>(&QWidget::setFocus));

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
                [this](const QString& text) { rendererMessage_ = text; if (!viewport_->isPlacing() && !viewport_->isMoving()) statusBar()->showMessage(text); });
        connect(renderWindow,&VulkanViewport::placementStatus,this,[this](const QString& text) { statusBar()->showMessage(text); });
        connect(renderWindow,&VulkanViewport::placementChanged,this,[this](bool active) {
            if (!active) statusBar()->showMessage(rendererMessage_);
            updateHistoryActions();
        });
        connect(renderWindow,&VulkanViewport::objectPlaced,this,[this](const n3d::ObjectData& object) { addCreatedObject(object); });
        connect(renderWindow,&VulkanViewport::objectMoved,this,[this](quint64 id,const n3d::ObjectData& moved) {
            // The viewport already owns this transient move. Do not feed it back
            // through updateObject(), which would cancel the active gesture.
            if (!scene_.update(id,moved)) return;
            for (int i=0;i<sceneTree_->topLevelItemCount();++i) {
                auto* item=sceneTree_->topLevelItem(i);
                if (item->data(0,Qt::UserRole).toULongLong()==id) item->setToolTip(0,objectDescription(moved));
            }
            refreshProperties();
        });
        connect(renderWindow,&VulkanViewport::moveChanged,this,[this](bool active) {
            if (active) {
                if (const auto* object=scene_.find(selectedObject_)) moveBefore_=*object;
            } else if (moveBefore_) {
                const auto previous=*moveBefore_; moveBefore_.reset();
                if (const auto* object=scene_.find(previous.id))
                    history_.recordApplied({"移动对象",previous.id,previous.data,object->data,0,previous.id,previous.id});
            }
            refreshProperties();
            updateHistoryActions();
            if (!active) statusBar()->showMessage(rendererMessage_);
        });
        connect(renderWindow, &VulkanViewport::selectionChanged, this,
                [this](quint64 id) { selectObject(id); });
        viewport = QWidget::createWindowContainer(renderWindow, workspace);
        renderWindow->installEventFilter(this);
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
    propertyLayout->setContentsMargins(6, 0, 6, 6);
    propertyLayout->setSpacing(6);
    auto* title = new QLabel(QStringLiteral("属性"), properties);
    title->setObjectName("panelHeading");
    title->setFont(titleFont);
    propertyLayout->addWidget(title);
    auto* examples = new QComboBox(properties);
    examples->setObjectName(QStringLiteral("sceneExamples"));
    examples->addItems({QStringLiteral("初始场景 · 2 个对象"), QStringLiteral("参数变化 · 仅修改长方体"),
                       QStringLiteral("多个对象 · 6 个基本体"), QStringLiteral("空场景")});
    sceneLayout->addWidget(new QLabel(QStringLiteral("场景示例")));
    sceneLayout->addWidget(examples);
    auto* selectedTitle = new QLabel(QStringLiteral("▾  常规"), properties);
    selectedTitle->setObjectName("sectionHeading");
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
    auto* dimensionHeading = new QLabel(QStringLiteral("▾  尺寸"),dimensionPanel_);
    dimensionHeading->setObjectName("sectionHeading");
    dimensions->addRow(dimensionHeading);
    const char* editorIds[] = {"dimensionWidth", "dimensionHeight", "dimensionDepth"};
    for (int field=0; field<3; ++field) {
        auto* editor = new DimensionSpinBox(dimensionPanel_);
        editor->setObjectName(QString::fromLatin1(editorIds[field]));
        dimensionEditors_[field] = editor;
        editor->installEventFilter(this);
        editor->findChild<QLineEdit*>()->installEventFilter(this);
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
        if (field == 0 || field == 3) {
            auto* heading = new QLabel(field == 0 ? QStringLiteral("▾  位置 · 底面中心") : QStringLiteral("▾  方向"),transformPanel_);
            heading->setObjectName("sectionHeading");
            transforms->addRow(heading);
        }
        auto* editor = new TransformSpinBox(field >= 3,transformPanel_);
        editor->setObjectName(QString::fromLatin1(transformIds[field]));
        editor->setAccessibleName((field < 3 ? QStringLiteral("位置 ") : QStringLiteral("旋转 ")) + axes[field%3]);
        transformEditors_[field] = editor;
        editor->installEventFilter(this);
        editor->findChild<QLineEdit*>()->installEventFilter(this);
        auto* axis = new QLabel(axes[field%3],transformPanel_);
        axis->setBuddy(editor);
        transforms->addRow(axis,editor);
        connect(editor,&QDoubleSpinBox::valueChanged,this,[this,field] { commitTransform(field); });
    }
    propertyLayout->addWidget(transformPanel_);
    auto* instructions = new QLabel(QStringLiteral(
        "回车或移开焦点应用；无效输入恢复\nCtrl 步进：位置 100 mm / 旋转 15°\n\n"
        "选中后拖动箭头移动：X 红 / Y 绿 / Z 蓝\nCtrl 吸附 100 mm / Esc 取消本次移动\n"
        "左键单击选择 / 其他区域拖动导航\n中键旋转 / Shift＋中键平移\n滚轮缩放 / 空白单击或 Esc 取消选择\n"
        "F 重置视图 / 空格切换预览旋转"), properties);
    instructions->setWordWrap(true);
    instructions->setObjectName(QStringLiteral("mutedLabel"));
    instructions->hide();
    sceneTree->setToolTip(instructions->text());
    propertyLayout->addStretch();
    connect(examples, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index<0) return;
        cancelTransientEdit();
        if (!confirmReplacement()) {
            const QSignalBlocker blocked(findChild<QComboBox*>("sceneExamples"));
            findChild<QComboBox*>("sceneExamples")->setCurrentIndex(exampleIndex_);
            return;
        }
        exampleIndex_=index;
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
    auto* viewPanel = new QFrame;
    auto* viewLayout = new QVBoxLayout(viewPanel);
    viewLayout->setContentsMargins(0,0,0,0); viewLayout->setSpacing(0);
    auto* viewHeader = new QWidget;
    auto* viewHeaderLayout = new QHBoxLayout(viewHeader);
    viewHeaderLayout->setContentsMargins(8,0,4,0);
    auto* viewTitle = new QLabel(QStringLiteral("▣  3D 视图"));
    viewTitle->setObjectName("panelHeading");
    viewHeaderLayout->addWidget(viewTitle); viewHeaderLayout->addStretch();
    auto* fit = new QToolButton; fit->setIcon(ribbonIcon("resetView"));
    fit->setToolTip(QStringLiteral("重置视图 (F)")); fit->setEnabled(viewport_ != nullptr);
    connect(fit,&QToolButton::clicked,this,[this] { if(viewport_) viewport_->resetView(); });
    viewHeaderLayout->addWidget(fit);
    viewLayout->addWidget(viewHeader); viewLayout->addWidget(viewport,1);
    workspace->addWidget(viewPanel);
    workspace->addWidget(propertyScroll);
    workspace->setStretchFactor(0, 0);
    workspace->setStretchFactor(1, 1);
    workspace->setStretchFactor(2, 0);
    workspace->setSizes({220, 800, 260});

    auto* shell = new QWidget;
    auto* shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(2,2,2,0); shellLayout->setSpacing(4);
    auto* body = new QWidget;
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0,0,0,0); bodyLayout->setSpacing(3);
    auto* rail = new QFrame; rail->setObjectName("activityRail"); rail->setFixedWidth(60);
    auto* railLayout = new QVBoxLayout(rail); railLayout->setContentsMargins(2,6,2,6); railLayout->setSpacing(10);
    const QString railLabels[]={QStringLiteral("模型"),QStringLiteral("选择"),QStringLiteral("平移"),QStringLiteral("缩放"),QStringLiteral("视图"),QStringLiteral("命令")};
    const char* railIcons[]={"createBox","select","navPan","navZoom","resetView","command"};
    for(int i=0;i<6;++i) {
        auto* b=new QToolButton; b->setText(railLabels[i]); b->setIcon(ribbonIcon(railIcons[i]));
        b->setIconSize(QSize(28,28)); b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon); b->setFixedSize(54, 60);
        railLayout->addWidget(b);
        connect(b,&QToolButton::clicked,this,[this,i] {
            auto* tabs=findChild<QTabWidget*>("ribbonTabs");
            if(i==0) tabs->setCurrentIndex(1);
            else if(i==4) tabs->setCurrentIndex(2);
            else if(i==5) findChild<QLineEdit*>("commandInput")->setFocus();
            else if(auto* action=findChild<QAction*>(i==1?"navOrbit":i==2?"navPan":"navZoom")) action->trigger();
        });
    }
    railLayout->addStretch(); bodyLayout->addWidget(rail); bodyLayout->addWidget(workspace,1);
    auto* vertical = new QSplitter(Qt::Vertical); vertical->setChildrenCollapsible(false);
    vertical->addWidget(body);
    auto* commandPanel = new QFrame; commandPanel->setObjectName("commandPanel");
    auto* commandLayout = new QVBoxLayout(commandPanel); commandLayout->setContentsMargins(6,0,6,4); commandLayout->setSpacing(2);
    auto* commandTitle=new QLabel(QStringLiteral("命令窗口")); commandTitle->setObjectName("panelHeading");
    commandLayout->addWidget(commandTitle);
    auto* log = new QPlainTextEdit; log->setObjectName("commandLog"); log->setReadOnly(true); log->setMaximumBlockCount(200);
    log->setPlainText(QStringLiteral("就绪。选择对象，或输入命令。输入 help 查看可用命令。"));
    commandLayout->addWidget(log,1);
    auto* commandRow=new QHBoxLayout; commandRow->addWidget(new QLabel(QStringLiteral("命令:")));
    auto* input=new QLineEdit; input->setObjectName("commandInput"); input->setPlaceholderText(QStringLiteral("help / select / fit / box / cylinder / undo / redo / delete"));
    commandRow->addWidget(input,1); commandLayout->addLayout(commandRow);
    connect(input,&QLineEdit::returnPressed,this,[this,input,log] {
        const auto command=input->text().trimmed().toLower(); if(command.isEmpty()) return;
        log->appendPlainText(QStringLiteral("> ")+command); input->clear();
        const QMap<QString,QString> actions={{"select","navOrbit"},{"fit","resetView"},{"box","createBox"},{"cylinder","createCylinder"},{"undo","undo"},{"redo","redo"},{"delete","deleteObject"},{"copy","duplicateObject"},{"pan","navPan"},{"zoom","navZoom"}};
        if(command=="help") log->appendPlainText(QStringLiteral("select 选择 · fit 重置视图 · pan 平移 · zoom 缩放\nbox 长方体 · cylinder 圆柱体 · copy 复制 · delete 删除 · undo 撤销 · redo 重做"));
        else if(actions.contains(command)) {
            auto* action=findChild<QAction*>(actions.value(command));
            if(action->isEnabled()) { action->trigger(); log->appendPlainText(QStringLiteral("已执行：")+action->text()); }
            else log->appendPlainText(QStringLiteral("当前状态下无法执行该命令。"));
        } else log->appendPlainText(QStringLiteral("未知命令。输入 help 查看可用命令。"));
    });
    vertical->addWidget(commandPanel); vertical->setStretchFactor(0,1); vertical->setStretchFactor(1,0); vertical->setSizes({720,110});
    shellLayout->addWidget(vertical,1); setCentralWidget(shell);
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("项目: Demo_Plant    │    单位: mm    │    网格: 100    │    3D 视图  ")));

}

void MainWindow::setScene(n3d::Scene scene)
{
    if (viewport_) viewport_->cancelMove();
    if (viewport_) viewport_->cancelPlacement();
    if (creationDialog_) creationDialog_->reject();
    scene_ = std::move(scene);
    projectPath_.clear();
    history_.clear(); moveBefore_.reset();
    syncScene(0);
    updateHistoryActions();
}

void MainWindow::syncScene(n3d::ObjectId selection)
{
    const QSignalBlocker blocked(sceneTree_);
    if (viewport_) {
        const QSignalBlocker blockedViewport(viewport_);
        viewport_->setScene(scene_);
    }
    sceneTree_->clear();
    for (const auto& object : scene_.objects()) {
        const auto name = QString::fromStdString(object.data.name);
        auto* item = new QTreeWidgetItem({name});
        item->setIcon(0,ribbonIcon(std::holds_alternative<n3d::BoxParameters>(object.data.shape)?"createBox":"createCylinder"));
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(object.id));
        sceneTree_->addTopLevelItem(item);
        item->setToolTip(0, objectDescription(object.data));
    }
    selectObject(selection);
}

void MainWindow::selectObject(n3d::ObjectId id)
{
    if (viewport_) viewport_->cancelMove();
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
    updateHistoryActions();
}

void MainWindow::duplicateSelected()
{
    if (historyBusy()) return;
    const auto* source=scene_.find(selectedObject_);
    if (!source) return;
    refreshProperties(); // Use committed parameters; discard pending input.
    const auto sourceId=source->id;
    auto copy=source->data;
    const auto base=copy.name+std::string(" 副本");
    copy.name=base;
    int suffix=2;
    while (std::any_of(scene_.objects().begin(),scene_.objects().end(),[&](const auto& object) { return object.data.name==copy.name; }))
        copy.name=base+" "+std::to_string(suffix++);
    n3d::ObjectId id;
    try { id=scene_.add(copy); }
    catch (const std::overflow_error&) { QMessageBox::warning(this,QStringLiteral("复制失败"),QStringLiteral("对象编号已耗尽。")); return; }
    history_.recordApplied({"复制对象",id,std::nullopt,copy,scene_.objects().size()-1,sourceId,id});
    syncScene(id);
    updateHistoryActions();
    statusBar()->showMessage(QStringLiteral("已原位复制并选中副本；拖动三轴箭头可将它移开。"),5000);
}

void MainWindow::deleteSelected()
{
    if (historyBusy()) return;
    const auto* source=scene_.find(selectedObject_);
    if (!source) return;
    refreshProperties();
    const auto removed=*source;
    const auto index=size_t(std::distance(scene_.objects().data(),source));
    scene_.remove(removed.id);
    history_.recordApplied({"删除对象",removed.id,removed.data,std::nullopt,index,removed.id,0});
    syncScene(0);
    updateHistoryActions();
}

void MainWindow::showCreationDialog(bool cylinder)
{
    if (!viewport_) return;
    viewport_->cancelMove();
    viewport_->cancelPlacement();
    if (creationDialog_) creationDialog_->reject();
    auto* dialog = new CreationDialog(cylinder,this);
    creationDialog_ = dialog;
    connect(dialog,&QDialog::accepted,this,[this,dialog] { viewport_->beginPlacement(dialog->objectData()); });
    connect(dialog,&QDialog::finished,this,[this] { updateHistoryActions(); });
    dialog->open();
    updateHistoryActions();
}

void MainWindow::addCreatedObject(n3d::ObjectData object)
{
    const auto previousSelection=selectedObject_;
    const QString base = std::holds_alternative<n3d::BoxParameters>(object.shape) ? QStringLiteral("长方体 ") : QStringLiteral("圆柱体 ");
    int suffix = 1;
    do {
        object.name = (base+QString::number(suffix++)).toStdString();
    } while (std::any_of(scene_.objects().begin(),scene_.objects().end(),[&](const auto& existing) { return existing.data.name == object.name; }));
    n3d::ObjectId id;
    try { id=scene_.add(object); }
    catch (const std::overflow_error&) { QMessageBox::warning(this,QStringLiteral("创建失败"),QStringLiteral("对象编号已耗尽。")); return; }
    // Keep the existing tree rows and camera, then select the newly assigned ID.
    auto* item = new QTreeWidgetItem({QString::fromStdString(object.name)});
    item->setData(0,Qt::UserRole,QVariant::fromValue<qulonglong>(id));
    item->setToolTip(0,objectDescription(object));
    sceneTree_->addTopLevelItem(item);
    viewport_->setScene(scene_);
    selectObject(id);
    history_.recordApplied({"创建基本体",id,std::nullopt,object,scene_.objects().size()-1,previousSelection,id});
    updateHistoryActions();
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
    const bool editable=object && (!viewport_ || !viewport_->isMoving());
    dimensionPanel_->setEnabled(editable);
    dimensionPanel_->setVisible(object != nullptr);
    transformPanel_->setEnabled(editable);
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
    const auto previous=object->data;
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
    history_.recordApplied({"修改属性",id,previous,edited,0,id,id});
    updateHistoryActions();
}

bool MainWindow::historyBusy() const
{
    return (creationDialog_ && creationDialog_->isVisible()) || (viewport_ && (viewport_->isMoving() || viewport_->isPlacing()));
}

void MainWindow::updateHistoryActions()
{
    updateProjectActions();
    const bool canEdit=!historyBusy() && scene_.find(selectedObject_);
    if (duplicateAction_) duplicateAction_->setEnabled(canEdit);
    if (deleteAction_) deleteAction_->setEnabled(canEdit);
    if (!undoAction_ || !redoAction_) return;
    undoAction_->setEnabled(!historyBusy() && history_.canUndo());
    redoAction_->setEnabled(!historyBusy() && history_.canRedo());
    undoAction_->setToolTip(QStringLiteral("撤销 %1（Ctrl+Z）").arg(QString::fromStdString(history_.undoLabel())));
    redoAction_->setToolTip(QStringLiteral("重做 %1（Ctrl+Y / Ctrl+Shift+Z）").arg(QString::fromStdString(history_.redoLabel())));
}

void MainWindow::undo() { replayHistory(false); }
void MainWindow::redo() { replayHistory(true); }

void MainWindow::replayHistory(bool redo)
{
    if (historyBusy()) return;
    // Discard uncommitted property text before changing selection or focus.
    refreshProperties();
    const auto selection=redo ? history_.redo(scene_):history_.undo(scene_);
    if (selection) syncScene(*selection);
    updateHistoryActions();
}

bool MainWindow::eventFilter(QObject* watched,QEvent* event)
{
    if (event->type()==QEvent::ShortcutOverride || event->type()==QEvent::KeyPress) {
        const auto* key=static_cast<QKeyEvent*>(event);
        const bool save=key->modifiers()==Qt::ControlModifier && key->key()==Qt::Key_S;
        const bool saveAs=key->modifiers()==(Qt::ControlModifier|Qt::ShiftModifier) && key->key()==Qt::Key_S;
        const bool open=key->modifiers()==Qt::ControlModifier && key->key()==Qt::Key_O;
        if (save || saveAs || open) {
            event->accept();
            if (event->type()==QEvent::KeyPress && !key->isAutoRepeat() && !historyBusy()) {
                if (open) openProjectDialog(); else saveProject(saveAs);
            }
            return true;
        }
        const bool objectContext=watched==viewport_ || watched==sceneTree_ || watched==sceneTree_->viewport();
        const bool duplicate=objectContext && key->modifiers()==Qt::ControlModifier && key->key()==Qt::Key_D;
        const bool remove=objectContext && key->modifiers()==Qt::NoModifier && key->key()==Qt::Key_Delete;
        if (duplicate || remove) {
            event->accept();
            if (event->type()==QEvent::KeyPress && !key->isAutoRepeat()) {
                if (duplicate) duplicateSelected(); else deleteSelected();
            }
            return true;
        }
        const bool undo=key->modifiers()==Qt::ControlModifier && key->key()==Qt::Key_Z;
        const bool redo=(key->modifiers()==Qt::ControlModifier && key->key()==Qt::Key_Y)
            || (key->modifiers()==(Qt::ControlModifier|Qt::ShiftModifier) && key->key()==Qt::Key_Z);
        if (undo || redo) {
            event->accept();
            if (event->type()==QEvent::KeyPress && !key->isAutoRepeat()) replayHistory(redo);
            return true;
        }
    }
    return QMainWindow::eventFilter(watched,event);
}

void MainWindow::applyTheme()
{
    qApp->setStyle("Fusion");
    qApp->setStyleSheet(QStringLiteral(R"(
        QWidget { font-family: "Segoe UI", "Microsoft YaHei UI"; font-size:12px; color:#14284b; background:#f7f9fc; }
        QMainWindow, QToolBar, QStatusBar { background:#eaf0f7; }
        QToolBar { border:0; spacing:0; padding:0; }
        QFrame[frameShape="6"], QScrollArea, QFrame#commandPanel { border:1px solid #c4d0e0; }
        QLabel#panelHeading { background:#e8eef6; font-weight:600; padding:6px 4px; min-height:18px; }
        QLabel#sectionHeading { background:#e7edf5; padding:5px; font-weight:600; }
        QLabel#mutedLabel { color:#697a91; }
        QTreeWidget { background:white; alternate-background-color:#f8fafd; border:0; outline:0; }
        QTreeWidget::item { height:27px; padding-left:8px; }
        QTreeWidget::item:selected { background:#d7e6fc; color:#123b78; }
        QTreeWidget::item:hover { background:#edf3fc; }
        QLineEdit, QDoubleSpinBox, QComboBox { background:#fff; border:1px solid #c8d4e5; border-radius:3px; padding:4px; min-height:19px; selection-background-color:#c7dcff; selection-color:#14284b; }
        QLineEdit:focus, QDoubleSpinBox:focus { border:1px solid #5689cc; }
        QDoubleSpinBox:disabled { color:#8795a7; }
        QToolButton { border:1px solid transparent; border-radius:4px; padding:3px; }
        QToolButton:hover { background:#e0ebfb; border-color:#b6cdec; }
        QToolButton:checked { background:#d2e1f8; border-color:#b7ccec; }
        QToolButton:disabled { color:#98a4b5; }
        QFrame#activityRail { background:#edf2f8; border:1px solid #cbd5e2; }
        QPlainTextEdit { background:white; border:0; font-family:Consolas; font-size:11px; }
        QSplitter::handle { background:#dce4ee; width:4px; height:4px; }
        QScrollBar:vertical { background:#edf1f6; width:9px; margin:0; }
        QScrollBar::handle:vertical { background:#b9c8db; min-height:24px; border-radius:4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:none; }
        QStatusBar { border-top:1px solid #c7d3e3; font-size:11px; }
        QStatusBar::item { border:0; }
        QToolTip { background:#fff; color:#14284b; border:1px solid #b5c9e4; padding:5px; }
    )"));
}
