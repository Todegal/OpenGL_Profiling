#include "pbr_functions.glsl"
#include "uniforms_common.glsl"

#line 1002

uniform samplerCube uIrradianceMap;
uniform samplerCube uPrefilteredMap;
uniform sampler2D uBRDF;

struct MaterialInput
{
    vec4 factor;
    sampler2D textureMap;
    bool isTextureEnabled;
};

uniform MaterialInput uBaseColour;
uniform MaterialInput uMetallicRoughness;
uniform MaterialInput uNormalMap;
uniform MaterialInput uOcclusionMap;

vec3 calculateF0(
    vec3 baseColour,
    float metalMask
)
{
    const float reflectance = 0.5;
    // Calculate the dielectric reflectance at normal incidence
    const float dielectricReflectance = 0.16 * reflectance * reflectance;

    // Interpolate between dielectric and metallic reflectance
    return dielectricReflectance * (1.0 - metalMask) + (baseColour.rgb * metalMask); // F0 is tinted for metallics
}

vec3 calculateLightContribution(
    vec3 baseColour,
    float roughness,
    float metalMask,
    vec3 normalVector,
    vec3 viewVector,
    vec3 fragPosition,
    vec3 lightVector, // Assumed to be normalized, vector to light source
    vec3 lightAttenuatedRadiance // Assumes no attenuation, must be done separately
)
{
    const vec3 diffuseColour = (1 - metalMask) * baseColour;

    const vec3 F0 = calculateF0(baseColour, metalMask); 

    vec3 halfVector = normalize(viewVector + lightVector);

    // Dot products
    const float NdotL = clamp(dot(normalVector, lightVector), 1e-5, 1.0);
    const float NdotH = clamp(dot(normalVector, halfVector), 1e-5, 1.0);
    const float LdotH = clamp(dot(lightVector, halfVector), 1e-5, 1.0);
    const float NdotV = clamp(abs(dot(normalVector, viewVector)), 1e-5, 1.0);

    // Cook-Torrance specular BRDF
    const float D = D_GGX(NdotH, roughness);
    const vec3 F = F_Schlick(LdotH, F0);
    const float V = V_SmithGGXCorrelated(NdotV, NdotL, roughness);

    const vec3 Fr = (D * F) * V; // Specular

    const float Fd = Fd_Lambert(); // Diffuse

    // float shadow = 0.0;

    // if (uShadowsEnabled)
    // {
    //     // todo: do shadows
    // }

    return (diffuseColour * Fd + Fr) * lightAttenuatedRadiance * NdotL;// * (1.0 - shadow);
}

vec3 calculateAmbientLightContribution(
    vec3 baseColour,
    float roughness,
    float metalMask,
    vec3 normalVector,
    vec3 viewVector
)
{
    const vec3 F0 = calculateF0(baseColour, metalMask);

    const vec3 reflectionVector = reflect(-viewVector, normalVector);

    const vec3 F = F_Schlick(max(dot(normalVector, viewVector), 0.0), F0, roughness);
    const vec3 kS = F;

    const vec3 kD = (1.0 - kS) * (1.0 - metalMask);

    const vec3 irradiance = texture(uIrradianceMap, normalVector).rgb;
    const vec3 diffuse = irradiance * baseColour.rgb;

    const float MAX_REFLECTION_LOD = 5.0;
    const vec3 prefilteredColour = textureLod(uPrefilteredMap, reflectionVector, roughness * MAX_REFLECTION_LOD).rgb;
    const vec2 brdf = texture(uBRDF, vec2(max(dot(normalVector, viewVector), 0.0), roughness)).rg;
    const vec3 specular = prefilteredColour * (F * brdf.x + brdf.y);

    const vec3 ambient = (kD * diffuse + specular);

    return ambient;
}

vec3 attenuatePointLight(
    vec3 lightPosition,
    vec3 lightRadiance,
    vec3 fragPosition
)
{
    const float lightDistance = length(lightPosition - fragPosition);
    const float attenuation = 1.0 / (lightDistance * lightDistance); // Simple attenuation
    return lightRadiance * attenuation;
}

uniform samplerCubeArrayShadow uPointShadowMaps;
uniform vec2 uPointShadowViewPlanes;

const int NUM_SHADOW_SAMPLES = 16;
const float SHADOW_BIAS = 0.00005;

float calculatePointShadow(
    vec3 fragPosition,
    vec3 lightPosition,
    vec3 cameraPosition,
    int shadowMapIndex
)
{
    const float gradientNoise = InterleavedGradientNoise(gl_FragCoord.xy);

    const vec3 invLightVector = fragPosition - lightPosition; 
    const float currentDepth = length(invLightVector);

    const float viewDistance = length(cameraPosition - fragPosition);
    const float diskRadius = 0.005;

    const float z = (currentDepth - uPointShadowViewPlanes.x) / (uPointShadowViewPlanes.y - uPointShadowViewPlanes.x);

    float shadow = 0.0;

    for (int j = 0; j < NUM_SHADOW_SAMPLES; j++)
    {
        const vec2 disk = VogelDiskSample(0, NUM_SHADOW_SAMPLES, gradientNoise * 2 * PI);
        const vec3 offset = vec3(disk, 1 - gradientNoise);
        const vec3 cubemapUVW = invLightVector + (offset * diskRadius);
        
        shadow += texture(uPointShadowMaps, vec4(cubemapUVW, shadowMapIndex), z + SHADOW_BIAS);
    }

    shadow /= float(NUM_SHADOW_SAMPLES);

    return shadow;
}