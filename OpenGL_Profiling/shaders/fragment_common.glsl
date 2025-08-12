mat3 getTBN(vec3 worldPos, vec3 normal, vec2 texCoords)
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( worldPos );
    vec3 dp2 = dFdy( worldPos );
    vec2 duv1 = dFdx( texCoords );
    vec2 duv2 = dFdy( texCoords );

    // solve the linear system
    vec3 dp2perp = cross( dp2, normal );
    vec3 dp1perp = cross( normal, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * invmax, B * invmax, normal );
}