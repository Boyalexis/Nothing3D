#pragma once

#include <QVulkanWindow>
#include <vector>

// Narrow adapter for Qt 6.10.3's frame-indexed present semaphores.
// All private Qt access stays in the .cpp. Re-audit when changing Qt version.
class QtPresentSync final
{
public:
    void initialize(QVulkanWindow* window);
    void selectImage(QVulkanWindow* window);
    void release(QVulkanWindow* window);

private:
    struct Semaphores {
        VkSemaphore draw = VK_NULL_HANDLE;
        VkSemaphore transfer = VK_NULL_HANDLE;
    };
    std::vector<Semaphores> originalFrames_;
    std::vector<Semaphores> images_;
};
