#version 460

#include "../uniforms_common.glsl"

out vec4 vFragColour;

in VS_OUT
{
	flat uint instanceID;	
} fs_in;

void main()
{
	const vec3 lightColour = normalize(bPointLights[fs_in.instanceID].radiance.rgb);
	vFragColour = vec4(lightColour, 1.0);
    // vFragColour = vec4(1, 0, 0, 1);
}