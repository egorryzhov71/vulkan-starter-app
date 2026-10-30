#include "lab2_scene.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(float a, float b, float tolerance = 0.0001f) {
    return std::abs(a - b) < tolerance;
}

void checkGeometry() {
    const auto mesh = lab2::makePyramid();
    require(mesh.indices.size() == 18, "Pyramid must contain six triangles");
    float volume = 0;
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        for (size_t corner = 0; corner < 3; ++corner)
            require(mesh.indices[i + corner] < mesh.vertices.size(), "Index out of bounds");
        const auto& a = mesh.vertices[mesh.indices[i]];
        const auto& b = mesh.vertices[mesh.indices[i + 1]];
        const auto& c = mesh.vertices[mesh.indices[i + 2]];
        const glm::vec3 cross = glm::cross(b.position - a.position, c.position - a.position);
        require(glm::length(cross) > 0, "Degenerate triangle");
        for (const auto* vertex : {&a, &b, &c}) {
            require(close(glm::length(vertex->normal), 1), "Normal must have unit length");
            require(close(glm::dot(vertex->normal, b.position - a.position), 0),
                    "Normal must be perpendicular to first edge");
            require(close(glm::dot(vertex->normal, c.position - a.position), 0),
                    "Normal must be perpendicular to second edge");
            require(glm::dot(vertex->normal, cross) > 0, "Normal and winding disagree");
        }
        const glm::vec3 centroid = (a.position + b.position + c.position) / 3.0f;
        require(glm::dot(a.normal, centroid - glm::vec3(0, -0.5f, 0)) > 0,
                "Normal must point away from the pyramid interior");
        volume += glm::dot(a.position, glm::cross(b.position, c.position)) / 6.0f;
    }
    require(close(volume, 8.0f / 3.0f), "Winding must enclose the expected positive volume");
}

void checkCamera() {
    lab2::Camera camera;
    const glm::vec4 eye = camera.view() * glm::vec4(camera.position, 1);
    require(glm::length(glm::vec3(eye)) < 0.0001f, "View must place camera at origin");
    require(glm::length(camera.forward() - glm::normalize(-camera.position)) < 0.0001f,
            "Default camera must look at pyramid");
    for (float pitch : {-90.0f, -35.0f, 0.0f, 62.0f, 90.0f}) {
        for (float roll : {0.0f, 45.0f, 90.0f}) {
            camera.pitch = pitch;
            camera.yaw = 71;
            camera.roll = roll;
            const auto f = camera.forward();
            const auto r = camera.right();
            const auto u = camera.up();
            require(close(glm::length(f), 1) && close(glm::length(r), 1) &&
                    close(glm::length(u), 1), "Camera basis must remain normalized");
            require(close(glm::dot(f, r), 0) && close(glm::dot(f, u), 0) &&
                    close(glm::dot(r, u), 0), "Camera basis must remain orthogonal");
            const glm::vec4 ahead = camera.view() * glm::vec4(camera.position + f, 1);
            require(glm::length(glm::vec3(ahead) - glm::vec3(0, 0, -1)) < 0.0001f,
                    "Camera forward must map to negative view Z");
        }
    }
    camera = {};
    const glm::vec3 originalRight = camera.right();
    camera.roll = 90;
    require(glm::length(camera.up() - originalRight) < 0.0001f,
            "Roll must rotate camera up around the forward axis");
}

void checkProjection() {
    lab2::Camera camera;
    for (bool perspective : {false, true}) {
        camera.perspective = perspective;
        const glm::mat4 projection = camera.projection(2);
        const auto nearPoint = projection * glm::vec4(0, 0, -camera.nearPlane, 1);
        const auto farPoint = projection * glm::vec4(0, 0, -camera.farPlane, 1);
        require(close(nearPoint.z / nearPoint.w, 0), "Vulkan near depth must be zero");
        require(close(farPoint.z / farPoint.w, 1), "Vulkan far depth must be one");
        require(projection[1][1] < 0, "Projection must account for Vulkan viewport Y");
    }
    camera.perspective = false;
    const glm::vec4 corner = camera.projection(2) * glm::vec4(6, 3, -camera.nearPlane, 1);
    require(close(corner.x, 1) && close(corner.y, -1), "Orthographic bounds must respect aspect");
    camera.nearPlane = -1;
    camera.farPlane = -2;
    camera.fov = 0;
    camera.orthoHalfHeight = 0;
    for (bool perspective : {false, true}) {
        camera.perspective = perspective;
        const glm::mat4 projection = camera.projection(0);
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                require(std::isfinite(projection[column][row]), "Projection guards must avoid NaN");
    }
}

}

int main() {
    try {
        checkGeometry();
        checkCamera();
        checkProjection();
        std::cout << "Lab 2 geometry and camera tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
