#version 460 

in vec4 vFragPos;

uniform vec3 uLightPos;

uniform float uShadowMapDepth;

void main()
{
    float lightDistance = abs(length(vFragPos.xyz - uLightPos));
    
    lightDistance = lightDistance / uShadowMapDepth;
    
    gl_FragDepth = lightDistance;
}