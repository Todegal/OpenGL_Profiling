#version 460

layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 uLightSpaceMatrices[5];

uniform int uIndex;

void main()
{          
	for (int i = 0; i < 3; ++i)
	{
		gl_Position = uLightSpaceMatrices[gl_InvocationID] * gl_in[i].gl_Position;
		gl_Layer = (uIndex * 5) + gl_InvocationID;
		EmitVertex();
	}
	EndPrimitive();
}  