#version 460

#include "fragment_shared.glsl"

uniform samplerCube uEnvironmentMap;
uniform float uEnvironmentMapFactor;

out vec4 vFragColour;

void main()
{
    vFragColour = textureLod(uEnvironmentMap, fs_in.vWorldPos, 0) * uEnvironmentMapFactor;
}