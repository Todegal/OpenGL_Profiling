#version 450
	   
struct MaterialInput
{
    bool isTextureEnabled;
    sampler2D textureMap;
    vec4 factor;
};

in GS_OUT
{
    vec2 vTexCoords;
    vec3 vWorldPos;
    vec3 vNormal;
    vec3 vViewPos;
} fs_in;

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