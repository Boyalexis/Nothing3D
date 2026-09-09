#include "render/VulkanViewport.h"
#include <QVulkanFunctions>
#include "render/QtPresentSync.h"
#include "render/ColorMesh.h"
#include "render/GuideGeometry.h"
#include "render/ViewCubeGeometry.h"
#include "render/InfiniteAxis.h"
#include "render/SceneGeometry.h"
#include "render/ScenePicking.h"
#include "render/GroundPlacement.h"
#include <QGuiApplication>
#include <QStyleHints>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

namespace {
class SceneRenderer final : public QVulkanWindowRenderer
{
public:
    explicit SceneRenderer(VulkanViewport* window) : window_(window) {}

    void initResources() override
    {
        functions_ = window_->vulkanInstance()->deviceFunctions(window_->device());
        primitives_.initialize(window_, SceneGeometry::vertices());
        previewMesh_.initialize(window_, SceneGeometry::wireVertices(), VK_PRIMITIVE_TOPOLOGY_LINE_LIST, true, false);
        const std::array<BoxVertex,3> screen{{{{-1,-1,0},{0,0,0}},{{3,-1,0},{0,0,0}},{{-1,3,0},{0,0,0}}}};
        ground_.initialize(window_,screen,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,true,false);
        gridOnly_.initialize(window_,screen,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,true,false);
        axesOnly_.initialize(window_,screen,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,true,false);
        std::vector<BoxVertex> vertical;
        Guides::line(vertical,{0,0,0},{0,1,0},Guides::green);
        axis_.initialize(window_,vertical,VK_PRIMITIVE_TOPOLOGY_LINE_LIST,true,false);
        compass_.initialize(window_, ViewCube::cube());
        labelGeometry_=ViewCube::labels();
        letters_.initialize(window_, labelGeometry_.mesh,VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,true,false);
        home_.initialize(window_,ViewCube::home(),VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,false,false);
    }

    void initSwapChainResources() override
    {
        sync_.initialize(window_);
        primitives_.createPipeline();
        previewMesh_.createPipeline();
        gridOnly_.createPipeline(true,1); axesOnly_.createPipeline(true,2);
        ground_.createPipeline(true); axis_.createPipeline(); compass_.createPipeline(); letters_.createPipeline(); home_.createPipeline();
        ++window_->swapchainGenerations;
        const auto* gpu = window_->physicalDeviceProperties();
        const auto size = window_->swapChainImageSize();
        emit window_->rendererStatus(QStringLiteral("参数化场景 · %1 · %2 × %3 · 验证层：%4 · 中键旋转 / 滚轮缩放")
            .arg(QString::fromUtf8(gpu->deviceName)).arg(size.width()).arg(size.height())
            .arg(window_->vulkanInstance()->layers().contains("VK_LAYER_KHRONOS_validation")
                 ? QStringLiteral("开启") : QStringLiteral("未安装")));
    }

