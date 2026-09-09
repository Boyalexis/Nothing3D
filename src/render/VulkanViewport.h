#pragma once
#include <QVulkanWindow>
#include <QTimer>
#include <QElapsedTimer>
#include "render/OrbitCamera.h"
#include "scene/Scene.h"
#include <optional>

class VulkanViewport final : public QVulkanWindow
{
    Q_OBJECT
public:
    VulkanViewport();
    QVulkanWindowRenderer* createRenderer() override;
    QMatrix4x4 modelViewProjection();
    void resetView();
    void toggleRotation();
    const n3d::Scene& scene() const { return scene_; }
    void setScene(n3d::Scene scene);
    bool updateObject(n3d::ObjectId id, const n3d::ObjectData& data);
    n3d::ObjectId selectedObject() const { return selectedObject_; }
    void selectObject(n3d::ObjectId id);
    void beginPlacement(const n3d::ObjectData& object);
    void cancelPlacement();
    bool isPlacing() const { return placement_.has_value(); }
    const std::optional<n3d::ObjectData>& placementPreview() const { return preview_; }
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
    void selectionChanged(quint64 id);
    void placementChanged(bool active);
    void placementStatus(const QString& message);
    void objectPlaced(const n3d::ObjectData& object);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;
private:
    void updatePlacement(QPointF position, Qt::KeyboardModifiers modifiers);
    std::optional<n3d::ObjectData> placement_, preview_;
    std::optional<QPointF> placementMouse_;
    n3d::Scene scene_;
    n3d::ObjectId selectedObject_ = 0;
    QPointF pressPosition_;
    bool clickPending_ = false, overlayPress_ = false;
    QPointF previousMouse_;
    QTimer animation_;
    QElapsedTimer elapsed_;
};
