#version 460

uniform sampler2D uEquirectangularMap;

in VS_OUT
{
	vec3 worldPosition;
} fs_in;

out vec4 vFragColour;

void main()
{
    const vec3 pos = normalize(fs_in.worldPosition.xyz);
    vec2 uv = vec2(atan(pos.z, pos.x), asin(pos.y));
    uv *= vec2(0.1591, 0.3183);
    uv += 0.5;
  
    vFragColour = vec4(texture(uEquirectangularMap, uv).rgb, 1.0);
}