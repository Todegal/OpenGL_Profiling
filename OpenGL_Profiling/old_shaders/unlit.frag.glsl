#version 450
	   
#include "fragment_shared.glsl"

uniform MaterialInput uBaseColour;

out vec4 vFragColour;

void main()
{
	vFragColour = uBaseColour.factor;
	if (uBaseColour.isTextureEnabled)
	{
		vFragColour *= texture(uBaseColour.textureMap, fs_in.vTexCoords);
	}
}