#include "application.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace application {
namespace {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

const std::array<Vertex, 5> vertices{{
    {{-1, -1, -1}, {1, 0, 0}},
    {{ 1, -1, -1}, {0, 1, 0}},
    {{ 1, -1,  1}, {0, 0, 1}},
    {{-1, -1,  1}, {1, 1, 0}},
    {{ 0,  1,  0}, {1, 0, 1}}
}};

const std::array<uint32_t, 18> indices{{
    0, 1, 2, 0, 2, 3,
    0, 4, 1, 1, 4, 2, 2, 4, 3, 3, 4, 0
}};

VkBuffer vertexBuffer = VK_NULL_HANDLE;
VkBuffer indexBuffer = VK_NULL_HANDLE;
VmaAllocation vertexAllocation = VK_NULL_HANDLE;
VmaAllocation indexAllocation = VK_NULL_HANDLE;
VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
VkPipeline graphicsPipeline = VK_NULL_HANDLE;
glm::vec3 position{0};
glm::vec3 rotation{0};
float scale = 1;
bool perspective = true;

void check(VkResult result, const char* message) {
    if (result != VK_SUCCESS) throw std::runtime_error(message);
}

void uploadBuffer(const void* data, size_t size, VkBufferUsageFlags usage,
                  VkBuffer& buffer, VmaAllocation& allocation) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo memory{};
    memory.usage = VMA_MEMORY_USAGE_AUTO;
    memory.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo mapped{};
    check(vmaCreateBuffer(graphics::internal::context.allocator, &info, &memory,
                          &buffer, &allocation, &mapped), "Cannot create geometry buffer");
    std::memcpy(mapped.pMappedData, data, size);
    check(vmaFlushAllocation(graphics::internal::context.allocator, allocation, 0, size),
          "Cannot flush geometry buffer");
}

VkShaderModule loadShader(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error(std::string("Cannot open shader: ") + path);
    const auto size = file.tellg();
    if (size <= 0 || size % 4 != 0) throw std::runtime_error("Invalid SPIR-V size");
    std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(code.data()), size))
        throw std::runtime_error("Cannot read shader");
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = static_cast<size_t>(size);
    info.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    check(vkCreateShaderModule(graphics::internal::context.device, &info, nullptr, &module),
          "Cannot create shader module");
    return module;
}

}

bool initialize() {
    const auto device = graphics::internal::context.device;
    VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
    try {
        uploadBuffer(vertices.data(), sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     vertexBuffer, vertexAllocation);
        uploadBuffer(indices.data(), sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     indexBuffer, indexAllocation);
        vert = loadShader("shaders/pyramid.vert.spv");
        frag = loadShader("shaders/pyramid.frag.spv");

        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        VkPushConstantRange transform{};
        transform.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        transform.size = sizeof(glm::mat4);
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &transform;
        check(vkCreatePipelineLayout(device, &layout, nullptr, &pipelineLayout),
              "Cannot create pipeline layout");

        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = "main";
        VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        std::array<VkVertexInputAttributeDescription, 2> attributes{{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}
        }};
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = 1;
        input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        input.pVertexAttributeDescriptions = attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo samples{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable = depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState color{};
        color.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &color;
        std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();
        VkGraphicsPipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipeline.stageCount = static_cast<uint32_t>(stages.size());
        pipeline.pStages = stages.data();
        pipeline.pVertexInputState = &input;
        pipeline.pInputAssemblyState = &assembly;
        pipeline.pViewportState = &viewport;
        pipeline.pRasterizationState = &raster;
        pipeline.pMultisampleState = &samples;
        pipeline.pDepthStencilState = &depth;
        pipeline.pColorBlendState = &blend;
        pipeline.pDynamicState = &dynamic;
        pipeline.layout = pipelineLayout;
        pipeline.renderPass = graphics::internal::context.render_pass;
        check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline, nullptr,
                                        &graphicsPipeline), "Cannot create graphics pipeline");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        vkDestroyShaderModule(device, vert, nullptr);
        vkDestroyShaderModule(device, frag, nullptr);
        shutdown();
        return false;
    }
    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
    return true;
}

