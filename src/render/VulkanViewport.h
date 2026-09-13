#pragma once
#include <QVulkanWindow>
#include <QTimer>
#include <QElapsedTimer>
#include "render/OrbitCamera.h"
#include "scene/Scene.h"
#include "render/MoveGizmo.h"
#include <optional>

class VulkanViewport final : public QVulkanWindow
{
    Q_OBJECT
public:
    VulkanViewport();
    QVulkanWindowRenderer* createRenderer() override;
    QMatrix4x4 modelViewProjection();
    void resetView();
    void fitScene();
    void setProjection(bool orthographic);
    void setStandardView(OrbitCamera::StandardView view);
    void toggleRotation();
    const n3d::Scene& scene() const { return scene_; }
    void setScene(n3d::Scene scene);
    bool updateObject(n3d::ObjectId id, const n3d::ObjectData& data);
    n3d::ObjectId selectedObject() const { return selectedObject_; }
    void selectObject(n3d::ObjectId id);
    void beginPlacement(const n3d::ObjectData& object);
    void cancelPlacement();
    void cancelMove();
    bool isMoving() const { return move_.has_value(); }
    int activeMoveAxis() const { return move_ ? move_->axisIndex : hoverAxis_; }
    std::optional<MoveGizmo::Layout> moveGizmo();
    bool isPlacing() const { return placement_.has_value(); }
    const std::optional<n3d::ObjectData>& placementPreview() const { return preview_; }
    enum class Navigation { Orbit, Pan, Zoom };
    Navigation navigation = Navigation::Orbit;
    bool showGrid = true, showAxes = true, showCompass = true, showMoveGizmo = true;
    OrbitCamera camera;
    float modelAngle = 0;
    int renderedFrames = 0;
    int swapchainGenerations = 0;
    bool continuousRendering = false;
signals:
    void frameRecorded(qint64 cpuNanoseconds);
    void projectionChanged(bool orthographic);
    void rendererStatus(const QString& message);
    void rotationChanged(bool enabled);
    void selectionChanged(quint64 id);
    void placementChanged(bool active);
    void placementStatus(const QString& message);
    void objectPlaced(const n3d::ObjectData& object);
    void objectMoved(quint64 id, const n3d::ObjectData& object);
    void moveChanged(bool active);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool event(QEvent* event) override;
private:
    void updateMove(QPointF position, Qt::KeyboardModifiers modifiers);
    void finishMove();
    void publishMove(const n3d::ObjectData& data);
    std::optional<MoveGizmo::Drag> move_;
    n3d::ObjectData moveOriginal_;
    n3d::ObjectId moveObject_ = 0;
    QPointF moveMouse_;
    int hoverAxis_ = -1;
    bool cancelledMovePress_ = false;
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
