#include "render/QtPresentSync.h"
#include <QVulkanFunctions>
#include <QtGui/private/qvulkanwindow_p.h>
#include <cstring>

static_assert(QT_VERSION == QT_VERSION_CHECK(6, 10, 3),
              "Re-audit QtPresentSync against the new Qt source before upgrading.");

namespace {
QVulkanWindowPrivate* state(QVulkanWindow* window)
{
    return static_cast<QVulkanWindowPrivate*>(QObjectPrivate::get(window));
}
}

void QtPresentSync::initialize(QVulkanWindow* window)
{
    if (std::strcmp(qVersion(), "6.10.3") != 0)
        qFatal("QtPresentSync requires the audited Qt 6.10.3 runtime.");
    if (!originalFrames_.empty() || !images_.empty())
        qFatal("QtPresentSync initialized without releasing the previous swapchain.");

    auto* d = state(window);
    originalFrames_.resize(d->frameLag);
    for (int i = 0; i < d->frameLag; ++i)
        originalFrames_[i] = {d->frameRes[i].drawSem, d->frameRes[i].presTransSem};

    images_.resize(window->swapChainImageCount());
    auto* functions = window->vulkanInstance()->deviceFunctions(window->device());
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (auto& image : images_) {
        VkResult result = functions->vkCreateSemaphore(window->device(), &info, nullptr, &image.draw);
        if (result == VK_SUCCESS && d->gfxQueueFamilyIdx != d->presQueueFamilyIdx)
            result = functions->vkCreateSemaphore(window->device(), &info, nullptr, &image.transfer);
        if (result != VK_SUCCESS) {
            release(window);
            qFatal("Cannot allocate present semaphores: %d", int(result));
        }
    }
}

void QtPresentSync::selectImage(QVulkanWindow* window)
{
    auto* d = state(window);
    const int index = window->currentSwapChainImageIndex();
    if (index < 0 || size_t(index) >= images_.size())
        qFatal("Invalid swapchain image in QtPresentSync.");

    // Qt has acquired this image and its submit will wait on imageSem.
    // That wait protects reuse of THIS image's previous present semaphore.
    // The frame's CPU fence alone cannot protect presentation completion.
    auto& frame = d->frameRes[window->currentFrame()];
    frame.drawSem = images_[index].draw;
    frame.presTransSem = images_[index].transfer;
}

void QtPresentSync::release(QVulkanWindow* window)
{
    if (originalFrames_.empty()) return;
    auto* d = state(window);
    // Qt 6.10.3 calls vkDeviceWaitIdle before releaseSwapChainResources().
    // Restore Qt-owned handles before Qt destroys them, avoiding double frees.
    for (size_t i = 0; i < originalFrames_.size(); ++i) {
        d->frameRes[i].drawSem = originalFrames_[i].draw;
        d->frameRes[i].presTransSem = originalFrames_[i].transfer;
    }
    auto* functions = window->vulkanInstance()->deviceFunctions(window->device());
    for (const auto& image : images_) {
        if (image.draw) functions->vkDestroySemaphore(window->device(), image.draw, nullptr);
        if (image.transfer) functions->vkDestroySemaphore(window->device(), image.transfer, nullptr);
    }
    images_.clear();
    originalFrames_.clear();
}
