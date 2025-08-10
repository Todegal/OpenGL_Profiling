#version 460

#include "../uniforms_common.glsl"
#include "../fragment_common.glsl"
#include "../pbr_functions.glsl"
#include "../pbr.glsl"

in VS_OUT
{
    vec3 worldPos;
    vec3 normal;
    vec3 viewPos;
    vec2 texCoords;
} fs_in;

out vec4 vFragColour;

void main()
{
    vec4 baseColour = uBaseColour.factor;
	if (uBaseColour.isTextureEnabled)
	{
		baseColour *= texture(uBaseColour.textureMap, fs_in.texCoords);
	}

	float roughness = uMetallicRoughness.factor.g;
	float metalMask = uMetallicRoughness.factor.b;
	if (uMetallicRoughness.isTextureEnabled)
	{
		vec3 mr = texture(uMetallicRoughness.textureMap, fs_in.texCoords).rgb;
		roughness *= mr.g;
		metalMask *= mr.b;
	}

	vec3 normalVector = normalize(fs_in.normal);

	if (uNormalMap.isTextureEnabled)
	{
		vec3 textureNormal = texture(uNormalMap.textureMap, fs_in.texCoords).rgb;
		vec3 scaledNormal;
		scaledNormal.xy = (textureNormal.rg * 2 - 1) * uNormalMap.factor.x;
		scaledNormal.z = (textureNormal.b * 2 - 1);

		normalVector = normalize(getTBN(fs_in.worldPos, fs_in.normal, fs_in.texCoords) * scaledNormal);
	}

    const vec3 viewVector = normalize(uCameraPosition - fs_in.worldPos);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < uNumPointLights; i++)
    {
        Lo += calculateLightContribution(
            vec3(1.0, 1.0, 1.0),
            1.0,
            0.0,
            normalVector,
            viewVector,
            fs_in.worldPos,
            normalize(bPointLights[i].position.xyz - fs_in.worldPos),
            attenuatePointLight(bPointLights[i].position.xyz, bPointLights[i].radiance.rgb, fs_in.worldPos)
        );
    }

    for (int i = 0; i < uNumDirectionalLights; i++)
    {
        Lo += calculateLightContribution(
            baseColour.rgb,
            roughness,
            metalMask,
            normalVector,
            viewVector,
            fs_in.worldPos,
            bDirectionalLights[i].direction.xyz,
            bDirectionalLights[i].radiance.rgb
        );
    }

    if (uOcclusionMap.isTextureEnabled)
	{
		Lo = mix(Lo, Lo * texture(uOcclusionMap.textureMap, fs_in.texCoords).r, uOcclusionMap.factor.r);
	}

	vFragColour = vec4(Lo, baseColour.a);
}