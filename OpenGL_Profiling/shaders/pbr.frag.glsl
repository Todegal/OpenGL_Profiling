#version 460

#include "fragment_shared.glsl"

#include "pbr_functions.glsl"

uniform MaterialInput uBaseColour;
uniform MaterialInput uMetallicRoughness;
uniform MaterialInput uNormal;
uniform MaterialInput uOcclusion;

#define NUM_CASCADES 5

uniform samplerCubeArrayShadow uPointShadowMaps;
uniform sampler2DArrayShadow uDirectionalShadowMaps;
uniform float uCascadeFarPlanes[NUM_CASCADES];
uniform vec2 uShadowMapViewPlanes;
uniform bool uShadowsEnabled;
const int NUM_SHADOW_SAMPLES = 20;

uniform bool uEnvironmentEnabled;
uniform float uEnvironmentMapFactor;
uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilteredMap;
uniform sampler2D uBRDF;

uniform bool uSunEnabled;
uniform vec3 uSunColour;
uniform vec3 uSunDirection;

struct Light
{
	vec3 position;
	float radius;
	vec3 radiance;
	int type;
	mat4 lightSpaceMatrices[NUM_CASCADES];
};

layout(std430, binding = 0) buffer bLightBuffer
{
	Light bLights[];
};

uniform int uNumLights;
uniform int uNumDirectionalLights;

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

int determineCascade()
{
	const float viewDepth = abs(fs_in.vViewPos.z);

	int layer = 0;
	for (int i = 0; i < NUM_CASCADES - 1; i++)
	{
		if (viewDepth < uCascadeFarPlanes[i])
		{
			layer = i;
			break;
		}
	}

	return layer;
}

