#pragma once
#include <QVulkanWindow>
class Triangle final {
public:
    void initialize(QVulkanWindow* window);
    void createPipeline();
    void draw(VkCommandBuffer command);
    void releasePipeline();
    void release();
private:
    VkShaderModule loadShader(const QString& path);
    QVulkanWindow* window_ = nullptr;
    QVulkanDeviceFunctions* functions_ = nullptr;
    VkBuffer vertices_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};
