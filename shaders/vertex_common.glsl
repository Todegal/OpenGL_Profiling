layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec4 aBoneIds;
layout(location = 4) in vec4 aBoneWeights;

layout(std430) buffer JointsBuffer
{
    mat4 bJointMatrices[];
};

layout (std140) uniform ObjectBuffer
{
    mat4 uModelMatrix;
    mat3 uNormalMatrix;
};

struct SkinnedVertex
{
    vec3 position;
    vec3 normal;
};

SkinnedVertex applySkinning(vec3 position, vec3 normal, vec4 boneIds, vec4 boneWeights)
{
    vec3 transformedPosition = vec3(0);
    vec3 transformedNormal = vec3(0);

    for (int i = 0; i < 4; i++)
    {
        transformedPosition += boneWeights[i] * (bJointMatrices[int(boneIds[i])] * vec4(position, 1.0)).xyz;
        transformedNormal += boneWeights[i] * (mat3(bJointMatrices[int(boneIds[i])]) * normal);
    }

    if (transformedPosition == vec3(0.0) || boneWeights.w > boneWeights.x)
    {
        transformedPosition = position;
        transformedNormal = normal;
    }

    return SkinnedVertex(transformedPosition, transformedNormal);
}