void main()
{
	const float gradientNoise = InterleavedGradientNoise(gl_FragCoord.xy);

	vec4 baseColour = uBaseColour.factor;
	if (uBaseColour.isTextureEnabled)
	{
		baseColour *= texture(uBaseColour.textureMap, fs_in.vTexCoords);
	}

	float roughness = uMetallicRoughness.factor.g;
	float metallic = uMetallicRoughness.factor.b;
	if (uMetallicRoughness.isTextureEnabled)
	{
		vec3 mr = texture(uMetallicRoughness.textureMap, fs_in.vTexCoords).rgb;
		roughness *= mr.g;
		metallic *= mr.b;
	}

	vec3 normalVector = normalize(fs_in.vNormal);
	vec3 viewVector = normalize(uCameraPosition - fs_in.vWorldPos);
	vec3 reflectionVector = reflect(-viewVector, normalVector);

	if (uNormal.isTextureEnabled)
	{
		vec3 textureNormal = texture(uNormal.textureMap, fs_in.vTexCoords).rgb;
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

	// Calculate point Lights
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < uNumLights; i++)
	{
		vec3 lightVector = normalize(bLights[i].position - fs_in.vWorldPos);
		if (bLights[i].type == 0) lightVector = -bLights[i].position;

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

		const float lightDistance = length(bLights[i].position - fs_in.vWorldPos);
		float attenuation = 1.0;
		if (bLights[i].type != 0) attenuation = 1.0 / (lightDistance * lightDistance); // ? + (2 * lightDistance) + 1;

		const vec3 radiance = bLights[i].radiance * attenuation;

		// Shadow Mapping
	
		float shadow = 0.0;
		const float diskRadius = 0.0015; 

		const int cascadeLayer = determineCascade();

		if (uShadowsEnabled) 
		{
			if (bLights[i].type == 1) // Point Light
			{
				vec3 invLightVector = fs_in.vWorldPos - bLights[i].position;

				float currentDepth = length(invLightVector);

				float viewDistance = length(uCameraPosition - fs_in.vWorldPos);
				for (int j = 0; j < NUM_SHADOW_SAMPLES; j++)
				{
					vec3 offset = sampleOffsetDirections[j % 20] * (1.0 + (float(j) / float(NUM_SHADOW_SAMPLES)));
					vec3 cubemapUVW = invLightVector + (offset * diskRadius);

					float z = (currentDepth - uShadowMapViewPlanes.x) / (uShadowMapViewPlanes.y - uShadowMapViewPlanes.x);

					shadow += texture(uPointShadowMaps, vec4(cubemapUVW, i - uNumDirectionalLights), z - shadowBias);
				}
				
				if (currentDepth > uShadowMapViewPlanes.y)
				{
					shadow = 0.0;
				}
			}
			else // Directional Light
			{	
				vec4 fragPosLightSpace = bLights[i].lightSpaceMatrices[cascadeLayer] * vec4(fs_in.vWorldPos, 1.0);

				// perform perspective divide
				vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
				// transform to [0,1] range
				projCoords = projCoords * 0.5 + 0.5;				

				float diskRadius = 0.0015; 
				for (int j = 0; j < NUM_SHADOW_SAMPLES; j++)
				{
					const vec2 disk = VogelDiskSample(j, NUM_SHADOW_SAMPLES, gradientNoise * 2 * PI);

					float bias = max(0.05 * (1.0 - dot(normalVector, bLights[i].position)), 0.0005);
					bias /= 0.5 * uCascadeFarPlanes[cascadeLayer];

					shadow += texture(uDirectionalShadowMaps, vec4(projCoords.xy + (disk * diskRadius), (i * NUM_CASCADES) + cascadeLayer, projCoords.z - shadowBias));
				}
			}

			shadow /= float(NUM_SHADOW_SAMPLES);
		}

		Lo += (diffuseColor * Fd + Fr) * radiance * NoL * (1 - shadow);
		// Lo = vec3(float(cascadeLayer) / float(NUM_CASCADES));
	}

	// Calculate sun
	// if (uSunEnabled)
	// {
	// 	vec3 lightVector = -uSunDirection;

	// 	vec3 halfVector = normalize(viewVector + lightVector); 

	// 	float NoL = clamp(dot(normalVector, lightVector), 0, 1);
	// 	float NoH = clamp(dot(normalVector, halfVector), 0, 1);
	// 	float LoH = clamp(dot(lightVector, halfVector), 0, 1);

	// 	// Cook-Torrance specular BRDF
	// 	float D = D_GGX(NoH, roughness);
	// 	vec3 F = F_Schlick(LoH, F0);
	// 	float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

	// 	vec3 Fr = (D * V) * F; // Specular component

	// 	float Fd = Fd_Burley(NoV, NoL, LoH, roughness);

	// 	Lo += (diffuseColor * Fd + Fr) * uSunColour * NoL;
	// }

    vec3 colour = Lo;

	if (uEnvironmentEnabled)
	{
		const vec3 F = F_Schlick(max(dot(normalVector, viewVector), 0.0), F0, roughness);
		const vec3 kS = F;

		const vec3 kD = (1.0 - kS) * (1.0 - metallic);

		const vec3 irradiance = texture(uIrradianceMap, normalVector).rgb * uEnvironmentMapFactor;
		const vec3 diffuse = irradiance * baseColour.rgb;

		const float MAX_REFLECTION_LOD = 5.0;
		vec3 prefilteredColour = textureLod(uPrefilteredMap, reflectionVector, roughness * MAX_REFLECTION_LOD).rgb * uEnvironmentMapFactor;
		vec2 brdf = texture(uBRDF, vec2(max(dot(normalVector, viewVector), 0.0), roughness)).rg;
		vec3 specular = prefilteredColour * (F * brdf.x + brdf.y);

		const vec3 ambient = (kD * diffuse + specular);

		colour += ambient;
	}

	if (uOcclusion.isTextureEnabled)
	{
		colour = mix(colour, colour * texture(uOcclusion.textureMap, fs_in.vTexCoords).r, uOcclusion.factor.r);
	}

	vFragColour = vec4(colour, baseColour.a);
	// vFragColour = vec4(texture(uDirectionalShadowMaps, vec3(vTexCoords, 0)).xyz, 1.0);
	// vFragColour = vec4(vec3(finalShadow), 1.0);
}
