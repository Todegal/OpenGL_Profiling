
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoords;

mat3 getTBN()
{
    vec3 Q1 = dFdx(vWorldPos);
    vec3 Q2 = dFdy(vWorldPos);
    vec2 st1 = dFdx(vTexCoords);
    vec2 st2 = dFdy(vTexCoords);

    vec3 N = normalize(vNormal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return TBN;
}

struct MaterialInput
{
    bool isTextureEnabled;
    sampler2D textureMap;
    vec4 factor;
};

const float shadowBias = 0.0001;

