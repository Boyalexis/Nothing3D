#include "render/VulkanViewport.h"
#include <QVulkanFunctions>
#include "render/QtPresentSync.h"
#include "render/ColorMesh.h"
#include "render/GuideGeometry.h"
#include "render/ViewCubeGeometry.h"
#include "render/InfiniteAxis.h"
#include "render/CylinderGeometry.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

namespace {
class BoxRenderer final : public QVulkanWindowRenderer
{
public:
    explicit BoxRenderer(VulkanViewport* window) : window_(window) {}

    void initResources() override
    {
        functions_ = window_->vulkanInstance()->deviceFunctions(window_->device());
        box_.initialize(window_, demoVertices());
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
        box_.createPipeline();
        gridOnly_.createPipeline(true,1); axesOnly_.createPipeline(true,2);
        ground_.createPipeline(true); axis_.createPipeline(); compass_.createPipeline(); letters_.createPipeline(); home_.createPipeline();
        ++window_->swapchainGenerations;
        const auto* gpu = window_->physicalDeviceProperties();
        const auto size = window_->swapChainImageSize();
        emit window_->rendererStatus(QStringLiteral("长方体 + 圆柱体 · %1 · %2 × %3 · 验证层：%4 · 中键旋转 / 滚轮缩放")
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
        box_.draw(command, window_->modelViewProjection());
        const auto world = window_->clipCorrectionMatrix()
            * window_->camera.projection(float(size.width())/float(std::max(1,size.height())))
            * window_->camera.view();
        if(window_->showGrid && window_->showAxes) ground_.drawGrid(command,world);
        else if(window_->showGrid) gridOnly_.drawGrid(command,world);
        else if(window_->showAxes) axesOnly_.drawGrid(command,world);
        if(window_->showAxes) if(const auto axisTransform=infiniteYAxis(world)) axis_.draw(command,*axisTransform);
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
        gridOnly_.release(); axesOnly_.release();
        box_.release(); ground_.release(); axis_.release(); compass_.release(); letters_.release(); home_.release(); functions_ = nullptr;
    }
    void releaseSwapChainResources() override {
        gridOnly_.releasePipeline(); axesOnly_.releasePipeline();
        box_.releasePipeline(); ground_.releasePipeline(); axis_.releasePipeline(); compass_.releasePipeline(); letters_.releasePipeline(); home_.releasePipeline();
        sync_.release(window_);
    }
private:
    ColorMesh box_, ground_, axis_, compass_, letters_, home_;
    ColorMesh gridOnly_, axesOnly_;
    ViewCube::Labels labelGeometry_;
    QtPresentSync sync_;
    VulkanViewport* window_;
    QVulkanDeviceFunctions* functions_ = nullptr;
};
}

QVulkanWindowRenderer* VulkanViewport::createRenderer()
{
    return new BoxRenderer(this);
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
}

void VulkanViewport::toggleRotation()
{
    if (animation_.isActive()) animation_.stop();
    else { elapsed_.start(); animation_.start(); }
    emit rotationChanged(animation_.isActive());
}

void VulkanViewport::mousePressEvent(QMouseEvent* event)
{
    if(event->button()==Qt::LeftButton && showCompass) {
        const auto rect=Guides::compassRect(swapChainImageSize(),devicePixelRatio());
        const QPointF pixel=event->position()*devicePixelRatio();
        const QPointF home(rect.x()+rect.width()*0.89,rect.y()+rect.height()*0.10);
        if(std::abs(pixel.x()-home.x())<rect.width()*0.09 && std::abs(pixel.y()-home.y())<rect.height()*0.09) {
            resetView(); event->accept(); return;
        }
    }
    if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) {
        previousMouse_ = event->position(); requestActivate(); event->accept(); return;
    }
    QVulkanWindow::mousePressEvent(event);
}

void VulkanViewport::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons().testFlag(Qt::MiddleButton) || event->buttons().testFlag(Qt::LeftButton)) {
        const auto delta = event->position()-previousMouse_;
        previousMouse_ = event->position();
        const bool left=event->buttons().testFlag(Qt::LeftButton) && !event->buttons().testFlag(Qt::MiddleButton);
        if(left && navigation==Navigation::Zoom) camera.zoom(float(-delta.y())/60.0f);
        else if ((left && navigation==Navigation::Pan) || event->modifiers().testFlag(Qt::ShiftModifier)) camera.pan(float(delta.x()),float(delta.y()),height());
        else camera.orbit(float(delta.x()),float(delta.y()));
        requestUpdate(); event->accept(); return;
    }
    QVulkanWindow::mouseMoveEvent(event);
}

void VulkanViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) { event->accept(); return; }
    QVulkanWindow::mouseReleaseEvent(event);
}

void VulkanViewport::wheelEvent(QWheelEvent* event)
{
    camera.zoom(float(event->angleDelta().y())/120.0f);
    requestUpdate(); event->accept();
}

void VulkanViewport::keyPressEvent(QKeyEvent* event)
{
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) { toggleRotation(); event->accept(); return; }
    if (event->key() == Qt::Key_F) { resetView(); event->accept(); return; }
    QVulkanWindow::keyPressEvent(event);
}
