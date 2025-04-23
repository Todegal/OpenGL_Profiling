#define NUM_CASCADES 5

layout (std140) uniform FlagsBuffer
{
    bool uNormalsEnabled;
    bool uOcclusionEnabled;
    bool uShadowsEnabled;
    bool uEnvironmentMapEnabled;
    bool uEmulateSunEnabled;
    bool uDeferredPassEnabled;
    bool uHDRPassEnabled;
};

layout(std140) uniform FrameUniformsBuffer
{
    mat4 uProjectionMatrix;
    mat4 uViewMatrix;
    vec3 uCameraPosition;

    vec4 uDirectionalShadowCascadePlanes;

    float uPointShadowFarPlane;
    float uPointShadowNearPlane;

    int uNumPointLights;
    int uNumDirectionalLights;
};

struct PointLight
{
    vec4 position; // X Y Z + padding
    vec4 radiance; // R G B + padding
};

layout(std430) buffer PointLightBuffer
{
    PointLight bPointLights[];
};

struct DirectionalLight
{
    vec4 direction; // X Y Z + padding
    vec4 radiance;  // R G B + padding
    mat4 lightSpaceMatrices[NUM_CASCADES];
};

layout(std430) buffer DirectionalLightBuffer
{
    DirectionalLight bDirectionalLights[];
};