#version 460 

layout(location = 0) in vec3 aPosition;

out VS_OUT
{
	vec3 worldPosition;
} vs_out;

void main()
{
	vs_out.worldPosition = aPosition;
	gl_Position = vec4(aPosition, 1.0);
}