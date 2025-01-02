
// Proprietary functions 

const float PI = 3.14159265359;
const float INV_PI = (1.0 / PI);

float D_GGX(float NoH, float roughness)
{
    float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * INV_PI;
}

vec3 F_Schlick(float cosTheta, vec3 F0)
{
    float f = pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    return f + F0 * (1.0 - f);
}

float F_Schlick(float cosTheta, float F0, float F90)
{
    return F0 + (F90 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 F_Schlick(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Burley(float NoV, float NoL, float LoH, float roughness) {
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(NoL, 1.0, f90);
    float viewScatter = F_Schlick(NoV, 1.0, f90);
    return lightScatter * viewScatter * INV_PI;
}

float Fd_Lambert()
{
    return INV_PI;
}
