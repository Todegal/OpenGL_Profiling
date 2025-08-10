
in VS_OUT
{
    vec2 vTexCoords;
    vec3 vWorldPos;
    vec3 vNormal;
    vec3 vViewPos;
} fs_in;

mat3 getTBN()
{
    vec3 Q1 =   dFdx(fs_in.vWorldPos);
    vec3 Q2 =   dFdy(fs_in.vWorldPos);
    vec2 st1 =  dFdx(fs_in.vTexCoords);
    vec2 st2 =  dFdy(fs_in.vTexCoords);

    vec3 N = normalize(fs_in.vNormal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return TBN;
}

float linearizeDepth(float depth, float near, float far)
{
    return (near * far) / (far - depth * (far - near));
}

float projectDepth(float depth, float near, float far)
{
    float invNear = 1.0 / near;
    float invFar = 1.0 / far;
    return ((1.0 / depth) - invNear) / (invFar - invNear);
}

vec2 VogelDiskSample(int sampleIndex, int samplesCount, float phi)
{
    const float GoldenAngle = 2.4; // Define the constant for better practice

    float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(samplesCount));
    float theta = float(sampleIndex) * GoldenAngle + phi;

    return vec2(r * cos(theta), r * sin(theta));
}

// https://www.shadertoy.com/view/ftKfzc
float InterleavedGradientNoise(vec2 uv) {
	// magic values are found by experimentation
	uv += (vec2(47, 17) * 0.695);

    vec3 magic = vec3( 0.06711056, 0.00583715, 52.9829189 );
    
    //https://juejin.cn/post/6844903687505068045
    //vec3 magic = vec3( 12.9898, 78.233, 43758.5453123 );
    
    return fract(magic.z * fract(dot(uv, magic.xy)));
}

struct MaterialInput
{
    bool isTextureEnabled;
    sampler2D textureMap;
    vec4 factor;
};

const float shadowBias = 0.00001;

