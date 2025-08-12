#version 460

#include "../uniforms_common.glsl"
#include "../vertex_common.glsl"

#line 5

out VS_OUT
{
	vec3 position;
} vs_out;

void main()
{
	vs_out.position = aPosition;
    gl_Position = (uProjectionMatrix * mat4(mat3(uViewMatrix)) * vec4(aPosition, 1.0)).xyww;
}