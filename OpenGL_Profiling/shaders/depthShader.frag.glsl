#version 460 

in vec4 vFragPos;

uniform vec3 uLightPos;

uniform vec2 uShadowMapViewPlanes;

void main()
{
    float lightDistance = abs(length(vFragPos.xyz - uLightPos));

    lightDistance -= uShadowMapViewPlanes.x;
    lightDistance /= (uShadowMapViewPlanes.y - uShadowMapViewPlanes.x);    
    
    gl_FragDepth = lightDistance;
}