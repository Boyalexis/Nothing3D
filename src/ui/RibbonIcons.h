#pragma once
#include <QIcon>
#include <QPainter>
#include <QPainterPath>

inline QIcon ribbonIcon(const QString& id) {
    QPixmap pixels(64,64); pixels.setDevicePixelRatio(2); pixels.fill(Qt::transparent);
    QPainter p(&pixels); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#b9d8ff"),1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    auto line=[&](int x,int y,int a,int b) { p.drawLine(x,y,a,b); };
    if(id=="navOrbit") {
        p.drawArc(QRectF(6,6,20,20),30*16,290*16);
        line(25,6,25,13); line(25,13,18,12);
    } else if(id=="navPan") {
        line(16,4,16,28); line(4,16,28,16);
        line(16,4,12,8); line(16,4,20,8); line(16,28,12,24); line(16,28,20,24);
        line(4,16,8,12); line(4,16,8,20); line(28,16,24,12); line(28,16,24,20);
    } else if(id=="navZoom") {
        p.drawEllipse(QRectF(4,4,17,17)); line(19,19,28,28); line(8,12,17,12); line(12,8,12,17);
    } else if(id=="resetView") {
        line(3,14,16,3); line(16,3,29,14); line(7,12,7,28); line(7,28,25,28); line(25,28,25,12);
        p.drawRect(QRect(13,19,6,9));
    } else if(id=="autoRotate") {
        p.setBrush(QColor("#82baff")); p.drawPolygon(QPolygonF{QPointF(9,5),QPointF(26,16),QPointF(9,27)});
    } else if(id=="showGrid") {
        for(int i=5;i<=29;i+=6) { line(i,5,i,29); line(5,i,29,i); }
    } else if(id=="showAxes") {
        p.setPen(QPen(QColor("#ff7777"),2)); line(10,23,28,27);
        p.setPen(QPen(QColor("#73db91"),2)); line(10,23,10,4);
        p.setPen(QPen(QColor("#79b6ff"),2)); line(10,23,2,30);
    } else if(id=="createCylinder") {
        p.drawEllipse(QRectF(6,4,20,8)); line(6,8,6,25); line(26,8,26,25);
        p.drawArc(QRectF(6,21,20,8),180*16,180*16);
    } else {
        p.drawPolygon(QPolygonF{QPointF(16,3),QPointF(28,10),QPointF(28,24),QPointF(16,30),QPointF(4,24),QPointF(4,10)});
        line(4,10,16,17); line(28,10,16,17); line(16,17,16,30);
    }
    p.end(); return QIcon(pixels);
}
