#version 460

#include "fragment_shared.glsl"

// G-Buffer textures
layout (location = 0) out vec4 g0; // Position.x	| Position.y	| Position.z	| MetallicMask
layout (location = 1) out vec4 g1; // Normal.x		| Normal.y		| Normal.z		| Roughness
layout (location = 2) out vec4 g2; // BaseColour.r	| BaseColour.g	| BaseColour.b	| AmbientOcllusion 

struct MaterialInput 
{
	bool isTextureEnabled;
	sampler2D textureMap;
	vec4 factor;
};

uniform MaterialInput uBaseColour;
uniform MaterialInput uMetallicRoughness;
uniform MaterialInput uNormal;
uniform MaterialInput uOcclusion;

void main()
{
	vec4 baseColour = uBaseColour.factor;
	if (uBaseColour.isTextureEnabled)
	{
		baseColour *= texture(uBaseColour.textureMap, vTexCoords);
	}

	float roughness = uMetallicRoughness.factor.g;
	float metallic = uMetallicRoughness.factor.b;
	if (uMetallicRoughness.isTextureEnabled)
	{
		vec3 mr = texture(uMetallicRoughness.textureMap, vTexCoords).rgb;
		roughness *= mr.g;
		metallic *= mr.b;
	}

	vec3 normalVector = normalize(vNormal);

	if (uNormal.isTextureEnabled)
	{
		vec3 scaledNormal = texture(uNormal.textureMap, vTexCoords).rgb;
		scaledNormal.xy = (scaledNormal.xy * 2 - 1) * uNormal.factor.x;
		scaledNormal.z = (scaledNormal.z * 2 - 1);

		normalVector = normalize(getTBN() * scaledNormal);
	}

	// colour 	= mix(colour, colour * texture(uOcclusion.textureMap, vTexCoords).r, uOcclusion.factor.r);
	// 		 	= colour * ( 1 - factor ) + ( colour * occlusion_map ) * factor
	// 			= K * (1 - f) + K * (M * f)
	//			= K * ((1 - f) + (M * f))

	float occlusion = 1.0;
	if (uOcclusion.isTextureEnabled)
	{
		occlusion = (1 - uOcclusion.factor.r) + (texture(uOcclusion.textureMap, vTexCoords).r * uOcclusion.factor.r);
	}
	
	occlusion = clamp(occlusion, 0, 1);

	// Pack the G-Buffer
	g0 = vec4(vWorldPos.xyz, metallic);
	g1 = vec4(normalVector.xyz, roughness);
	g2 = vec4(baseColour.xyz, occlusion);
}