#version 460 

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 uViewProjectMatrices[6];

in VS_OUT
{
    vec3 worldPosition;
} gs_in[];

out GS_OUT
{
    vec3 worldPosition;
} gs_out;

void main()
{
    for(int face = 0; face < 6; ++face)
    {
        gl_Layer = face;
        for(int i = 0; i < 3; ++i) 
        {
            gs_out.worldPosition = gl_in[i].gl_Position.xyz;
            gl_Position = (uViewProjectMatrices[face] * gl_in[i].gl_Position);
            EmitVertex();
        }    
        EndPrimitive();
    }
} 