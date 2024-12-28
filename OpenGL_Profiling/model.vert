#version 460

#include "vertex_shared.glsl"

void main()
{
    parseJoints();
    
    vTexCoords = aTexCoords;
    vWorldPos = (uModel * vec4(vWorldPos, 1.0)).xyz;
    vNormal = uNormalMatrix * vNormal;

    gl_Position = vec4(vWorldPos, 1.0);
}
