#pragma once
#include <QTabWidget>
#include <QToolButton>
#include <QAction>
#include <QMouseEvent>
#include <QApplication>
#include "render/VulkanViewport.h"
#include "render/GuideImageCheck.h"

inline bool checkRibbon(QWidget& window,VulkanViewport* viewport) {
    auto* tabs=window.findChild<QTabWidget*>("ribbonTabs");
    if(!tabs || tabs->count()!=4 || !viewport) return false;
    bool okay=true;
    const auto fitCamera=viewport->camera;
    const auto fitSelection=viewport->selectedObject();
    auto* fitAction=window.findChild<QAction*>("fitScene");
    if(!fitAction || !fitAction->isEnabled()) okay=false;
    else {
        viewport->camera.target={1000,1000,1000};
        fitAction->trigger();
        okay &= viewport->camera.framingRadius>0 && viewport->camera.target.length()<10
            && viewport->selectedObject()==fitSelection;
        const auto frame=viewport->grab();
        okay &= !frame.isNull(); frame.save("build/fit-scene.png");
    }
    viewport->camera=fitCamera;
    tabs->setCurrentIndex(2);
    for(const char* id:{"topView","frontView","rightView","orthographicView","perspectiveView"}) {
        auto* action=window.findChild<QAction*>(id);
        if(!action || !action->isEnabled()) { okay=false; continue; }
        action->trigger();
        okay &= viewport->camera.orthographic==(QString::fromLatin1(id)!="perspectiveView");
        const auto frame=viewport->grab();
        okay &= !frame.isNull();
        frame.save(QStringLiteral("build/%1.png").arg(QString::fromLatin1(id)));
    }
    viewport->resetView();
    okay &= window.findChild<QAction*>("perspectiveView")->isChecked();
    qInfo("Orthographic ribbon and rendered views: %d",int(okay));
    auto click=[&](const char* id) {
        auto* action=window.findChild<QAction*>(id);
        if(!action || !action->isEnabled()) { okay=false; return; }
        for(auto* button : tabs->findChildren<QToolButton*>()) if(button->defaultAction()==action) { button->click(); return; }
        okay=false;
    };
    tabs->setCurrentIndex(1);
    for(const char* id : {"createBox","createCylinder"}) {
        auto* a=window.findChild<QAction*>(id); okay &= a && a->isEnabled() && !a->toolTip().isEmpty();
    }
    window.grab().save(QStringLiteral("build/ribbon-model.png"));
    tabs->setCurrentIndex(0);
    for(const char* mode : {"navOrbit","navPan","navZoom"}) {
        viewport->resetView(); click(mode);
        const auto before=viewport->camera;
        QMouseEvent down(QEvent::MouseButtonPress,QPointF(200,250),QPointF(200,250),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent move(QEvent::MouseMove,QPointF(220,270),QPointF(220,270),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent up(QEvent::MouseButtonRelease,QPointF(220,270),QPointF(220,270),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(viewport,&down); QApplication::sendEvent(viewport,&move); QApplication::sendEvent(viewport,&up);
        if(viewport->navigation==VulkanViewport::Navigation::Orbit) okay &= viewport->camera.yaw!=before.yaw && viewport->camera.target==before.target;
        if(viewport->navigation==VulkanViewport::Navigation::Pan) okay &= viewport->camera.target!=before.target && viewport->camera.yaw==before.yaw;
        if(viewport->navigation==VulkanViewport::Navigation::Zoom) okay &= viewport->camera.distance!=before.distance && viewport->camera.target==before.target;
    }
    click("navOrbit"); click("autoRotate");
    okay &= window.findChild<QAction*>("autoRotate")->isChecked();
    click("resetView");
    okay &= !window.findChild<QAction*>("autoRotate")->isChecked() && viewport->camera.distance==5.5f;
    tabs->setCurrentIndex(2);
    QImage visibleCorner;
    for(int mask=7;mask>=0;--mask) {
        const char* ids[]={"showGrid","showAxes","showCompass"};
        for(int i=0;i<3;++i) {
            auto* a=window.findChild<QAction*>(ids[i]);
            if(a->isChecked()!=bool(mask&(1<<i))) click(ids[i]);
        }
        const auto image=viewport->grab();
        const auto rect=Guides::compassRect(image.size(),viewport->devicePixelRatio());
        okay &= viewport->showGrid==bool(mask&1) && viewport->showAxes==bool(mask&2) && viewport->showCompass==bool(mask&4);
        okay &= hasInfiniteGrid(image,rect)==bool(mask&1);
        int green=0;
        for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
            const auto c=image.pixelColor(x,y);
            green += c.green()>190 && c.red()<90 && c.blue()<110;
        }
        okay &= (green>10)==bool(mask&2);
        if(mask==7) visibleCorner=image.copy(rect);
        if(mask==3) okay &= image.copy(rect)!=visibleCorner;
        image.save(QStringLiteral("build/ribbon-visibility-%1.png").arg(mask));
    }
    click("showGrid"); click("showAxes"); click("showCompass");
    viewport->grab();
    window.grab().save(QStringLiteral("build/ribbon-view.png"));
    tabs->setCurrentIndex(0);
    window.grab().save(QStringLiteral("build/ribbon-home.png"));
    window.grab(QRect(0,0,window.width(),tabs->height()+36)).save(QStringLiteral("build/ribbon-home-top.png"));
    qInfo("Ribbon navigation / tabs / 8 visibility combinations: %d",int(okay));
    return okay;
}
