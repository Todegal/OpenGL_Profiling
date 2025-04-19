#version 460

#include "vertex_shared.glsl"

void main()
{
    vs_out.vWorldPos = aPos;

    gl_Position = (uProjection * mat4(mat3(uView)) * vec4(aPos, 1.0)).xyww;
}
