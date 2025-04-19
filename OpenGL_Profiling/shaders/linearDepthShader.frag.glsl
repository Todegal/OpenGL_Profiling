#version 460 

#include "fragment_shared.glsl"

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