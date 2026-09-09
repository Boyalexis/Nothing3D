#include "render/ColorMesh.h"
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

}

void ColorMesh::initialize(QVulkanWindow* window, std::span<const BoxVertex> vertices,
                           VkPrimitiveTopology topology, bool depthTest, bool depthWrite) {
    vertexCount_ = uint32_t(vertices.size());
    topology_ = topology; depthTest_ = depthTest; depthWrite_ = depthWrite;
    window_ = window;
    functions_ = window->vulkanInstance()->deviceFunctions(window->device());
    VkBufferCreateInfo buffer{};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = vertices.size_bytes();
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
    check(functions_->vkMapMemory(window->device(), memory_, 0, vertices.size_bytes(), 0, &mapped), "Map vertex memory");
    std::memcpy(mapped, vertices.data(), vertices.size_bytes());
    functions_->vkUnmapMemory(window->device(), memory_);
}

VkShaderModule ColorMesh::loadShader(const QString& path) {
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

void ColorMesh::createPipeline(bool grid, int gridMode) {
    grid_ = grid;
    const auto vertex = loadShader(grid ? QStringLiteral(":/shaders/grid.vert.spv") : QStringLiteral(":/shaders/box.vert.spv"));
    const auto fragment = loadShader(grid ? QStringLiteral(":/shaders/grid.frag.spv") : QStringLiteral(":/shaders/box.frag.spv"));
    VkPipelineShaderStageCreateInfo stages[2]{};
    for (auto& stage : stages) { stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stage.pName = "main"; }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertex;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment;
    const VkSpecializationMapEntry entry{0,0,sizeof(int)};
    const VkSpecializationInfo specialization{1,&entry,sizeof(int),&gridMode};
    if(grid) stages[1].pSpecializationInfo=&specialization;
    VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}
    };
    VkPipelineVertexInputStateCreateInfo input{};
    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.vertexBindingDescriptionCount = 1; input.pVertexBindingDescriptions = &binding;
    input.vertexAttributeDescriptionCount = grid ? 1 : 2; input.pVertexAttributeDescriptions = attributes;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = topology_;
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
    depth.depthTestEnable = depthTest_;
    depth.depthWriteEnable = depthWrite_;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask = 0xf;
    attachment.blendEnable = grid;
    attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.colorBlendOp = VK_BLEND_OP_ADD;
    attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1; blend.pAttachments = &attachment;
    const VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = states;
    VkPipelineLayoutCreateInfo layout{};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPushConstantRange transform{VK_SHADER_STAGE_VERTEX_BIT, 0, 80};
    if(grid) transform = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,0,128};
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

void ColorMesh::draw(VkCommandBuffer command, const QMatrix4x4& mvp, const QRect& area,
                     uint32_t first, uint32_t count, bool selected) {
    const auto size = window_->swapChainImageSize();
    const QRect rect = area.isEmpty() ? QRect(QPoint(0,0), size) : area;
    VkViewport viewport{float(rect.x()), float(rect.y()), float(rect.width()), float(rect.height()), 0, 1};
    VkRect2D scissor{{rect.x(), rect.y()}, {uint32_t(rect.width()), uint32_t(rect.height())}};
    functions_->vkCmdSetViewport(command, 0, 1, &viewport);
    functions_->vkCmdSetScissor(command, 0, 1, &scissor);
    functions_->vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    const VkDeviceSize offset = 0;
    functions_->vkCmdBindVertexBuffers(command, 0, 1, &vertices_, &offset);
    if(!grid_) {
        float constants[20];
        std::memcpy(constants, mvp.constData(), 64);
        constants[16] = 1.0f; constants[17] = 0.8f; constants[18] = 0.15f;
        constants[19] = selected ? 0.6f : 0.0f;
        functions_->vkCmdPushConstants(command, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), constants);
    }
    functions_->vkCmdDraw(command, count ? count : vertexCount_, 1, first, 0);
}

void ColorMesh::drawGrid(VkCommandBuffer command, const QMatrix4x4& viewProjection) {
    const auto inverse=viewProjection.inverted();
    float matrices[32];
    std::memcpy(matrices,inverse.constData(),64);
    std::memcpy(matrices+16,viewProjection.constData(),64);
    functions_->vkCmdPushConstants(command,layout_,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,0,128,matrices);
    draw(command,viewProjection);
}

void ColorMesh::releasePipeline() {
    if (pipeline_) functions_->vkDestroyPipeline(window_->device(), pipeline_, nullptr);
    if (layout_) functions_->vkDestroyPipelineLayout(window_->device(), layout_, nullptr);
    pipeline_ = VK_NULL_HANDLE; layout_ = VK_NULL_HANDLE;
}
void ColorMesh::release() {
    if (vertices_) functions_->vkDestroyBuffer(window_->device(), vertices_, nullptr);
    if (memory_) functions_->vkFreeMemory(window_->device(), memory_, nullptr);
    vertices_ = VK_NULL_HANDLE; memory_ = VK_NULL_HANDLE;
}
