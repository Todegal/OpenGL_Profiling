#version 460

#include "vertex_shared.glsl"

void main()
{
    parseJoints();

    vs_out.vTexCoords = aTexCoords;
    vs_out.vWorldPos = (uModel * vec4(vs_out.vWorldPos, 1.0)).xyz;
    vs_out.vNormal = uNormalMatrix * vs_out.vNormal;
    vs_out.vViewPos = (uView * vec4(vs_out.vWorldPos, 1.0)).xyz;

    gl_Position = uProjection * uView * vec4(vs_out.vWorldPos, 1.0);
}
