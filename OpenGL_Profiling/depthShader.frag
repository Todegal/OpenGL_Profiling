#version 460 

in vec4 vFragPos;

uniform vec3 uLightPos;

uniform float uDepth;

void main()
{
    float lightDistance = abs(length(vFragPos.xyz - uLightPos));
    
    lightDistance = lightDistance / uDepth;
    
    gl_FragDepth = lightDistance;
}