#include "application.hpp"
#include "lab2_scene.hpp"

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

using lab2::Vertex;

struct alignas(16) SceneUniforms {
    glm::mat4 model;
    glm::mat4 viewProjection;
    glm::mat4 normalMatrix;
    glm::vec4 cameraPosition;
    glm::vec4 lightDirection;
    glm::vec4 lightColorIntensity;
    glm::vec4 ambientColorIntensity;
    glm::vec4 albedo;
    glm::vec4 specularShininess;
};
static_assert(sizeof(SceneUniforms) == 288);
static_assert(offsetof(SceneUniforms, cameraPosition) == 192);
static_assert(offsetof(SceneUniforms, specularShininess) == 272);

VkBuffer vertexBuffer = VK_NULL_HANDLE;
VkBuffer indexBuffer = VK_NULL_HANDLE;
VmaAllocation vertexAllocation = VK_NULL_HANDLE;
VmaAllocation indexAllocation = VK_NULL_HANDLE;
VkBuffer uniformBuffer = VK_NULL_HANDLE;
VmaAllocation uniformAllocation = VK_NULL_HANDLE;
void* uniformMapped = nullptr;
VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
VkPipeline graphicsPipeline = VK_NULL_HANDLE;
uint32_t indexCount = 0;
lab2::Camera camera;
glm::vec3 position{0};
glm::vec3 rotation{0};
float scale = 1;
float cameraSpeed = 3;
glm::vec3 lightDirection{0.5f, -1.0f, -0.6f};
glm::vec3 lightColor{1.0f, 0.95f, 0.85f};
float lightIntensity = 1;
glm::vec3 ambientColor{1};
float ambientIntensity = 0.12f;
glm::vec3 albedo{0.72f, 0.42f, 0.16f};
glm::vec3 specularColor{0.9f};
float shininess = 32;

void resetScene() {
    camera = lab2::Camera{};
    position = rotation = glm::vec3(0);
    scale = 1;
    cameraSpeed = 3;
    lightDirection = {0.5f, -1.0f, -0.6f};
    lightColor = {1.0f, 0.95f, 0.85f};
    lightIntensity = 1;
    ambientColor = glm::vec3(1);
    ambientIntensity = 0.12f;
    albedo = {0.72f, 0.42f, 0.16f};
    specularColor = glm::vec3(0.9f);
    shininess = 32;
}

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

void createUniformResources() {
    const auto& context = graphics::internal::context;
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = sizeof(SceneUniforms);
    buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo memory{};
    memory.usage = VMA_MEMORY_USAGE_AUTO;
    memory.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo allocationInfo{};
    check(vmaCreateBuffer(context.allocator, &buffer, &memory,
                          &uniformBuffer, &uniformAllocation, &allocationInfo),
          "Cannot create scene uniform buffer");
    uniformMapped = allocationInfo.pMappedData;

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount = 1;
    layout.pBindings = &binding;
    check(vkCreateDescriptorSetLayout(context.device, &layout, nullptr, &descriptorSetLayout),
          "Cannot create scene descriptor layout");

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.maxSets = 1;
    pool.poolSizeCount = 1;
    pool.pPoolSizes = &poolSize;
    check(vkCreateDescriptorPool(context.device, &pool, nullptr, &descriptorPool),
          "Cannot create scene descriptor pool");
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = descriptorPool;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &descriptorSetLayout;
    check(vkAllocateDescriptorSets(context.device, &allocate, &descriptorSet),
          "Cannot allocate scene descriptor set");

    VkDescriptorBufferInfo bufferInfo{uniformBuffer, 0, sizeof(SceneUniforms)};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
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
        const auto geometry = lab2::makePyramid();
        indexCount = static_cast<uint32_t>(geometry.indices.size());
        uploadBuffer(geometry.vertices.data(), geometry.vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     vertexBuffer, vertexAllocation);
        uploadBuffer(geometry.indices.data(), geometry.indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     indexBuffer, indexAllocation);
        createUniformResources();
        vert = loadShader("shaders/pyramid.vert.spv");
        frag = loadShader("shaders/pyramid.frag.spv");

        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &descriptorSetLayout;
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
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)}
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
    vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(context.device, descriptorSetLayout, nullptr);
    if (uniformBuffer) vmaDestroyBuffer(context.allocator, uniformBuffer, uniformAllocation);
    if (indexBuffer) vmaDestroyBuffer(context.allocator, indexBuffer, indexAllocation);
    if (vertexBuffer) vmaDestroyBuffer(context.allocator, vertexBuffer, vertexAllocation);
    graphicsPipeline = VK_NULL_HANDLE;
    pipelineLayout = VK_NULL_HANDLE;
    descriptorPool = VK_NULL_HANDLE;
    descriptorSetLayout = VK_NULL_HANDLE;
    descriptorSet = VK_NULL_HANDLE;
    uniformBuffer = VK_NULL_HANDLE;
    uniformMapped = nullptr;
    indexBuffer = vertexBuffer = VK_NULL_HANDLE;
}

