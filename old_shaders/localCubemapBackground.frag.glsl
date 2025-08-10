#version 460

in GS_OUT
{
    vec2 vTexCoords;
    vec3 vWorldPos;
    vec3 vNormal;
    vec3 vViewPos;
} fs_in;

uniform samplerCube uEnvironmentMap;
uniform float uEnvironmentMapFactor;

out vec4 vFragColour;

void main()
{
    vFragColour = textureLod(uEnvironmentMap, fs_in.vWorldPos, 0) * uEnvironmentMapFactor;
}