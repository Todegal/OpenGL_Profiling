#version 460 

#include "vertex_shared.glsl"

void main()
{
	vTexCoords = aTexCoords;
	gl_Position = vec4(aPos, 1.0);
}