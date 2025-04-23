#version 460

#include "pbr_functions.glsl"

#include "fragment_shared.glsl"

out vec2 vFragColour;

vec2 IntegrateBRDF(float NdotV, float roughness)
{
    const vec3 V = vec3(sqrt(1.0 - NdotV*NdotV), 0.0, NdotV);

    float A = 0.0;
    float B = 0.0;

    const vec3 N = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        const vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        const vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        const vec3 L = normalize(2.0 * dot(V, H) * H - V);

        const float NdotL = max(L.z, 0.0);
        const float NdotH = max(H.z, 0.0);
        const float VdotH = max(dot(V, H), 0.0);

        if(NdotL > 0.0)
        {
            float G = G_Smith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return vec2(A, B);
}

void main() 
{
    vec2 integratedBRDF = IntegrateBRDF(fs_in.vTexCoords.x, fs_in.vTexCoords.y);
    vFragColour = integratedBRDF;
}