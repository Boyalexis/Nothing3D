#pragma once
#include <QVulkanWindow>
#include <QTimer>
#include <QElapsedTimer>
#include "render/OrbitCamera.h"

class VulkanViewport final : public QVulkanWindow
{
    Q_OBJECT
public:
    VulkanViewport();
    QVulkanWindowRenderer* createRenderer() override;
    QMatrix4x4 modelViewProjection();
    void resetView();
    void toggleRotation();
    enum class Navigation { Orbit, Pan, Zoom };
    Navigation navigation = Navigation::Orbit;
    bool showGrid = true, showAxes = true, showCompass = true;
    OrbitCamera camera;
    float modelAngle = 0;
    int renderedFrames = 0;
    int swapchainGenerations = 0;
signals:
    void rendererStatus(const QString& message);
    void rotationChanged(bool enabled);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
private:
    QPointF previousMouse_;
    QTimer animation_;
    QElapsedTimer elapsed_;
};
