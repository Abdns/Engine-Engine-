#version 450

layout(set = 1, binding = 0) uniform Camera {
    mat4 ViewProj;
} cam;

layout(push_constant) uniform Push {
    mat4 Model;
} pc;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
    gl_Position = cam.ViewProj * pc.Model * vec4(inPos, 1.0);
    fragColor = inColor;
    fragUV = inUV;
}
