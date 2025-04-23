layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec4 aBoneIds;
layout(location = 4) in vec4 aBoneWeights;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

uniform int uJointsOffset;

layout(binding = 1) buffer bJointsBuffer
{
    mat4 bJoints[];
};

out VS_OUT
{
    vec2 vTexCoords;
    vec3 vWorldPos;
    vec3 vNormal;
    vec3 vViewPos;
} vs_out;

void parseJoints()
{
    vs_out.vWorldPos = vec3(0);
    vs_out.vNormal = vec3(0);

    for (int i = 0; i < 4; i++)
    {
        vs_out.vWorldPos += aBoneWeights[i] * (bJoints[int(aBoneIds[i]) + uJointsOffset] * vec4(aPos, 1.0)).xyz;
        vs_out.vNormal += aBoneWeights[i] * (mat3(bJoints[int(aBoneIds[i]) + uJointsOffset]) * aNormal);
    }

    if (vs_out.vWorldPos == vec3(0.0) || aBoneWeights.w > aBoneWeights.x)
    {
        vs_out.vWorldPos = aPos;
        vs_out.vNormal = aNormal;
    }
}