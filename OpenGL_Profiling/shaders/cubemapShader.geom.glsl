#version 460 

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 uViewProjectMatrices[6];
uniform int uIndex;

out vec4 vFragPos; 

void main()
{
    for(int face = 0; face < 6; ++face)
    {
        gl_Layer = face + (uIndex * 6);
        for(int i = 0; i < 3; ++i) 
        {
            vFragPos = gl_in[i].gl_Position;
            gl_Position = (uViewProjectMatrices[face] * vFragPos);
            EmitVertex();
        }    
        EndPrimitive();
    }
} 