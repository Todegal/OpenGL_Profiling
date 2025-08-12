layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec4 aBoneIds;
layout(location = 4) in vec4 aBoneWeights;

struct SkinnedVertex
{
    vec3 position;
    vec3 normal;
};

layout(std430) buffer JointsBuffer
{
    mat4 bJointMatrices[];
};

struct InstanceTransform
{
    mat4 modelMatrix;
    mat3 normalMatrix;
};

layout (std140) buffer InstanceBuffer
{
    InstanceTransform bInstanceTransforms[];
};

uniform int uInstanceOffset;    

SkinnedVertex applySkinning(vec3 position, vec3 normal, vec4 boneIds, vec4 boneWeights)
{
    SkinnedVertex vtx = SkinnedVertex(position, normal);

    if (boneWeights.w < boneWeights.x)
    {
        vtx.position = vec3(0.0);
        vtx.normal = vec3(0.0);

        for (int i = 0; i < 4; i++)
        {
            vtx.position += boneWeights[i] * (bJointMatrices[int(boneIds[i])] * vec4(position, 1.0)).xyz;
            vtx.normal += boneWeights[i] * (mat3(bJointMatrices[int(boneIds[i])]) * normal);
        }
    }

    return vtx;
}
