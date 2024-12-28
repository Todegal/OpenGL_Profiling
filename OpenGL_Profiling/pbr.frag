#version 460

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoords;

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

uniform samplerCubeArray uDepthMaps;
uniform float uDepth;
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

// Proprietary functions 

const float PI = 3.14159265359;
const float INV_PI = (1.0 / PI);

const vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
); 

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

float D_GGX(float NoH, float roughness)
{
	float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * INV_PI;
}

vec3 F_Schlick(float cosTheta, vec3 F0)
{
    float f = pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    return f + F0 * (1.0 - f);
} 

float F_Schlick(float cosTheta, float F0, float F90)
{
	return F0 + (F90 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 F_Schlick(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Burley(float NoV, float NoL, float LoH, float roughness) {
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(NoL, 1.0, f90);
    float viewScatter = F_Schlick(NoV, 1.0, f90);
    return lightScatter * viewScatter * INV_PI;
}

float Fd_Lambert()
{
	return INV_PI;
}

mat3 getTBN()
{
    vec3 Q1 = dFdx(vWorldPos);
    vec3 Q2 = dFdy(vWorldPos);
    vec2 st1 = dFdx(vTexCoords);
    vec2 st2 = dFdy(vTexCoords);

    vec3 N = normalize(vNormal);
    vec3 T = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
	
	return TBN;
}

void main()
{
	vec4 baseColour = uBaseColour.factor;
	if (uBaseColour.isTextureEnabled)
	{
		baseColour *= vec4(texture(uBaseColour.textureMap, vTexCoords).xyz, 1.0);
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
		attenuation /= (lightDistance * lightDistance);
		
		vec3 radiance = bLights[i].colour * bLights[i].strength * attenuation;

		// Shadow Mapping
		vec3 invLightVector = vWorldPos - bLights[i].position;
		float currentDepth = length(invLightVector);
	
		float shadow = 0.0;

		if (uShadowsEnabled) 
		{
			float bias = 0.000001;

			float viewDistance = length(uCameraPosition - vWorldPos);
			float diskRadius = 0.0015; 
			for (int j = 0; j < NUM_SHADOW_SAMPLES; j++)
			{
				float closestDepth = texture(uDepthMaps, vec4(invLightVector + sampleOffsetDirections[j] * diskRadius, i)).r * uDepth;
				if (currentDepth - bias > closestDepth)
				{
					shadow += 1.0;
				}
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

	vFragColour = vec4(ACESFilm(colour), baseColour.a);
}
