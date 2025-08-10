#version 460

#include "fragment_shared.glsl"

uniform sampler2D uEquirectangularMap;

out vec4 vFragColour;

void main()
{
    vec3 pos = normalize(fs_in.vWorldPos.xyz);
    vec2 uv = vec2(atan(pos.z, pos.x), asin(pos.y));
    uv *= vec2(0.1591, 0.3183);
    uv += 0.5;

    vec3 colour = texture(uEquirectangularMap, uv).rgb;
  
    vFragColour = vec4(colour, 1.0);
}