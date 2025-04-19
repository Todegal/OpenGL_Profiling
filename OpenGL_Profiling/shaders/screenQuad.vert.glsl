#version 460 

#include "vertex_shared.glsl"

void main()
{
	vs_out.vTexCoords = aTexCoords;
	vs_out.vWorldPos = aPos;
	gl_Position = vec4(aPos, 1.0);
}