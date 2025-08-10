#version 460

#include "../vertex_common.glsl"

void main()
{
    gl_Position = vec4(aPosition, 1.0);
}