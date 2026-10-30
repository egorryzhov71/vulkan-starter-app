#pragma once

#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lab2 {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct Geometry {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

inline Geometry makePyramid() {
    const std::array<glm::vec3, 5> points{{
        {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}, {0, 1, 0}
    }};
    const std::array<uint32_t, 18> triangles{{
        0, 1, 2, 0, 2, 3,
        0, 4, 1, 1, 4, 2, 2, 4, 3, 3, 4, 0
    }};
    Geometry result;
    result.vertices.reserve(triangles.size());
    result.indices.reserve(triangles.size());
    for (size_t i = 0; i < triangles.size(); i += 3) {
        const glm::vec3 a = points[triangles[i]];
        const glm::vec3 b = points[triangles[i + 1]];
        const glm::vec3 c = points[triangles[i + 2]];
        const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        for (size_t corner = 0; corner < 3; ++corner) {
            result.indices.push_back(static_cast<uint32_t>(result.vertices.size()));
            result.vertices.push_back({points[triangles[i + corner]], normal});
        }
    }
    return result;
}

struct Camera {
    glm::vec3 position{4, 3, 5};
    float yaw = -128.6598f;
    float pitch = -25.1041f;
    float roll = 0;
    float fov = 45;
    float nearPlane = 0.1f;
    float farPlane = 100;
    float orthoHalfHeight = 3;
    bool perspective = true;

    glm::vec3 forward() const {
        const float y = glm::radians(yaw);
        const float p = glm::radians(std::clamp(pitch, -89.9f, 89.9f));
        return glm::normalize(glm::vec3(std::cos(y) * std::cos(p),
                                        std::sin(p), std::sin(y) * std::cos(p)));
    }

    glm::vec3 up() const {
        const glm::vec3 f = forward();
        const glm::vec3 baseRight = glm::normalize(glm::cross(f, glm::vec3(0, 1, 0)));
        const glm::vec3 baseUp = glm::cross(baseRight, f);
        const float r = glm::radians(roll);
        return baseUp * std::cos(r) + baseRight * std::sin(r);
    }

    glm::vec3 right() const {
        return glm::normalize(glm::cross(forward(), up()));
    }

    glm::mat4 view() const {
        return glm::lookAtRH(position, position + forward(), up());
    }

    glm::mat4 projection(float aspect) const {
        aspect = aspect > 0 ? aspect : 1;
        const float nearDistance = std::max(nearPlane, 0.001f);
        const float farDistance = std::max(farPlane, nearDistance + 0.001f);
        const float halfHeight = std::max(orthoHalfHeight, 0.001f);
        glm::mat4 result = perspective
            ? glm::perspectiveRH_ZO(glm::radians(std::clamp(fov, 1.0f, 179.0f)),
                                    aspect, nearDistance, farDistance)
            : glm::orthoRH_ZO(-halfHeight * aspect, halfHeight * aspect,
                              -halfHeight, halfHeight, nearDistance, farDistance);
        result[1][1] *= -1;
        return result;
    }
};

}
