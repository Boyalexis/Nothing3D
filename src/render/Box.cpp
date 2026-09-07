#include "render/Box.h"
#include "render/BoxGeometry.h"
#include <QVulkanFunctions>
#include <QFile>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

namespace {
void check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) qFatal("%s failed: %d", operation, int(result));
}
using Vertex = BoxVertex;
const auto vertices = boxVertices();
}

void Box::initialize(QVulkanWindow* window) {
    window_ = window;
    functions_ = window->vulkanInstance()->deviceFunctions(window->device());
    VkBufferCreateInfo buffer{};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = sizeof(vertices);
    buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(functions_->vkCreateBuffer(window->device(), &buffer, nullptr, &vertices_), "Create vertex buffer");
    VkMemoryRequirements requirements{};
    functions_->vkGetBufferMemoryRequirements(window->device(), vertices_, &requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    window->vulkanInstance()->functions()->vkGetPhysicalDeviceMemoryProperties(window->physicalDevice(), &properties);
    uint32_t memoryType = UINT32_MAX;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        const auto required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((requirements.memoryTypeBits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & required) == required) {
            memoryType = i;
            break;
        }
    }
    if (memoryType == UINT32_MAX) qFatal("No host-visible coherent memory for box.");
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    check(functions_->vkAllocateMemory(window->device(), &allocation, nullptr, &memory_), "Allocate vertex memory");
    check(functions_->vkBindBufferMemory(window->device(), vertices_, memory_, 0), "Bind vertex memory");
    void* mapped = nullptr;
    check(functions_->vkMapMemory(window->device(), memory_, 0, sizeof(vertices), 0, &mapped), "Map vertex memory");
    std::memcpy(mapped, vertices.data(), sizeof(vertices));
    functions_->vkUnmapMemory(window->device(), memory_);
}

VkShaderModule Box::loadShader(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) qFatal("Cannot open shader: %s", qPrintable(path));
    const auto bytes = file.readAll();
    if (bytes.isEmpty() || bytes.size() % 4 != 0) qFatal("Invalid SPIR-V byte count.");
    // Aligned storage for Vulkan's uint32_t shader code.
    std::vector<uint32_t> words(size_t(bytes.size()) / 4);
    std::memcpy(words.data(), bytes.constData(), size_t(bytes.size()));
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = size_t(bytes.size()); info.pCode = words.data();
    VkShaderModule module = VK_NULL_HANDLE;
    check(functions_->vkCreateShaderModule(window_->device(), &info, nullptr, &module), "Create shader module");
    return module;
}

void Box::createPipeline() {
    const auto vertex = loadShader(QStringLiteral(":/shaders/box.vert.spv"));
    const auto fragment = loadShader(QStringLiteral(":/shaders/box.frag.spv"));
    VkPipelineShaderStageCreateInfo stages[2]{};
    for (auto& stage : stages) { stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName = "main"; }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertex;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
    VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}
    };
    VkPipelineVertexInputStateCreateInfo input{};
    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.vertexBindingDescriptionCount = 1; input.pVertexBindingDescriptions = &binding;
    input.vertexAttributeDescriptionCount = 2; input.pVertexAttributeDescriptions = attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1; viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_NONE; raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = window_->sampleCountFlagBits();
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1; blend.pAttachments = &attachment;
    const VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPushConstantRange transform{VK_SHADER_STAGE_VERTEX_BIT, 0, 64};
    layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &transform;
    check(functions_->vkCreatePipelineLayout(window_->device(), &layout, nullptr, &layout_), "Create pipeline layout");
    VkGraphicsPipelineCreateInfo pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2; pipeline.pStages = stages;
    pipeline.pVertexInputState = &input; pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport; pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &multisample; pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &blend; pipeline.pDynamicState = &dynamic;
    pipeline.layout = layout_; pipeline.renderPass = window_->defaultRenderPass();
    const auto result = functions_->vkCreateGraphicsPipelines(window_->device(), VK_NULL_HANDLE, 1, &pipeline, nullptr, &pipeline_);
    functions_->vkDestroyShaderModule(window_->device(), vertex, nullptr);
    functions_->vkDestroyShaderModule(window_->device(), fragment, nullptr);
    check(result, "Create box pipeline");
}

void Box::draw(VkCommandBuffer command, const QMatrix4x4& mvp) {
    const auto size = window_->swapChainImageSize();
    VkViewport viewport{0, 0, float(size.width()), float(size.height()), 0, 1};
    VkRect2D scissor{{0, 0}, {uint32_t(size.width()), uint32_t(size.height())}};
    functions_->vkCmdSetViewport(command, 0, 1, &viewport);
    functions_->vkCmdSetScissor(command, 0, 1, &scissor);
    functions_->vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    const VkDeviceSize offset = 0;
    functions_->vkCmdBindVertexBuffers(command, 0, 1, &vertices_, &offset);
    functions_->vkCmdPushConstants(command, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, mvp.constData());
    functions_->vkCmdDraw(command, uint32_t(vertices.size()), 1, 0, 0);
}

void Box::releasePipeline() {
    if (pipeline_) functions_->vkDestroyPipeline(window_->device(), pipeline_, nullptr);
    if (layout_) functions_->vkDestroyPipelineLayout(window_->device(), layout_, nullptr);
    pipeline_ = VK_NULL_HANDLE; layout_ = VK_NULL_HANDLE;
}
void Box::release() {
    if (vertices_) functions_->vkDestroyBuffer(window_->device(), vertices_, nullptr);
    if (memory_) functions_->vkFreeMemory(window_->device(), memory_, nullptr);
    vertices_ = VK_NULL_HANDLE; memory_ = VK_NULL_HANDLE;
}
