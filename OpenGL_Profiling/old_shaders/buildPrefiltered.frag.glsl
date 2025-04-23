#version 460

#include "pbr_functions.glsl"

in GS_OUT
{
    vec2 vTexCoords;
    vec3 vWorldPos;
    vec3 vNormal;
    vec3 vViewPos;
} fs_in;

uniform samplerCube uEnvironmentMap;

uniform float uRoughness;
uniform int uEnvironmentMapDimensions;

out vec4 vFragColour;

void main()
{ 
    const vec3 N = normalize(vec3(fs_in.vWorldPos));
    const vec3 R = N;
    const vec3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;   
    vec3 prefilteredColour = vec3(0.0);     
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        // generates a sample vector that's biased towards the preferred alignment direction (importance sampling).
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, uRoughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            // sample from the environment's mip level based on roughness/pdf
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            
            float D = D_GGX(NdotH, uRoughness);
            float pdf = D * NdotH / (4.0 * HdotV) + 0.0001; 

            float resolution = uEnvironmentMapDimensions; // resolution of source cubemap (per face)
            float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = uRoughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
            
            prefilteredColour += min(textureLod(uEnvironmentMap, L, mipLevel).rgb, vec3(1.0)) * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColour = prefilteredColour / totalWeight;

    vFragColour = vec4(prefilteredColour, 1.0);
}