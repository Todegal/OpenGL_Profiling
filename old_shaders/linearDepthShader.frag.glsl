#version 460 

in GS_OUT
{
    vec2 vTexCoords;
    vec3 vWorldPos;
    vec3 vNormal;
    vec3 vViewPos;
} fs_in;

uniform vec3 uLightPos;

uniform vec2 uShadowMapViewPlanes;

void main()
{
    float lightDistance = abs(length(fs_in.vWorldPos.xyz - uLightPos));

    lightDistance -= uShadowMapViewPlanes.x;
    lightDistance /= (uShadowMapViewPlanes.y - uShadowMapViewPlanes.x);    
    
    gl_FragDepth = lightDistance;
    // gl_FragDepth = gl_FragCoord.z;
}