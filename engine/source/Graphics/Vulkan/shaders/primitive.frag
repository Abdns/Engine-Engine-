#version 450

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(set = 3, binding = 0) uniform Object {
    vec4 Tint;
} obj;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragUV) * obj.Tint;
}