void update([[maybe_unused]] double time) {
    const auto& io = ImGui::GetIO();
    const float step = std::min(io.DeltaTime, 0.1f);
    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) camera.yaw -= 60 * step;
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) camera.yaw += 60 * step;
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) camera.pitch += 60 * step;
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) camera.pitch -= 60 * step;
        camera.pitch = std::clamp(camera.pitch, -89.0f, 89.0f);
        glm::vec3 movement(0);
        if (ImGui::IsKeyDown(ImGuiKey_W)) movement += camera.forward();
        if (ImGui::IsKeyDown(ImGuiKey_S)) movement -= camera.forward();
        if (ImGui::IsKeyDown(ImGuiKey_A)) movement -= camera.right();
        if (ImGui::IsKeyDown(ImGuiKey_D)) movement += camera.right();
        if (ImGui::IsKeyDown(ImGuiKey_Q)) movement.y -= 1;
        if (ImGui::IsKeyDown(ImGuiKey_E)) movement.y += 1;
        if (glm::dot(movement, movement) > 0.0001f)
            camera.position += glm::normalize(movement) * cameraSpeed * step;
        if (ImGui::IsKeyPressed(ImGuiKey_P, false)) camera.perspective = !camera.perspective;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) resetScene();
    }
    if (!io.WantCaptureMouse && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        camera.yaw += io.MouseDelta.x * 0.2f;
        camera.pitch = std::clamp(camera.pitch - io.MouseDelta.y * 0.2f, -89.0f, 89.0f);
    }
    camera.yaw = std::remainder(camera.yaw, 360.0f);

    ImGui::SetNextWindowPos(ImVec2(14, 14), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(370, 650), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lab 2: Camera and Lighting");
    ImGui::TextUnformatted("Variant 6: square pyramid | 6 triangles");
    ImGui::TextUnformatted("One directional light | Blinn-Phong");
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position##camera", &camera.position.x, 0.05f);
        ImGui::SliderFloat("Yaw", &camera.yaw, -180, 180, "%.1f deg");
        ImGui::SliderFloat("Pitch", &camera.pitch, -89, 89, "%.1f deg");
        ImGui::SliderFloat("Roll", &camera.roll, -180, 180, "%.1f deg");
        ImGui::Checkbox("Perspective", &camera.perspective);
        if (camera.perspective)
            ImGui::SliderFloat("Field of view", &camera.fov, 10, 120, "%.1f deg");
        else
            ImGui::SliderFloat("Ortho half-height", &camera.orthoHalfHeight, 0.2f, 15);
        ImGui::DragFloat("Near plane", &camera.nearPlane, 0.01f, 0.01f, 10);
        ImGui::DragFloat("Far plane", &camera.farPlane, 0.5f, 1, 1000);
        camera.nearPlane = std::clamp(camera.nearPlane, 0.01f, 10.0f);
        camera.farPlane = std::clamp(camera.farPlane, camera.nearPlane + 0.1f, 1000.0f);
        ImGui::SliderFloat("Move speed", &cameraSpeed, 0.2f, 10);
        if (ImGui::Button("Reset camera")) camera = lab2::Camera{};
    }
    if (ImGui::CollapsingHeader("Directional light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Ray direction", &lightDirection.x, 0.02f, -1, 1);
        if (glm::dot(lightDirection, lightDirection) < 0.000001f)
            ImGui::TextUnformatted("Zero direction uses (0, -1, 0).");
        ImGui::ColorEdit3("Light color", &lightColor.x);
        ImGui::SliderFloat("Light intensity", &lightIntensity, 0, 5);
    }
    if (ImGui::CollapsingHeader("Ambient light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Ambient color", &ambientColor.x);
        ImGui::SliderFloat("Ambient intensity", &ambientIntensity, 0, 1);
    }
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Albedo", &albedo.x);
        ImGui::ColorEdit3("Specular color", &specularColor.x);
        ImGui::SliderFloat("Shininess", &shininess, 1, 256, "%.1f", ImGuiSliderFlags_Logarithmic);
    }
    if (ImGui::CollapsingHeader("Model transform")) {
        ImGui::DragFloat3("Position##model", &position.x, 0.02f);
        ImGui::SliderFloat3("Rotation", &rotation.x, -180, 180, "%.1f deg");
        ImGui::SliderFloat("Scale", &scale, 0.2f, 2);
        if (ImGui::Button("Reset model")) {
            position = rotation = glm::vec3(0);
            scale = 1;
        }
    }
    if (ImGui::Button("Reset all (R)")) resetScene();
    ImGui::TextWrapped("WASD: move camera | Q/E: down/up\nArrows or right drag: look\nP: projection | R: reset all");
    ImGui::End();
    scale = std::clamp(scale, 0.2f, 2.0f);
    cameraSpeed = std::clamp(cameraSpeed, 0.2f, 10.0f);
    lightIntensity = std::clamp(lightIntensity, 0.0f, 5.0f);
    ambientIntensity = std::clamp(ambientIntensity, 0.0f, 1.0f);
    shininess = std::clamp(shininess, 1.0f, 256.0f);
}

void render(const graphics::internal::FrameData& fd) {
    const auto& context = graphics::internal::context;
    const auto extent = context.swapchain_extent;
    const float aspect = static_cast<float>(extent.width) / extent.height;
    glm::mat4 model = glm::translate(glm::mat4(1), position);
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(scale));

    const SceneUniforms uniforms{
        model,
        camera.projection(aspect) * camera.view(),
        glm::transpose(glm::inverse(model)),
        glm::vec4(camera.position, 1),
        glm::vec4(lightDirection, 0),
        glm::vec4(lightColor, lightIntensity),
        glm::vec4(ambientColor, ambientIntensity),
        glm::vec4(albedo, 1),
        glm::vec4(specularColor, shininess)
    };
    std::memcpy(uniformMapped, &uniforms, sizeof(uniforms));
    check(vmaFlushAllocation(context.allocator, uniformAllocation, 0, sizeof(uniforms)),
          "Cannot flush scene uniform buffer");

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
    vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDrawIndexed(fd.command_buffer, indexCount, 1, 0, 0, 0);
    vkCmdEndRenderPass(fd.command_buffer);
    check(vkEndCommandBuffer(fd.command_buffer), "Cannot end command buffer");
}

}
