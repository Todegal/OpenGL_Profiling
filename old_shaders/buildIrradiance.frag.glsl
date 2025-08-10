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

out vec4 vFragColour;

void main()
{
    const vec3 normal = normalize(fs_in.vWorldPos.xyz);

    vec3 irradiance = vec3(0.0);

    const vec3 up = vec3(0.0, 1.0, 0.0);
    const vec3 right = normalize(cross(normal, normalize(cross(up, normal))));

    const float sampleDelta = 0.025;

    int numSamples = 0; 
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        const float sinPhi = sin(phi);
        const float cosPhi = cos(phi);

        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            const float sinTheta = sin(theta);
            const float cosTheta = cos(theta);

            // spherical to cartesian (in tangent space)
            const vec3 tangentSample = vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
            // tangent space to world
            const vec3 sampleVec = 
            tangentSample.x * right + 
            tangentSample.y * up + 
            tangentSample.z * normal; 

            irradiance += min(texture(uEnvironmentMap, sampleVec).rgb, vec3(1.0)) * cosTheta * sinTheta;
            numSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(numSamples));
  
    vFragColour = vec4(irradiance, 1.0);
}