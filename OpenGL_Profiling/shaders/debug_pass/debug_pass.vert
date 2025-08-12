#version 460

#include "../uniforms_common.glsl"
#include "../vertex_common.glsl"

out VS_OUT
{
    flat uint instanceID;
} vs_out;

void main()
{
    const InstanceTransform instanceTransform = bInstanceTransforms[gl_InstanceID];
    vs_out.instanceID = gl_InstanceID;

    gl_Position = uProjectionMatrix * uViewMatrix * instanceTransform.modelMatrix * vec4(aPosition, 1.0);
}