#version 450

#include "../uniforms_common.glsl"
#include "../fragment_common.glsl"
#include "../pbr_functions.glsl"
#include "../pbr.glsl"

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Converts a color from linear light gamma to sRGB gamma
vec3 fromLinear(vec3 linearRGB)
{
    bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
    vec3 higher = vec3(1.055) * pow(linearRGB.rgb, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower = linearRGB.rgb * vec3(12.92);

    return mix(higher, lower, cutoff);
}

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

	if (baseColour.a < 0.5) { discard; }

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
				baseColour.rgb,
				roughness,
				metalMask,
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

	vec3 colour = ACESFilm(Lo);
	colour = fromLinear(Lo);

	vFragColour = vec4(colour, baseColour.a);
}
