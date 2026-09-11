#include "ui/MainWindow.h"
#include "ui/RibbonIcons.h"
#include "render/VulkanViewport.h"
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QActionGroup>
#include <QStyle>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QLineEdit>
#include <QScrollArea>

void MainWindow::buildToolbar() {
    auto* titleBar=addToolBar(QStringLiteral("项目标题"));
    titleBar->setMovable(false); titleBar->setFloatable(false);
    auto* titleWidget=new QWidget; auto* titleLayout=new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(12,3,12,3);
    auto* brand=new QLabel(QStringLiteral("▣  Nothing3D"));
    brand->setStyleSheet("font-size:19px; font-weight:700; color:#15396c;");
    titleLayout->addWidget(brand); titleLayout->addSpacing(26);
    titleLayout->addWidget(new QLabel(QStringLiteral("工业三维设计  -  [Demo_Plant]"))); titleLayout->addStretch();
    auto* search=new QLineEdit; search->setPlaceholderText(QStringLiteral("搜索项目  (Ctrl+Q)")); search->setFixedWidth(260);
    connect(search,&QLineEdit::textChanged,this,[this](const QString& text) { findChild<QLineEdit*>("projectSearch")->setText(text); });
    titleLayout->addWidget(search);
    titleWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed); titleBar->addWidget(titleWidget);
    addToolBarBreak();
    auto* bar=addToolBar(QStringLiteral("功能区"));
    bar->setMovable(false); bar->setFloatable(false);
    auto* tabs=new QTabWidget;
    tabs->setObjectName("ribbonTabs"); tabs->setDocumentMode(true);
    tabs->setMinimumWidth(600); tabs->setFixedHeight(140);
    tabs->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    bar->addWidget(tabs);
    tabs->setStyleSheet(QStringLiteral(
        "QTabBar::tab { padding:7px 22px; color:#14284b; }"
        "QTabBar::tab:selected { color:#124b98; background:#f7f9fc; border-bottom:3px solid #5c9cff; }"
        "QTabWidget::pane { border:1px solid #c7d3e3; }"
        "QFrame#ribbonGroup { border-right:1px solid #c7d3e3; }"
        "QToolButton { border:1px solid transparent; border-radius:4px; padding:3px; }"
        "QToolButton:checked { background:#d2e1f8; border:1px solid #5c9cff; }"
        "QToolButton:hover { background:#e0ebfb; }"
        "QToolButton:disabled { color:#98a4b5; }"));
    auto page=[&](const QString& title) {
        auto* p=new QWidget; auto* row=new QHBoxLayout(p);
        row->setContentsMargins(8,3,8,3); row->setSpacing(5);
        auto* scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setWidget(p);
        tabs->addTab(scroll,title); return row;
    };
    auto group=[](QHBoxLayout* row,const QString& title) {
        auto* frame=new QFrame; frame->setObjectName("ribbonGroup");
        auto* column=new QVBoxLayout(frame); column->setContentsMargins(5,1,10,1); column->setSpacing(1);
        auto* buttons=new QHBoxLayout; buttons->setSpacing(4); column->addLayout(buttons);
        auto* label=new QLabel(title); label->setAlignment(Qt::AlignCenter); label->setObjectName("mutedLabel");
        column->addWidget(label); row->addWidget(frame); return buttons;
    };
    auto button=[&](QHBoxLayout* row,const QString& title,const char* id,QStyle::StandardPixmap icon,bool enabled=true) {
        Q_UNUSED(icon);
        auto* action=new QAction(ribbonIcon(QString::fromLatin1(id)),title,this); action->setObjectName(id);
        action->setEnabled(enabled && viewport_);
        auto* b=new QToolButton; b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIconSize(QSize(28,28)); b->setMinimumSize(62,65); b->setDefaultAction(action);
        row->addWidget(b); return action;
    };
    auto* home=page(QStringLiteral("首页 Home"));
    auto* nav=group(home,QStringLiteral("导航 · 左键拖动"));
    auto* exclusive=new QActionGroup(this); exclusive->setExclusive(true);
    const QString names[]={QStringLiteral("旋转视角"),QStringLiteral("平移视角"),QStringLiteral("缩放视角")};
    const char* ids[]={"navOrbit","navPan","navZoom"};
    const QStyle::StandardPixmap icons[]={QStyle::SP_BrowserReload,QStyle::SP_ArrowUp,QStyle::SP_FileDialogContentsView};
    for(int i=0;i<3;++i) {
        auto* a=button(nav,names[i],ids[i],icons[i]); a->setCheckable(true); a->setChecked(i==0); exclusive->addAction(a);
        a->setToolTip(names[i]+QStringLiteral("：视口内按住左键拖动，单击选择对象。中键 / Shift+中键 / 滚轮仍可使用。"));
        connect(a,&QAction::triggered,this,[this,i] { if(viewport_) viewport_->navigation=static_cast<VulkanViewport::Navigation>(i); });
    }
    auto* observe=group(home,QStringLiteral("观察"));
    auto* reset=button(observe,QStringLiteral("重置视图"),"resetView",QStyle::SP_DirHomeIcon);
    reset->setToolTip(QStringLiteral("恢复相机和演示组姿态，停止旋转。视口快捷键 F。"));
    auto* rotate=button(observe,QStringLiteral("自动旋转"),"autoRotate",QStyle::SP_MediaPlay);
    rotate->setCheckable(true); rotate->setToolTip(QStringLiteral("旋转整个演示组。视口快捷键：空格。"));
    if(viewport_) {
        connect(reset,&QAction::triggered,viewport_,&VulkanViewport::resetView);
        connect(rotate,&QAction::triggered,viewport_,&VulkanViewport::toggleRotation);
        connect(viewport_,&VulkanViewport::rotationChanged,rotate,&QAction::setChecked);
        connect(viewport_,&VulkanViewport::moveChanged,this,[this,rotate](bool active) {
            rotate->setEnabled(!active && !viewport_->isPlacing());
        });
    }
    auto* history=group(home,QStringLiteral("编辑历史"));
    undoAction_=button(history,QStringLiteral("撤销"),"undo",QStyle::SP_ArrowBack,false);
    redoAction_=button(history,QStringLiteral("重做"),"redo",QStyle::SP_ArrowForward,false);
    undoAction_->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Z"))});
    redoAction_->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Y")),QKeySequence(QStringLiteral("Ctrl+Shift+Z"))});
    addAction(undoAction_); addAction(redoAction_);
    connect(undoAction_,&QAction::triggered,this,&MainWindow::undo);
    connect(redoAction_,&QAction::triggered,this,&MainWindow::redo);
    updateHistoryActions();
    auto* quickCreate=group(home,QStringLiteral("建模"));
    auto* quickBox=button(quickCreate,QStringLiteral("创建长方体"),"homeBox",QStyle::SP_FileIcon);
    quickBox->setIcon(ribbonIcon("createBox"));
    connect(quickBox,&QAction::triggered,this,[this] { showCreationDialog(false); });
    auto* quickCylinder=button(quickCreate,QStringLiteral("创建圆柱体"),"homeCylinder",QStyle::SP_FileIcon);
    quickCylinder->setIcon(ribbonIcon("createCylinder"));
    connect(quickCylinder,&QAction::triggered,this,[this] { showCreationDialog(true); });
    auto* panels=group(home,QStringLiteral("工作区"));
    auto* properties=button(panels,QStringLiteral("属性"),"properties",QStyle::SP_FileIcon);
    properties->setEnabled(true);
    connect(properties,&QAction::triggered,this,[this] { findChild<QScrollArea*>("propertyScroll")->setFocus(); });
    auto* command=button(panels,QStringLiteral("命令窗口"),"command",QStyle::SP_FileIcon);
    command->setEnabled(true);
    connect(command,&QAction::triggered,this,[this] { findChild<QLineEdit*>("commandInput")->setFocus(); });
    home->addStretch();
    auto* model=page(QStringLiteral("模型 Model"));
    auto* create=group(model,QStringLiteral("基本体 · 地面放置"));
    auto* box=button(create,QStringLiteral("长方体"),"createBox",QStyle::SP_FileIcon);
    auto* cylinder=button(create,QStringLiteral("圆柱体"),"createCylinder",QStyle::SP_DriveHDIcon);
    auto* cancel=button(create,QStringLiteral("取消放置"),"cancelPlacement",QStyle::SP_DialogCancelButton,false);
    cancel->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    box->setToolTip(QStringLiteral("输入宽、高、深，再在 Y=0 地面单击放置长方体。"));
    cylinder->setToolTip(QStringLiteral("输入直径、高度，再在 Y=0 地面单击放置圆柱体。"));
    cancel->setToolTip(QStringLiteral("取消未确认的放置，保留原场景。快捷键 Esc。"));
    connect(box,&QAction::triggered,this,[this] { showCreationDialog(false); });
    connect(cylinder,&QAction::triggered,this,[this] { showCreationDialog(true); });
    auto* edit=group(model,QStringLiteral("所选对象"));
    duplicateAction_=button(edit,QStringLiteral("复制对象"),"duplicateObject",QStyle::SP_FileIcon,false);
    deleteAction_=button(edit,QStringLiteral("删除对象"),"deleteObject",QStyle::SP_TrashIcon,false);
    duplicateAction_->setToolTip(QStringLiteral("原位复制所选对象并选中副本。视口 / 场景列表快捷键：Ctrl+D。可撤销。"));
    deleteAction_->setToolTip(QStringLiteral("删除所选对象。视口 / 场景列表快捷键：Delete。Ctrl+Z 可恢复。"));
    connect(duplicateAction_,&QAction::triggered,this,&MainWindow::duplicateSelected);
    connect(deleteAction_,&QAction::triggered,this,&MainWindow::deleteSelected);
    updateHistoryActions();
    auto* hint=new QLabel(QStringLiteral("先输入尺寸，再移动鼠标预览并单击地面\nCtrl：100 mm 吸附 · Esc：取消放置"));
    hint->setWordWrap(true);
    hint->setMinimumWidth(220);
    if (viewport_) {
        connect(cancel,&QAction::triggered,viewport_,&VulkanViewport::cancelPlacement);
        connect(viewport_,&VulkanViewport::placementChanged,this,[cancel,rotate,hint](bool active) {
            cancel->setEnabled(active); rotate->setEnabled(!active);
            hint->setText(active ? QStringLiteral("正在放置：移动鼠标显示金色线框，左键确认\nCtrl：100 mm 吸附 · Esc：取消放置")
                                 : QStringLiteral("先输入尺寸，再移动鼠标预览并单击地面\nCtrl：100 mm 吸附 · Esc：取消放置"));
        });
    }
    hint->setObjectName("mutedLabel"); model->addWidget(hint); model->addStretch();
    auto* view=page(QStringLiteral("视图 View"));
    auto* display=group(view,QStringLiteral("视口辅助 · 独立显示开关"));
    const QString titles[]={QStringLiteral("地面网格"),QStringLiteral("世界坐标轴"),QStringLiteral("方向立方体"),QStringLiteral("移动手柄")};
    const char* toggles[]={"showGrid","showAxes","showCompass","showMoveGizmo"};
    for(int i=0;i<4;++i) {
        auto* a=button(display,titles[i],toggles[i],QStyle::SP_FileDialogDetailedView);
        a->setCheckable(true); a->setChecked(true); a->setToolTip(QStringLiteral("显示 / 隐藏")+titles[i]);
        connect(a,&QAction::toggled,this,[this,i](bool shown) {
            if(!viewport_) return;
            if(i==0) viewport_->showGrid=shown;
            if(i==1) viewport_->showAxes=shown;
            if(i==2) viewport_->showCompass=shown;
            if(i==3) { viewport_->cancelMove(); viewport_->showMoveGizmo=shown; }
            viewport_->requestUpdate();
        });
    }
    view->addStretch();
    auto* project=page(QStringLiteral("工程 Project"));
    auto* files=group(project,QStringLiteral(".n3d 工程文件"));
    openAction_=button(files,QStringLiteral("打开工程"),"openProject",QStyle::SP_DialogOpenButton);
    saveAction_=button(files,QStringLiteral("保存"),"saveProject",QStyle::SP_DialogSaveButton);
    saveAsAction_=button(files,QStringLiteral("另存为"),"saveProjectAs",QStyle::SP_DialogSaveButton);
    openAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    saveAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    saveAsAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    for (auto* action:{openAction_,saveAction_,saveAsAction_}) addAction(action);
    openAction_->setToolTip(QStringLiteral("打开 .n3d 工程；未保存的修改会先提示。Ctrl+O"));
    saveAction_->setToolTip(QStringLiteral("保存当前场景。Ctrl+S"));
    saveAsAction_->setToolTip(QStringLiteral("选择另一文件保存当前场景。Ctrl+Shift+S"));
    connect(openAction_,&QAction::triggered,this,&MainWindow::openProjectDialog);
    connect(saveAction_,&QAction::triggered,this,[this] { saveProject(); });
    connect(saveAsAction_,&QAction::triggered,this,[this] { saveProject(true); });
    auto* fileHint=new QLabel(QStringLiteral("保存对象参数与编号；标题中的 * 表示尚未保存\n打开文件、切换示例或关闭时，可选择保存、不保存或取消"));
    fileHint->setWordWrap(true); fileHint->setObjectName("mutedLabel"); project->addWidget(fileHint); project->addStretch();
    auto* homeFiles=group(home,QStringLiteral("项目"));
    for(auto* action:{openAction_,saveAction_,saveAsAction_}) {
        auto* b=new QToolButton; b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIconSize(QSize(28,28)); b->setMinimumSize(62,65); b->setDefaultAction(action); homeFiles->addWidget(b);
    }
    auto* fileGroup=home->takeAt(home->count()-1); home->insertWidget(0,fileGroup->widget()); delete fileGroup;
    updateProjectActions();
}
