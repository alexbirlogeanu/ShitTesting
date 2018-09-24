#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(vertices = 3) out;

layout(location=0) in vec4 vs_normal[];
layout(location=1) in vec4 vs_material[];
layout(location=2) in vec2 vs_uv[];

layout(location=0) out vec4 te_normal[];
layout(location=1) out vec4 te_material[];
layout(location=2) out vec2 te_uv[];

void main()
{
	te_normal[gl_InvocationID] = vs_normal[gl_InvocationID];
	te_material[gl_InvocationID] = vs_material[gl_InvocationID];
	te_uv[gl_InvocationID] = vs_uv[gl_InvocationID];
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position; //we pass the world position that comes from VS
	
	gl_TessLevelOuter[0] = 3;
	gl_TessLevelOuter[1] = 3;
	gl_TessLevelOuter[2] = 3;
	gl_TessLevelInner[0] = 2;
}