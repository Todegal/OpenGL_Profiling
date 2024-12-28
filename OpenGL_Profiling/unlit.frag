#version 450
	   
in vec2 vTexCoords;

struct MaterialInput 
{
	bool isTextureEnabled;
	sampler2D textureMap;
	vec4 factor;
};

uniform MaterialInput uBaseColour;

out vec4 vFragColour;

void main()
{
	vFragColour = uBaseColour.factor;
	if (uBaseColour.isTextureEnabled)
	{
		vFragColour *= texture(uBaseColour.textureMap, vTexCoords);
	}
}