    void startNextFrame() override
    {
        sync_.selectImage(window_);
        // Qt owns the swapchain and synchronization. We record the actual
        // Vulkan commands that clear its color and depth attachments.
        VkClearValue clears[2]{};
        clears[0].color = {{0.04f, 0.09f, 0.16f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo pass{};
        pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass = window_->defaultRenderPass();
        pass.framebuffer = window_->currentFramebuffer();
        const auto size = window_->swapChainImageSize();
        pass.renderArea.extent = {uint32_t(size.width()), uint32_t(size.height())};
        pass.clearValueCount = 2;
        pass.pClearValues = clears;
        const auto command = window_->currentCommandBuffer();
        functions_->vkCmdBeginRenderPass(command, &pass, VK_SUBPASS_CONTENTS_INLINE);
        const auto sceneView = window_->modelViewProjection();
        for (const auto& object : window_->scene().objects()) {
            const auto renderObject = SceneGeometry::prepare(object);
            const bool box = renderObject.primitive == SceneGeometry::Primitive::Box;
            // Push constants are recorded into this frame's command buffer.
            // Edits never overwrite vertex memory still in use by another frame.
            primitives_.draw(command, sceneView * renderObject.model, {},
                box ? 0 : SceneGeometry::boxCount,
                box ? SceneGeometry::boxCount : SceneGeometry::cylinderCount,
                object.id == window_->selectedObject());
        }
        const auto world = window_->clipCorrectionMatrix()
            * window_->camera.projection(float(size.width())/float(std::max(1,size.height())))
            * window_->camera.view();
        if(window_->showGrid && window_->showAxes) ground_.drawGrid(command,world);
        else if(window_->showGrid) gridOnly_.drawGrid(command,world);
        else if(window_->showAxes) axesOnly_.drawGrid(command,world);
        if(window_->showAxes) if(const auto axisTransform=infiniteYAxis(world)) axis_.draw(command,*axisTransform);
        if (const auto& preview = window_->placementPreview()) {
            const auto object = SceneGeometry::prepare({0,*preview});
            const bool box = object.primitive == SceneGeometry::Primitive::Box;
            previewMesh_.draw(command,sceneView*object.model,{},box ? 0 : SceneGeometry::boxCount*2,
                              (box ? SceneGeometry::boxCount : SceneGeometry::cylinderCount)*2);
        }
        if(window_->showCompass) {
        const auto corner = Guides::compassRect(size,window_->devicePixelRatio());
        const auto orientation = Guides::compassMatrix(window_->camera.view(), window_->clipCorrectionMatrix());
        // A local depth clear lets cube faces occlude each other without scene interference.
        VkClearAttachment depthClear{};
        depthClear.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT; depthClear.clearValue.depthStencil={1,0};
        VkClearRect clearRect{{{corner.x(),corner.y()},{uint32_t(corner.width()),uint32_t(corner.height())}},0,1};
        functions_->vkCmdClearAttachments(command,1,&depthClear,1,&clearRect);
        compass_.draw(command, orientation, corner);
        for (uint32_t i=0;i<4;++i) {
            const auto p=orientation*QVector4D(ViewCube::cardinalPositions[i],1);
            QMatrix4x4 label;
            label.translate(p.x()/p.w(),p.y()/p.w(),p.z()/p.w());
            letters_.draw(command,label,corner,labelGeometry_.first[i],labelGeometry_.count[i]);
        }
        QMatrix4x4 homeTransform; homeTransform.translate(0.78f,-0.80f,0);
        home_.draw(command,homeTransform,corner);
        }
        functions_->vkCmdEndRenderPass(command);
        ++window_->renderedFrames;
        window_->frameReady();
        // Static scene: redraw on expose/resize, without a continuous busy loop.
    }

    void releaseResources() override {
        previewMesh_.release();
        gridOnly_.release(); axesOnly_.release();
        primitives_.release(); ground_.release(); axis_.release(); compass_.release(); letters_.release(); home_.release(); functions_ = nullptr;
    }
    void releaseSwapChainResources() override {
        previewMesh_.releasePipeline();
        gridOnly_.releasePipeline(); axesOnly_.releasePipeline();
        primitives_.releasePipeline(); ground_.releasePipeline(); axis_.releasePipeline(); compass_.releasePipeline(); letters_.releasePipeline(); home_.releasePipeline();
        sync_.release(window_);
    }
private:
    ColorMesh primitives_, previewMesh_, ground_, axis_, compass_, letters_, home_;
    ColorMesh gridOnly_, axesOnly_;
    ViewCube::Labels labelGeometry_;
    QtPresentSync sync_;
    VulkanViewport* window_;
    QVulkanDeviceFunctions* functions_ = nullptr;
};
}

QVulkanWindowRenderer* VulkanViewport::createRenderer()
{
    return new SceneRenderer(this);
}

void VulkanViewport::setScene(n3d::Scene scene)
{
    cancelPlacement();
    scene_ = std::move(scene);
    clickPending_ = false;
    selectObject(0);
    requestUpdate();
}

void VulkanViewport::beginPlacement(const n3d::ObjectData& object)
{
    n3d::Scene validation;
    validation.add(object);
    animation_.stop(); modelAngle = 0;
    emit rotationChanged(false);
    placement_ = object; preview_.reset(); placementMouse_.reset();
    clickPending_ = false;
    setCursor(Qt::CrossCursor);
    emit placementChanged(true);
    emit placementStatus(QStringLiteral("移动鼠标到地面预览，左键单击放置 · Ctrl：100 mm 吸附 · Esc：取消"));
    requestUpdate(); requestActivate();
}

void VulkanViewport::cancelPlacement()
{
    if (!placement_) return;
    placement_.reset(); preview_.reset(); placementMouse_.reset(); clickPending_ = false;
    unsetCursor(); emit placementChanged(false); requestUpdate();
}

void VulkanViewport::updatePlacement(QPointF position, Qt::KeyboardModifiers modifiers)
{
    if (!placement_) return;
    placementMouse_ = position;
    const auto corner = Guides::compassRect(swapChainImageSize(),devicePixelRatio());
    const bool overlay = showCompass && corner.contains((position*devicePixelRatio()).toPoint());
    const auto point = overlay ? std::optional<n3d::Vector3>{}
        : GroundPlacement::point(modelViewProjection(),position,QSizeF(width(),height()),modifiers.testFlag(Qt::ControlModifier));
    preview_.reset();
    if (point) {
        preview_ = *placement_; preview_->positionMm = *point;
        setCursor(Qt::CrossCursor);
        emit placementStatus(QStringLiteral("放置底心：(%1, 0, %2) mm · %3 · 左键确认 / Esc 取消")
            .arg(point->x,0,'f',2).arg(point->z,0,'f',2)
            .arg(modifiers.testFlag(Qt::ControlModifier) ? QStringLiteral("100 mm 吸附") : QStringLiteral("自由放置")));
    } else {
        setCursor(Qt::ForbiddenCursor);
        emit placementStatus(QStringLiteral("此处无法放置，请移动到可见地面 · Esc：取消"));
    }
    requestUpdate();
}

bool VulkanViewport::event(QEvent* event)
{
    if (isPlacing() && (event->type() == QEvent::Leave || event->type() == QEvent::Resize)) {
        preview_.reset(); placementMouse_.reset(); requestUpdate();
    }
    return QVulkanWindow::event(event);
}

void VulkanViewport::selectObject(n3d::ObjectId id)
{
    if (!scene_.find(id)) id = 0;
    if (selectedObject_ == id) return;
    selectedObject_ = id;
    emit selectionChanged(id);
    requestUpdate();
}

bool VulkanViewport::updateObject(n3d::ObjectId id, const n3d::ObjectData& data)
{
    if (!scene_.update(id, data)) return false;
    clickPending_ = false;
    requestUpdate();
    return true;
}

VulkanViewport::VulkanViewport()
{
    animation_.setInterval(16);
    connect(&animation_, &QTimer::timeout, this, [this] {
        const auto milliseconds = elapsed_.restart();
        if (!isExposed()) return;
        modelAngle = std::fmod(modelAngle + float(milliseconds)*0.03f, 360.0f);
        requestUpdate();
    });
}

QMatrix4x4 VulkanViewport::modelViewProjection()
{
    const auto size = swapChainImageSize();
    QMatrix4x4 model;
    model.rotate(modelAngle, 0, 1, 0); // Bottom centre remains fixed at the origin.
    return clipCorrectionMatrix() * camera.projection(float(size.width())/float(std::max(1,size.height())))
        * camera.view() * model;
}

void VulkanViewport::resetView()
{
    camera.reset(); modelAngle = 0;
    animation_.stop(); emit rotationChanged(false); requestUpdate();
    if (placementMouse_) updatePlacement(*placementMouse_,QGuiApplication::keyboardModifiers());
}

void VulkanViewport::toggleRotation()
{
    if (isPlacing()) return;
    if (animation_.isActive()) animation_.stop();
    else { elapsed_.start(); animation_.start(); }
    emit rotationChanged(animation_.isActive());
}

void VulkanViewport::mousePressEvent(QMouseEvent* event)
{
    clickPending_ = false;
    overlayPress_ = false;
    if(event->button()==Qt::LeftButton && showCompass) {
        const auto rect=Guides::compassRect(swapChainImageSize(),devicePixelRatio());
        const QPointF pixel=event->position()*devicePixelRatio();
        const QPointF home(rect.x()+rect.width()*0.89,rect.y()+rect.height()*0.10);
        if (rect.contains(pixel.toPoint())) overlayPress_ = true;
        if(std::abs(pixel.x()-home.x())<rect.width()*0.09 && std::abs(pixel.y()-home.y())<rect.height()*0.09) {
            resetView(); event->accept(); return;
        }
        if (overlayPress_) { event->accept(); return; }
    }
    if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) {
        previousMouse_ = pressPosition_ = event->position();
        clickPending_ = event->button() == Qt::LeftButton && !event->buttons().testFlag(Qt::MiddleButton);
        requestActivate(); event->accept(); return;
    }
    QVulkanWindow::mousePressEvent(event);
}

void VulkanViewport::mouseMoveEvent(QMouseEvent* event)
{
    if (isPlacing() && event->buttons() == Qt::NoButton) {
        updatePlacement(event->position(),event->modifiers()); event->accept(); return;
    }
    if (event->buttons().testFlag(Qt::MiddleButton) || event->buttons().testFlag(Qt::LeftButton)) {
        if (overlayPress_) { event->accept(); return; }
        if (clickPending_) {
            if ((event->position()-pressPosition_).manhattanLength() < QGuiApplication::styleHints()->startDragDistance()) {
                event->accept(); return;
            }
            clickPending_ = false;
        }
        const auto delta = event->position()-previousMouse_;
        previousMouse_ = event->position();
        const bool left=event->buttons().testFlag(Qt::LeftButton) && !event->buttons().testFlag(Qt::MiddleButton);
        if(left && navigation==Navigation::Zoom) camera.zoom(float(-delta.y())/60.0f);
        else if ((left && navigation==Navigation::Pan) || event->modifiers().testFlag(Qt::ShiftModifier)) camera.pan(float(delta.x()),float(delta.y()),height());
        else camera.orbit(float(delta.x()),float(delta.y()));
        if (isPlacing()) updatePlacement(event->position(),event->modifiers());
        requestUpdate(); event->accept(); return;
    }
    QVulkanWindow::mouseMoveEvent(event);
}

void VulkanViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) {
        if (event->button() == Qt::LeftButton && clickPending_ && !overlayPress_
            && event->buttons() == Qt::NoButton
            && (event->position()-pressPosition_).manhattanLength() < QGuiApplication::styleHints()->startDragDistance()) {
            const auto corner = Guides::compassRect(swapChainImageSize(), devicePixelRatio());
            if (!showCompass || !corner.contains((event->position()*devicePixelRatio()).toPoint())) {
                if (isPlacing()) {
                    updatePlacement(event->position(),event->modifiers());
                    if (preview_) {
                        const auto object = *preview_;
                        cancelPlacement();
                        emit objectPlaced(object);
                    }
                } else selectObject(ScenePicking::pick(scene_, modelViewProjection(), event->position(), QSizeF(width(),height())));
            }
        }
        clickPending_ = false; overlayPress_ = false;
        event->accept(); return;
    }
    QVulkanWindow::mouseReleaseEvent(event);
}

void VulkanViewport::wheelEvent(QWheelEvent* event)
{
    camera.zoom(float(event->angleDelta().y())/120.0f);
    if (isPlacing()) updatePlacement(event->position(),event->modifiers());
    requestUpdate(); event->accept();
}

void VulkanViewport::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) { clickPending_ = false; if (isPlacing()) cancelPlacement(); else selectObject(0); event->accept(); return; }
    if (isPlacing() && event->key() == Qt::Key_Control && placementMouse_) {
        updatePlacement(*placementMouse_,event->modifiers() | Qt::ControlModifier); event->accept(); return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) { toggleRotation(); event->accept(); return; }
    if (event->key() == Qt::Key_F) { resetView(); event->accept(); return; }
    QVulkanWindow::keyPressEvent(event);
}

void VulkanViewport::keyReleaseEvent(QKeyEvent* event)
{
    if (isPlacing() && event->key() == Qt::Key_Control && placementMouse_) {
        updatePlacement(*placementMouse_,event->modifiers() & ~Qt::ControlModifier); event->accept(); return;
    }
    QVulkanWindow::keyReleaseEvent(event);
}
