#version 460 // forward_pass.vert

#include "../uniforms_common.glsl"
#include "../vertex_common.glsl"

#line 6

out VS_OUT
{
    vec3 worldPos;
    vec3 normal;
    vec3 viewPos;
    vec2 texCoords;
} vs_out;

void main()
{
    const InstanceTransform instanceTransform = bInstanceTransforms[gl_InstanceID + uInstanceOffset];
    const SkinnedVertex vtx = applySkinning(aPosition, aNormal, aBoneIds, aBoneWeights);
    
    vs_out.worldPos = (instanceTransform.modelMatrix * vec4(vtx.position, 1.0)).xyz;
    vs_out.normal = normalize(instanceTransform.normalMatrix * vtx.normal);
    vs_out.viewPos = (uViewMatrix * vec4(vs_out.worldPos, 1.0)).xyz;
    vs_out.texCoords = aTexCoords;

    gl_Position = uProjectionMatrix * vec4(vs_out.viewPos, 1.0);
}