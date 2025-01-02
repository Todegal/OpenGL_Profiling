#version 460

#include "vertex_shared.glsl"

// only for instancing
layout(binding = 2) buffer bModelBuffer
{
    mat4 bModel[];
};

out int vInstanceIdx;

void main()
{
    parseJoints();

    vTexCoords = aTexCoords;
    vWorldPos = (bModel[gl_InstanceID] * vec4(vWorldPos, 1.0)).xyz;
    vNormal = uNormalMatrix * vNormal;

    vInstanceIdx = gl_InstanceID;

    gl_Position = uProjection * uView * vec4(vWorldPos, 1.0);
}
