#version 460

uniform samplerCube uEnvironmentCubemap;

in VS_OUT
{
	vec3 position;
} fs_in;

out vec4 vFragColour;

void main()
{
    const vec3 normalizedPosition = normalize(fs_in.position);
  
    vFragColour = textureLod(uEnvironmentCubemap, normalizedPosition, 0);
}