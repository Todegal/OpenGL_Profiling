#version 460

uniform samplerCube uEnvironmentMap;

in vec3 vWorldPos;

out vec4 vFragColour;

void main()
{
    vFragColour = texture(uEnvironmentMap, normalize(vWorldPos));
}