void shutdown() {
    const auto& context = graphics::internal::context;
    vkDeviceWaitIdle(context.device);
    vkDestroyPipeline(context.device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
    if (indexBuffer) vmaDestroyBuffer(context.allocator, indexBuffer, indexAllocation);
    if (vertexBuffer) vmaDestroyBuffer(context.allocator, vertexBuffer, vertexAllocation);
    graphicsPipeline = VK_NULL_HANDLE;
    pipelineLayout = VK_NULL_HANDLE;
    indexBuffer = vertexBuffer = VK_NULL_HANDLE;
}

void update([[maybe_unused]] double time) {
    const auto& io = ImGui::GetIO();
    const float step = std::min(io.DeltaTime, 0.1f);
    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) rotation.y -= step;
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) rotation.y += step;
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) rotation.x -= step;
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) rotation.x += step;
        if (ImGui::IsKeyDown(ImGuiKey_A)) position.x -= step;
        if (ImGui::IsKeyDown(ImGuiKey_D)) position.x += step;
        if (ImGui::IsKeyDown(ImGuiKey_W)) position.y += step;
        if (ImGui::IsKeyDown(ImGuiKey_S)) position.y -= step;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) scale -= step;
        if (ImGui::IsKeyDown(ImGuiKey_E)) scale += step;
        scale = std::clamp(scale, 0.2f, 2.0f);
        if (ImGui::IsKeyPressed(ImGuiKey_P, false)) perspective = !perspective;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            position = rotation = glm::vec3(0);
            scale = 1;
            perspective = true;
        }
    }

    ImGui::Begin("Lab 1: Square pyramid");
    ImGui::TextUnformatted("Variant 6 | 5 vertices | 6 triangles");
    ImGui::Text("Projection: %s", perspective ? "Perspective" : "Orthographic");
    ImGui::TextWrapped("Arrows: rotate | WASD: move model\nQ/E: scale | P: projection | R: reset");
    ImGui::End();
}

void render(const graphics::internal::FrameData& fd) {
    const auto& context = graphics::internal::context;
    const auto extent = context.swapchain_extent;
    const float aspect = static_cast<float>(extent.width) / extent.height;
    glm::mat4 model = glm::translate(glm::mat4(1), position);
    model = glm::rotate(model, rotation.y, glm::vec3(0, 1, 0));
    model = glm::rotate(model, rotation.x, glm::vec3(1, 0, 0));
    model = glm::scale(model, glm::vec3(scale));
    const glm::mat4 view = glm::lookAtRH(glm::vec3(4, 3, 5), glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 projection = perspective
        ? glm::perspectiveRH_ZO(glm::radians(45.0f), aspect, 0.1f, 100.0f)
        : glm::orthoRH_ZO(-3.0f * aspect, 3.0f * aspect, -3.0f, 3.0f, 0.1f, 100.0f);
    projection[1][1] *= -1;
    const glm::mat4 mvp = projection * view * model;

    check(vkResetCommandBuffer(fd.command_buffer, 0), "Cannot reset command buffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    check(vkBeginCommandBuffer(fd.command_buffer, &begin), "Cannot begin command buffer");
    std::array<VkClearValue, 2> clear{};
    clear[0].color = {{0.06f, 0.08f, 0.12f, 1}};
    clear[1].depthStencil = {1, 0};
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = context.render_pass;
    pass.framebuffer = fd.framebuffer;
    pass.renderArea.extent = extent;
    pass.clearValueCount = static_cast<uint32_t>(clear.size());
    pass.pClearValues = clear.data();
    vkCmdBeginRenderPass(fd.command_buffer, &pass, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);
    vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vertexBuffer, &offset);
    vkCmdBindIndexBuffer(fd.command_buffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(fd.command_buffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(mvp), &mvp);
    vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    vkCmdEndRenderPass(fd.command_buffer);
    check(vkEndCommandBuffer(fd.command_buffer), "Cannot end command buffer");
}

}
