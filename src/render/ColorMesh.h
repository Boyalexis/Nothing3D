#pragma once
#include <QVulkanWindow>
#include <QRect>
#include <span>
#include "render/BoxGeometry.h"

class ColorMesh final {
public:
    void initialize(QVulkanWindow* window, std::span<const BoxVertex> vertices,
                    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    bool depthTest = true, bool depthWrite = true);
    void createPipeline(bool grid = false, int gridMode = 0);
    void drawGrid(VkCommandBuffer command, const QMatrix4x4& viewProjection);
    void draw(VkCommandBuffer command, const QMatrix4x4& mvp, const QRect& area = {},
              uint32_t first = 0, uint32_t count = 0);
    void releasePipeline();
    void release();
private:
    uint32_t vertexCount_ = 0;
    VkPrimitiveTopology topology_ = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool depthTest_ = true, depthWrite_ = true;
    bool grid_ = false;
    VkShaderModule loadShader(const QString& path);
    QVulkanWindow* window_ = nullptr;
    QVulkanDeviceFunctions* functions_ = nullptr;
    VkBuffer vertices_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};
