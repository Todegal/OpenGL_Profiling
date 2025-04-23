struct MaterialInput
{
    vec4 factor;
    bool isTextureEnabled;
    sampler2D textureMap;
};

uniform MaterialInput uBaseColour;
uniform MaterialInput uMetallicRoughness;
uniform MaterialInput uNormalMap;
uniform MaterialInput uOcclusionMap;

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

	const float reflectance = 0.5;
    // Calculate the dielectric reflectance at normal incidence
    const float dielectricReflectance = 0.16 * reflectance * reflectance;

    // Interpolate between dielectric and metallic reflectance
    const vec3 F0 = dielectricReflectance * (1.0 - metalMask) + baseColour.rgb * metalMask; // F0 is tinted for metallics

    const vec3 halfVector = normalize(lightVector + viewVector);

    // Dot products
    const float NdotL = max(dot(normalVector, lightVector), 0.0);
    const float NdotH = max(dot(normalVector, halfVector), 0.0);
    const float LdotH = max(dot(lightVector, halfVector), 0.0);
    const float NdotV = abs(dot(normalVector, viewVector)) + 1e-5;

    // Cook-Torrance specular BRDF
    const float D = D_GGX(NdotH, roughness);
    const vec3 F = F_Schlick(LdotH, F0);
    const float V = V_SmithGGXCorrelated(NdotV, NdotL, roughness);

    const vec3 Fr = (D * F) * V; // Specular

    const float Fd = Fd_Burley(NdotV, NdotL, LdotH, roughness); // Diffuse

    float shadow = 0.0;

    if (uShadowsEnabled)
    {
        // todo: do shadows
    }

    return (diffuseColour * Fd + Fr) * lightAttenuatedRadiance * NdotL * (1.0 - shadow);
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