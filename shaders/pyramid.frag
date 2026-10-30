#version 450

layout(location = 0) in vec3 worldPosition;
layout(location = 1) in vec3 worldNormal;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0, std140) uniform SceneUniforms {
    mat4 model;
    mat4 viewProjection;
    mat4 normalMatrix;
    vec4 cameraPosition;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    vec4 ambientColorIntensity;
    vec4 albedo;
    vec4 specularShininess;
} scene;

vec3 safeNormalize(vec3 value, vec3 fallback)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 1e-12 ? value * inversesqrt(lengthSquared) : fallback;
}

void main()
{
    vec3 N = safeNormalize(worldNormal, vec3(0.0, 1.0, 0.0));
    vec3 L = safeNormalize(-scene.lightDirection.xyz, vec3(0.0, 1.0, 0.0));
    vec3 V = safeNormalize(scene.cameraPosition.xyz - worldPosition, N);

    float diffuseFactor = max(dot(N, L), 0.0);
    vec3 diffuse = scene.albedo.rgb * diffuseFactor;
    vec3 specular = vec3(0.0);
    vec3 halfVector = L + V;
    float halfLengthSquared = dot(halfVector, halfVector);
    if (diffuseFactor > 0.0 && halfLengthSquared > 1e-12) {
        vec3 H = halfVector * inversesqrt(halfLengthSquared);
        float highlight = pow(max(dot(N, H), 0.0), max(scene.specularShininess.a, 1.0));
        specular = scene.specularShininess.rgb * highlight;
    }

    vec3 ambient = scene.ambientColorIntensity.rgb * scene.ambientColorIntensity.a * scene.albedo.rgb;
    vec3 directionalLight = scene.lightColorIntensity.rgb * scene.lightColorIntensity.a;
    outColor = vec4(ambient + (diffuse + specular) * directionalLight, 1.0);
}
