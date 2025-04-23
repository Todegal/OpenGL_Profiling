#version 460 

layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 uViewProjectMatrices[6];
uniform int uIndex;

in VS_OUT
{
    vec2 vTexCoords;
    vec3 vWorldPos;
    vec3 vNormal;
    vec3 vViewPos;
} gs_in[];

out GS_OUT
{
    vec2 vTexCoords;
    vec3 vWorldPos;
    vec3 vNormal;
    vec3 vViewPos;
} gs_out;

out vec3 vWorldPos;
out vec2 vTexCoords;

void main()
{
    for(int face = 0; face < 6; ++face)
    {
        gl_Layer = face + (uIndex * 6);
        for(int i = 0; i < 3; ++i) 
        {
            gs_out.vWorldPos = gl_in[i].gl_Position.xyz;
            gs_out.vTexCoords = gs_in[i].vTexCoords;
            gl_Position = (uViewProjectMatrices[face] * gl_in[i].gl_Position);
            EmitVertex();
        }    
        EndPrimitive();
    }
} 