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

void MainWindow::buildToolbar() {
    auto* bar=addToolBar(QStringLiteral("功能区"));
    bar->setMovable(false); bar->setFloatable(false);
    auto* tabs=new QTabWidget;
    tabs->setObjectName("ribbonTabs"); tabs->setDocumentMode(true);
    tabs->setMinimumWidth(850); tabs->setFixedHeight(134);
    tabs->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    bar->addWidget(tabs);
    tabs->setStyleSheet(QStringLiteral(
        "QTabBar::tab { padding:7px 22px; color:#a8b7cc; }"
        "QTabBar::tab:selected { color:white; background:#293a53; border-bottom:3px solid #5c9cff; }"
        "QTabWidget::pane { border:1px solid #354259; }"
        "QFrame#ribbonGroup { border-right:1px solid #354259; }"
        "QToolButton { border:1px solid transparent; border-radius:4px; padding:3px; }"
        "QToolButton:checked { background:#294c77; border:1px solid #5c9cff; }"
        "QToolButton:hover { background:#34445b; }"
        "QToolButton:disabled { color:#687385; }"));
    auto page=[&](const QString& title) {
        auto* p=new QWidget; auto* row=new QHBoxLayout(p);
        row->setContentsMargins(8,3,8,3); row->setSpacing(12);
        tabs->addTab(p,title); return row;
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
        b->setIconSize(QSize(28,28)); b->setMinimumSize(76,65); b->setDefaultAction(action);
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
        a->setToolTip(names[i]+QStringLiteral("：视口内按住左键拖动。中键 / Shift+中键 / 滚轮仍可使用。"));
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
    }
    home->addStretch();
    auto* model=page(QStringLiteral("模型 Model"));
    auto* create=group(model,QStringLiteral("基本体 · 尚未开放"));
    auto* box=button(create,QStringLiteral("长方体"),"createBox",QStyle::SP_FileIcon,false);
    auto* cylinder=button(create,QStringLiteral("圆柱体"),"createCylinder",QStyle::SP_DriveHDIcon,false);
    const auto reason=QStringLiteral("待接入场景对象和尺寸参数。当前模型为固定演示，暂不支持创建。" );
    box->setToolTip(reason); cylinder->setToolTip(reason);
    auto* hint=new QLabel(QStringLiteral("下一阶段接入场景对象与尺寸参数\n当前长方体、圆柱体为只读演示"));
    hint->setObjectName("mutedLabel"); model->addWidget(hint); model->addStretch();
    auto* view=page(QStringLiteral("视图 View"));
    auto* display=group(view,QStringLiteral("视口辅助 · 独立显示开关"));
    const QString titles[]={QStringLiteral("地面网格"),QStringLiteral("世界坐标轴"),QStringLiteral("方向立方体")};
    const char* toggles[]={"showGrid","showAxes","showCompass"};
    for(int i=0;i<3;++i) {
        auto* a=button(display,titles[i],toggles[i],QStyle::SP_FileDialogDetailedView);
        a->setCheckable(true); a->setChecked(true); a->setToolTip(QStringLiteral("显示 / 隐藏")+titles[i]);
        connect(a,&QAction::toggled,this,[this,i](bool shown) {
            if(!viewport_) return;
            if(i==0) viewport_->showGrid=shown;
            if(i==1) viewport_->showAxes=shown;
            if(i==2) viewport_->showCompass=shown;
            viewport_->requestUpdate();
        });
    }
    view->addStretch();
}
