#version 450

// Прямоугольник без вершинного буфера: геометрия строится из push-константы.
// Min/Max — в ПИКСЕЛЯХ (origin верхний левый), Screen — размер кадра в пикселях.
layout(push_constant) uniform Push {
    vec2 Min;
    vec2 Max;
    vec4 Color;
    vec2 Screen;
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    // 6 вершин = 2 треугольника на квад
    vec2 corners[6] = vec2[](
        vec2(pc.Min.x, pc.Min.y),
        vec2(pc.Max.x, pc.Min.y),
        vec2(pc.Max.x, pc.Max.y),
        vec2(pc.Min.x, pc.Min.y),
        vec2(pc.Max.x, pc.Max.y),
        vec2(pc.Min.x, pc.Max.y)
    );

    vec2 px = corners[gl_VertexIndex];
    // пиксели -> NDC. В Vulkan y направлен ВНИЗ: px(0,0) -> (-1,-1) = верх-лево.
    vec2 ndc = (px / pc.Screen) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    fragColor = pc.Color;
}
