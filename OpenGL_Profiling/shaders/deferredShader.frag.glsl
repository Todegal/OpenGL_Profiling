#version 460

#include "fragment_shared.glsl"

#include "pbr_functions.glsl"

layout(origin_upper_left) in vec4 gl_FragCoord;

uniform sampler2D g0; // Position.x		| Position.y	| Position.z	| MetallicMask
uniform sampler2D g1; // Normal.x		| Normal.y		| Normal.z		| Roughness
uniform sampler2D g2; // BaseColour.r	| BaseColour.g	| BaseColour.b	| AmbientOcllusion 

uniform samplerCubeArray uShadowMaps;
uniform float uShadowMapDepth;
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
uniform vec2 uScreenDimensions;

out vec4 vFragColour;

const vec3 sampleOffsetDirections[20] = vec3[]
(
	vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
	vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
	vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
	vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
	vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1)
);

void main()
{
	// Decode g-buffer
	
	vec2 uvs = (gl_FragCoord.xy / uScreenDimensions);
	uvs.y = 1 - uvs.y;

	vec4 buffer0 = texture(g0, uvs).xyzw;
	vec4 buffer1 = texture(g1, uvs).xyzw;
	vec4 buffer2 = texture(g2, uvs).xyzw;

	vec3 worldPos = buffer0.xyz;
	vec3 normalVector = buffer1.xyz;
	vec3 baseColour = buffer2.xyz;

	float metallic = buffer0.w;
	float roughness = buffer1.w;
	float occlusion = buffer2.w;
	
	//

	vec3 viewVector = normalize(uCameraPosition - worldPos);
	vec3 reflectionVector = reflect(-viewVector, normalVector);

	float NoV = abs(dot(normalVector, viewVector)) + 1e-5;

	vec3 diffuseColor = (1 - metallic) * baseColour.rgb;

	float reflectance = 0.5;

	//	vec3 F0 = vec3(0.04); // Assume constant F0 for dieletrics
	vec3 F0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + baseColour.rgb * metallic; // F0 is tinted for metallics

	// Reflectance
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < uNumLights; i++)
	{
		vec3 lightVector = normalize(bLights[i].position - worldPos);

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

		float lightDistance = length(bLights[i].position - worldPos);
		attenuation /= (lightDistance * lightDistance); // ? + (2 * lightDistance) + 1;

		vec3 radiance = bLights[i].colour * bLights[i].strength * attenuation;

		// Shadow Mapping

		float shadow = 0.0;

		if (uShadowsEnabled)
		{
			vec3 invLightVector = worldPos - bLights[i].position;
			float currentDepth = length(invLightVector);

			float bias = 0.01;

			float viewDistance = length(uCameraPosition - worldPos);
			float diskRadius = 0.0015;
			for (int j = 0; j < NUM_SHADOW_SAMPLES; j++)
			{
				float closestDepth = texture(uShadowMaps, vec4(invLightVector + sampleOffsetDirections[j] * diskRadius, i)).r * uShadowMapDepth;
				if (currentDepth - bias > closestDepth)
				{
					shadow += 1.0;
				}
			}

			shadow /= float(NUM_SHADOW_SAMPLES);
		}


		Lo += (diffuseColor * Fd + Fr) * radiance * NoL * (1 - shadow);
	}

	vec3 colour = Lo * (1 - occlusion);

	vFragColour = vec4(colour.rgb, 1.0);
}