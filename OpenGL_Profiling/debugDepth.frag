#version 460

uniform sampler2D uDepthPass;

in vec2 vTexCoords;

out vec4 vFragColour;

void main()
{
	vFragColour = vec4(texture(uDepthPass, vTexCoords).rgb, 1.0);
}