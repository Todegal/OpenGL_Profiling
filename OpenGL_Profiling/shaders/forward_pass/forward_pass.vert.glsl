#version 460

#include "../vertex_common.glsl"
#include "../uniforms_common.glsl"

out VS_OUT
{
    vec3 worldPos;
    vec3 normal;
    vec3 viewPos;
    vec2 texCoords;
} vs_out;

void main()
{
    SkinnedVertex vtx = applySkinning(aPosition, aNormal, aBoneIds, aBoneWeights);

    vs_out.worldPos = (uModelMatrix * vec4(vtx.position, 1.0)).xyz;
    vs_out.normal = uNormalMatrix * vtx.normal;
    vs_out.viewPos = (uViewMatrix * vec4(vs_out.worldPos, 1.0)).xyz;
    vs_out.texCoords = aTexCoords;

    gl_Position = uProjectionMatrix * vec4(vs_out.viewPos, 1.0);
}