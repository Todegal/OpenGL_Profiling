#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

layout(std140) uniform MatrixUniforms
{
	mat4 uProjectionMatrix;
	mat4 uViewMatrix;
	mat4 uModelMatrix;
	mat3 uNormalMatrix;	
};

out VS_OUT
{
	vec3 worldPos;
	vec2 texCoords;
	vec3 normal;
} vs_out;

void main() 
{
	vs_out.worldPos = (uModelMatrix * vec4(aPosition, 1.0)).xyz;
	vs_out.texCoords = aTexCoord;
	vs_out.normal = normalize(uNormalMatrix * aNormal);
	gl_Position = uProjectionMatrix * uViewMatrix * vec4(vs_out.worldPos, 1.0);
}
