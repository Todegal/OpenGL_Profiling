#version 460 

in GS_OUT
{
    vec3 worldPosition;
} fs_in;

uniform vec3 uLightPos;

uniform vec2 uPointShadowViewPlanes;

void main()
{
    float lightDistance = length(fs_in.worldPosition.xyz - uLightPos);

    lightDistance -= uPointShadowViewPlanes.x;
    lightDistance /= (uPointShadowViewPlanes.y - uPointShadowViewPlanes.x);    
    
    gl_FragDepth = lightDistance;
}