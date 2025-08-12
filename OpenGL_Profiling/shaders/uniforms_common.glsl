// SSBOs and UBOs common binding, even if not needed having symbols helps with debugging

#define NUM_CASCADES 5

layout (std140) uniform FlagUniforms
{
    uint uFlagsBits;
};

bool uNormalsEnabled            = (uFlagsBits & (1 << 0)) != 0;
bool uOcclusionEnabled          = (uFlagsBits & (1 << 1)) != 0;
bool uShadowsEnabled            = (uFlagsBits & (1 << 2)) != 0;
bool uEnvironmentMapEnabled     = (uFlagsBits & (1 << 3)) != 0;
bool uEmulateSunEnabled 	    = (uFlagsBits & (1 << 4)) != 0;
bool uDeferredPassEnabled 	    = (uFlagsBits & (1 << 5)) != 0;
bool uHDRPassEnabled 	        = (uFlagsBits & (1 << 6)) != 0;
bool uDebugPassEnabled 	        = (uFlagsBits & (1 << 7)) != 0;

layout(std140) uniform FrameUniforms
{
    mat4 uProjectionMatrix;
    mat4 uViewMatrix;
    vec3 uCameraPosition;

    vec4 uDirectionalShadowCascadePlanes;

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

