#version 460 // forward_pass.frag

//#extension GL_ARB_bindless_texture : enable

#include "../fragment_common.glsl"
#include "../pbr.glsl"

#line 10

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

        mat3 tbnMatrix = getTBN(fs_in.worldPos, fs_in.normal, fs_in.texCoords);

        if (any(isnan(tbnMatrix[0])) || any(isinf(tbnMatrix[0])) || 
        any(isnan(tbnMatrix[1])) || any(isinf(tbnMatrix[1])) || 
        any(isnan(tbnMatrix[2])) || any(isinf(tbnMatrix[2])))
        {
            vFragColour = vec4(0, 0, 1, 1);
            return;
        }

		normalVector = normalize(tbnMatrix * scaledNormal);
	}

    if (any(isnan(normalVector)) || any(isinf(normalVector))) 
    {
        vFragColour = vec4(0, 1, 1, 1);
        return;
    }

    const vec3 viewVector = normalize(uCameraPosition - fs_in.worldPos);

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < uNumPointLights; i++)
    {
        float shadow = 0;
        if (uShadowsEnabled)
        {
            shadow = calculatePointShadow(
                fs_in.worldPos,
                bPointLights[i].position.xyz,
                uCameraPosition,
                i
            );
        }

        Lo += calculateLightContribution(
            baseColour.rgb,
            roughness,
            metalMask,
            normalVector,
            viewVector,
            fs_in.worldPos,
            normalize(bPointLights[i].position.xyz - fs_in.worldPos),
            attenuatePointLight(bPointLights[i].position.xyz, bPointLights[i].radiance.rgb, fs_in.worldPos)
        ) * (1 - shadow);
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

    // if (uOcclusionMap.isTextureEnabled)
	// {
	// 	Lo = mix(Lo, Lo * texture(uOcclusionMap.textureMap, fs_in.texCoords).r, uOcclusionMap.factor.r);
	// }

    if (uEnvironmentMapEnabled)
    {
        Lo += calculateAmbientLightContribution(
            baseColour.rgb,
            roughness,
            metalMask,
            normalVector,
            viewVector
        );
    }

	vFragColour = vec4(Lo, baseColour.a);

    if (any(isnan(vFragColour)) || any(isinf(vFragColour))) 
    {
        vFragColour = vec4(1, 0, 1, 1); 
    }
}