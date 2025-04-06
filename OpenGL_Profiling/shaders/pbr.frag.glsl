#version 460

#include "fragment_shared.glsl"

#include "pbr_functions.glsl"

uniform MaterialInput uBaseColour;
uniform MaterialInput uMetallicRoughness;
uniform MaterialInput uNormal;
uniform MaterialInput uOcclusion;

uniform samplerCubeArrayShadow uShadowMaps;
uniform vec2 uShadowMapViewPlanes;
uniform bool uShadowsEnabled;
const int NUM_SHADOW_SAMPLES = 20;

struct Light
{
	vec3 position;
	vec3 colour;
	float strength;
};

layout(std430, binding = 0) buffer bLightBuffer
{
	Light bLights[];
};

uniform int uNumLights;

uniform vec3 uCameraPosition;

out vec4 vFragColour;

const vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
); 

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
	vec3 viewVector = normalize(uCameraPosition - vWorldPos);
	vec3 reflectionVector = reflect(-viewVector, normalVector);

	if (uNormal.isTextureEnabled)
	{
		vec3 textureNormal = texture(uNormal.textureMap, vTexCoords).rgb;
		vec3 scaledNormal;
		scaledNormal.xy = (textureNormal.rg * 2 - 1) * uNormal.factor.x;
		scaledNormal.z = (textureNormal.b * 2 - 1);

		normalVector = normalize(getTBN() * scaledNormal);
	}

	float NoV = abs(dot(normalVector, viewVector)) + 1e-5;
	
	
	vec3 diffuseColor = (1 - metallic) * baseColour.rgb;

	float reflectance = 0.5;

//	vec3 F0 = vec3(0.04); // Assume constant F0 for dieletrics
	vec3 F0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + baseColour.rgb * metallic; // F0 is tinted for metallics

	// Reflectance
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < uNumLights; i++)
	{
		vec3 lightVector = normalize(bLights[i].position - vWorldPos);

		vec3 halfVector = normalize(viewVector + lightVector); 

		float NoL = clamp(dot(normalVector, lightVector), 0, 1);
		float NoH = clamp(dot(normalVector, halfVector), 0, 1);
		float LoH = clamp(dot(lightVector, halfVector), 0, 1);

		// Cook-Torrance specular BRDF
		float D = D_GGX(NoH, roughness);
		vec3 F = F_Schlick(LoH, F0);
		float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

		vec3 Fr = (D * V) * F; // Specular component

		float Fd = Fd_Burley(NoV, NoL, LoH, roughness);
//		float Fd = Fd_Lambert();

		float attenuation = 1.0;

		float lightDistance = length(bLights[i].position - vWorldPos);
		attenuation /= (lightDistance * lightDistance); // ? + (2 * lightDistance) + 1;
		
		vec3 radiance = bLights[i].colour * bLights[i].strength * attenuation;

		// Shadow Mapping
	
		float shadow = 0.0;

		if (uShadowsEnabled) 
		{
			vec3 invLightVector = vWorldPos - bLights[i].position;
			float currentDepth = length(invLightVector);

			float viewDistance = length(uCameraPosition - vWorldPos);
			float diskRadius = 0.0015; 
			for (int j = 0; j < NUM_SHADOW_SAMPLES; j++)
			{
				vec3 offset = sampleOffsetDirections[j % 20] * (1.0 + (float(j) / float(NUM_SHADOW_SAMPLES)));
				vec3 cubemapUVW = invLightVector + offset * diskRadius;

				float z = (currentDepth - uShadowMapViewPlanes.x) / (uShadowMapViewPlanes.y - uShadowMapViewPlanes.x);
				shadow += texture(uShadowMaps, vec4(cubemapUVW, i), z - shadowBias);
			}
			
			shadow /= float(NUM_SHADOW_SAMPLES);
		}


		Lo += (diffuseColor * Fd + Fr) * radiance * NoL * (1 - shadow);
	}

    vec3 colour = Lo;

	if (uOcclusion.isTextureEnabled)
	{
		colour = mix(colour, colour * texture(uOcclusion.textureMap, vTexCoords).r, uOcclusion.factor.r);
	}

	vFragColour = vec4(colour, baseColour.a);
}
