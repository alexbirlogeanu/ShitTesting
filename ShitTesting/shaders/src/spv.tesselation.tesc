#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(vertices = 3) out;

layout(location=0) in vec4 vs_normal[];
layout(location=1) in vec4 vs_worldPos[];
layout(location=2) in vec4 vs_material[];
layout(location=3) in vec2 vs_uv[];

layout(location=0) out vec4 te_normal[];
layout(location=1) out vec4 te_worldPos[];
layout(location=2) out vec4 te_material[];
layout(location=3) out vec2 te_uv[];

void main()
{
	te_normal[gl_InvocationID] = vs_normal[gl_InvocationID];
	te_worldPos[gl_InvocationID] = vs_worldPos[gl_InvocationID];
	te_material[gl_InvocationID] = vs_material[gl_InvocationID];
	te_uv[gl_InvocationID] = vs_uv[gl_InvocationID];
	
	gl_TessLevelOuter[0] = 1;
	gl_TessLevelOuter[1] = 1;
	gl_TessLevelOuter[2] = 1;
	gl_TessLevelInner[0] = 1;
}