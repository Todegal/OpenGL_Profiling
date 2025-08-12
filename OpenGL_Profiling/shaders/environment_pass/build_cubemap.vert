#version 460

layout(location = 0) in vec3 aPosition;

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;

out VS_OUT
{
	vec3 worldPosition;
} vs_out;

void main()
{
	vs_out.worldPosition = aPosition;
	gl_Position = uProjectionMatrix * uViewMatrix * vec4(aPosition, 1.0);
}