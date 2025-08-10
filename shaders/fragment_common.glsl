mat3 getTBN(vec3 worldPos, vec3 normal, vec2 texCoords)
{
    vec3 Q1 =   dFdx(worldPos);
    vec3 Q2 =   dFdy(worldPos);
    vec2 st1 =  dFdx(texCoords);
    vec2 st2 =  dFdy(texCoords);

    vec3 N = normalize(normal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return TBN;
}