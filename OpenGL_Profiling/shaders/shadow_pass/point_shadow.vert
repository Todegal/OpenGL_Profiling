#version 460

#include "../uniforms_common.glsl"
#include "../vertex_common.glsl"

out VS_OUT
{
	vec3 worldPosition;
} vs_out;

void main()
{
	const InstanceTransform instanceTransform = bInstanceTransforms[gl_InstanceID + uInstanceOffset];
    const SkinnedVertex vtx = applySkinning(aPosition, aNormal, aBoneIds, aBoneWeights);
    
    vs_out.worldPosition = (instanceTransform.modelMatrix * vec4(vtx.position, 1.0)).xyz;

    gl_Position = vec4(vs_out.worldPosition, 1.0);
}